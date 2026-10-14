"""Complete and verify the independent CD Player folder ZIP after its native build."""
from pathlib import Path
import hashlib
import json
import shutil
import stat
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TITLE = 'PPSA98427'
APP = ROOT / 'dist' / TITLE

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

param = json.loads((APP / 'sce_sys/param.json').read_text(encoding='utf-8'))
assert param == json.loads((ROOT / 'sce_sys/param.json').read_text(encoding='utf-8')), 'Rebuild/copy the current application metadata first'
assert param['titleId'] == TITLE and param['conceptId'] == TITLE[4:]
assert param['contentId'] == 'UP9000-PPSA98427_00-CDPLAYER00000001'
assert param['applicationCategoryType'] == 65536 and param['contentBadgeType'] == 2
assert 'gameIntent' not in param
assert 'amm' not in param and 'kernel' not in param
assert (param['attribute'], param['attribute2'], param['attribute3']) == (0, 0, 0)
assert all(param['localizedParameters'][lang]['titleName'] == 'CD Player' for lang in ('it-IT', 'en-US'))
manifest = json.loads((ROOT / 'build/helper/lapy-manifest.json').read_text(encoding='utf-8'))
assert manifest['target_title'] == TITLE and manifest['max_requests'] == 1
assert manifest['service'] is False
assert sha(ROOT / 'build/helper/lapy.elf') == manifest['elf_sha256']
assert manifest['protocol_sha256'] == 'bb02c4aa814eaba7a7a423a31b29ff41f786212c2953678cf29434e85fa0f869'
assert sha(ROOT / '.deps/lapy/PS5-Lapy-JB-Daemon-57fcc19/source/lapy_elevation_protocol.h') == manifest['protocol_sha256']
for name in ('lapy.elf', 'lapy-manifest.json'):
    shutil.copy2(ROOT / 'build/helper' / name, APP / name)
shutil.copytree(ROOT / 'licenses', APP / 'licenses', dirs_exist_ok=True)
shutil.copy2(ROOT / 'THIRD_PARTY_NOTICES.md', APP / 'THIRD_PARTY_NOTICES.md')
shutil.copy2(ROOT / 'README.txt', APP / 'README.txt')
# The demo font collection is not needed by CD Player; retain Doto and its fallback.
for name in ('dejavu-sans-mono.huifont', 'montserrat-medium.huifont', 'patrick-hand.huifont', 'press-start-2p.huifont',
             'DejaVu-LICENSE.txt', 'Montserrat-LICENSE.txt', 'PatrickHand-LICENSE.txt', 'PressStart2P-LICENSE.txt'):
    (APP / 'assets/fonts' / name).unlink(missing_ok=True)
required = ['eboot.bin', 'sce_module/libc.prx', 'sce_sys/param.json', 'sce_sys/icon0.png', 'sce_sys/pic0.dds', 'sce_sys/pic1.dds',
            'lapy.elf', 'lapy-manifest.json', 'assets/fonts/doto-semibold.huifont', 'assets/fonts/inter-regular.huifont',
            'assets/fonts/inter-semibold.huifont', 'assets/fonts/Doto-OFL.txt', 'licenses/PSSwanStation-ADDITIONAL-TERMS.txt']
required += ['assets/disc/engraving.png', 'licenses/curl-LICENSE.txt', 'licenses/OpenSSL-LICENSE.txt',
             'licenses/libpsl-LICENSE.txt', 'licenses/zstd-LICENSE.txt']
for name in required:
    assert (APP / name).is_file() and (APP / name).stat().st_size > 0, name
checksums = {p.relative_to(APP).as_posix(): sha(p) for p in sorted(APP.rglob('*')) if p.is_file() and p.name != 'SHA256SUMS.txt'}
(APP / 'SHA256SUMS.txt').write_text(''.join(f'{digest}  {name}\n' for name, digest in checksums.items()), encoding='utf-8')
archive = ROOT / 'dist' / (TITLE + '.zip')
temporary = archive.with_suffix('.zip.tmp')
with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for path in [APP, *sorted(APP.rglob('*'))]:
        if path.is_symlink():
            raise ValueError('Unexpected package symlink: ' + str(path))
        name = path.relative_to(APP.parent).as_posix() + ('/' if path.is_dir() else '')
        info = zipfile.ZipInfo(name)
        info.create_system = 3
        info.external_attr = ((stat.S_IFDIR if path.is_dir() else stat.S_IFREG) | 0o777) << 16
        if path.is_dir():
            info.external_attr |= 0x10
        info.compress_type = zipfile.ZIP_DEFLATED
        z.writestr(info, b'' if path.is_dir() else path.read_bytes())
with zipfile.ZipFile(temporary) as z:
    assert z.testzip() is None
    expected = {TITLE + '/' + p.relative_to(APP).as_posix() for p in APP.rglob('*') if p.is_file()}
    assert {i.filename for i in z.infolist() if not i.is_dir()} == expected
    for item in z.infolist():
        assert item.filename.startswith(TITLE + '/') and (item.external_attr >> 16) & 0o777 == 0o777
        if not item.is_dir():
            assert hashlib.sha256(z.read(item)).hexdigest() == sha(APP.parent / item.filename)
temporary.replace(archive)
report = {'title_id': TITLE, 'category': 'Media', 'package': str(archive), 'bytes': archive.stat().st_size,
          'sha256': sha(archive), 'file_count': len(expected), 'zip_crc_and_contents_verified': True,
          'helper_title_and_protocol_verified': True, 'console_validation': 'not performed by this packaging check'}
(ROOT / 'build/package-validation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
