// CD Player - USB optical drive through FreeBSD CAM, adapted from PSSwanStation.
// SPDX-FileCopyrightText: 2026 Press5elect
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cd/disc.hpp"
#include "platform/ps5/system.hpp"
#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_pass.h>
#include <sys/cdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace cd
{
namespace
{
class CamDrive final : public Drive
{
    int fd_ = -1, pass_ = -1;
    path_id_t path_ = 0;
    target_id_t target_ = 0;
    lun_id_t lun_ = 0;
    std::vector<uint8_t> bounce_;
    std::string label_;

  public:
    CamDrive(int fd, int pass, const ccb_hdr &header, std::string name)
        : fd_(fd), pass_(pass), path_(header.path_id), target_(header.target_id),
          lun_(header.target_lun), label_(std::move(name))
    {
    }
    ~CamDrive() override
    {
        ::close(pass_);
        ::close(fd_);
    }
    std::string label() const override
    {
        return label_;
    }
    Reply execute(const Command &cmd, std::span<uint8_t> out) override
    {
        Reply reply;
        if (cmd.length > 16 || cmd.length == 0 || cmd.allocation > out.size())
        {
            reply.error = "Invalid SCSI command";
            return reply;
        }
        uint8_t *buffer = nullptr;
        if (cmd.allocation)
        {
            const size_t bytes = ((size_t(cmd.allocation) + 4095) & ~size_t(4095)) + 4096;
            bounce_.resize(bytes);
            buffer = reinterpret_cast<uint8_t *>(
                (reinterpret_cast<uintptr_t>(bounce_.data()) + 4095) & ~uintptr_t(4095));
            std::memset(buffer, 0, cmd.allocation);
        }
        union ccb ccb
        {
        };
        ccb.ccb_h.path_id = path_;
        ccb.ccb_h.target_id = target_;
        ccb.ccb_h.target_lun = lun_;
        cam_fill_csio(&ccb.csio, 0, nullptr, cmd.allocation ? CAM_DIR_IN : CAM_DIR_NONE,
                      MSG_SIMPLE_Q_TAG, buffer, cmd.allocation, SSD_FULL_SIZE, cmd.length, 4000);
        std::memcpy(ccb.csio.cdb_io.cdb_bytes, cmd.bytes.data(), cmd.length);
        ccb.ccb_h.flags |= CAM_DEV_QFRZDIS;
        const int result = ioctl(pass_, CAMIOCOMMAND, &ccb);
        reply.system_error = result ? errno : 0;
        const auto *sense = reinterpret_cast<const uint8_t *>(&ccb.csio.sense_data);
        if ((sense[0] & 0x7f) == 0x70 || (sense[0] & 0x7f) == 0x71)
        {
            reply.sense_key = sense[2] & 15;
            reply.asc = sense[12];
            reply.ascq = sense[13];
        }
        else if ((sense[0] & 0x7f) == 0x72 || (sense[0] & 0x7f) == 0x73)
        {
            reply.sense_key = sense[1] & 15;
            reply.asc = sense[2];
            reply.ascq = sense[3];
        }
        const bool residual_ok = ccb.csio.resid >= 0 && uint32_t(ccb.csio.resid) <= cmd.allocation;
        reply.transferred = residual_ok ? cmd.allocation - uint32_t(ccb.csio.resid) : 0;
        reply.ok = result == 0 && (ccb.ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP &&
                   residual_ok && (cmd.short_reply || reply.transferred == cmd.allocation);
        if (reply.ok && reply.transferred)
            std::memcpy(out.data(), buffer, reply.transferred);
        if (!reply.ok)
        {
            char reason[180];
            std::snprintf(reason, sizeof(reason),
                          "SCSI %02X: %s; CAM %X; sense %X/%02X/%02X; bytes %u/%u", cmd.bytes[0],
                          result ? std::strerror(errno) : "drive response",
                          unsigned(ccb.ccb_h.status & CAM_STATUS_MASK), reply.sense_key, reply.asc,
                          reply.ascq, reply.transferred, cmd.allocation);
            reply.error = reason;
        }
        return reply;
    }
};
} // namespace
std::vector<std::string> drive_names()
{
    std::vector<std::string> names;
    // cd0 is accepted only when CAM positively identifies a USB bus below.
    for (unsigned i = 0; i < 16; ++i)
    {
        std::string path = "/dev/cd" + std::to_string(i);
        struct stat st
        {
        };
        if (stat(path.c_str(), &st) == 0)
            names.push_back(std::move(path));
    }
    return names;
}
std::unique_ptr<Drive> open_drive(const std::string &name, std::string &error)
{
    const auto names = drive_names();
    if (std::find(names.begin(), names.end(), name) == names.end())
    {
        error = "USB drive unavailable";
        return nullptr;
    }
    const int fd = ::open(name.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0)
    {
        error = std::strerror(errno);
        return nullptr;
    }
    union ccb ccb
    {
    };
    ccb.ccb_h.func_code = XPT_GDEVLIST;
    if (ioctl(fd, CAMGETPASSTHRU, &ccb) || ccb.cgdl.status == CAM_GDEVLIST_ERROR ||
        !ccb.cgdl.periph_name[0])
    {
        error = "SCSI access to the drive is unavailable";
        ::close(fd);
        return nullptr;
    }
    const std::string pass_name =
        "/dev/" + std::string(ccb.cgdl.periph_name) + std::to_string(ccb.cgdl.unit_number);
    const int pass = ::open(pass_name.c_str(), O_RDWR);
    if (pass < 0)
    {
        error = pass_name + ": " + std::strerror(errno);
        ::close(fd);
        return nullptr;
    }
    ccb = {};
    ccb.ccb_h.func_code = XPT_GDEVLIST;
    if (ioctl(pass, CAMGETPASSTHRU, &ccb))
    {
        error = "Unable to identify the USB bus";
        ::close(pass);
        ::close(fd);
        return nullptr;
    }
    const ccb_hdr address = ccb.ccb_h;
    if (name == "/dev/cd0")
    {
        union ccb path
        {
        };
        path.ccb_h = address;
        path.ccb_h.func_code = XPT_PATH_INQ;
        const int result = ioctl(pass, CAMIOCOMMAND, &path);
        const std::string sim(path.cpi.dev_name,
                              strnlen(path.cpi.dev_name, sizeof(path.cpi.dev_name)));
        if (result || (path.ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP ||
            sim.find("umass") == std::string::npos)
        {
            error = "The internal console drive is not used";
            ::close(pass);
            ::close(fd);
            return nullptr;
        }
    }
    return std::make_unique<CamDrive>(fd, pass, address, name);
}
} // namespace cd
