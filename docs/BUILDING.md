# Build and validation

Build on Ubuntu 24.04 or a matching WSL distribution. The release is a native
PS5 app, not a Windows executable running under a compatibility layer.

## Host setup

Install Git, Make, Python 3, curl, unzip, tar, pkg-config, and a C/C++ build
environment. The local bootstrap extracts the Clang 18 and graphics packages
under `build/host-sysroot` rather than installing them globally:

```sh
bash tools/setup-host.sh
bash tools/linker-deps.sh
bash tools/build-cdplayer.sh
```

The release folder is `dist/PPSA98427`; the importable archive is
`dist/PPSA98427.zip`. The build checks the SELF container, independent Media
identity, helper target, archive contents, file permissions and SHA-256 values.
`build/package-validation.json` records those checks.

Native build dependencies are downloaded into ignored `.deps/` with pinned
versions and hashes. They include PS5 Payload SDK v0.42, the ps5-opengl SDK,
zlib, and PacBrew v0.40.2 libraries. The helper is rebuilt from its pinned
upstream commit. `runtime/libc.prx` is the project-generated compatibility shim;
its source and byte-identical rebuild check are in `tooling/native/` and
`tools/rebuild-libc.sh`.

## Host checks

```sh
source tools/host-env.sh
make test
```

The checks cover malformed TOC/CD-Text input, CD commands, metadata parsing,
resampling and DSP, gapless boundaries, pause/eject/reinsertion, autoplay,
saved-preference round trips, and animation behavior. Tests use synthetic
settings and public metadata fixtures, not a user's logs or saved configuration.
AddressSanitizer and UndefinedBehaviorSanitizer are enabled where appropriate.

The audio checks compare the front pair sample by sample while changing the
generated surround and LFE gains, and test their independence, mute and peak
behavior. Lyrics checks use self-authored timing fixtures to exercise LRC
parsing, metadata/duration matching, malformed responses and playback-position
lookup. The UI check covers R3 switching, smooth phrase following and seeks.
These host checks do not establish network availability or perceived surround
levels on a particular PS5 and audio system.

For UI snapshots, fetch the public album metadata and artwork into the ignored
preview cache. No album artwork or audio recording is part of the application
assets:

```sh
python3 tools/prepare-preview.py
CD_PLAYER_RELEASE_GALLERY=1 bash tools/preview.sh
bash tools/test-archive.sh
bash tools/test-screen.sh
```

Preview frames use the same UI code and OpenGL shaders as the PS5 app, rendered
on the host. A synthetic audio signal is used if no local test recording is
available. The archive check exercises all 152 imported drawing programs; nine
additional native composition studies complete the 161-item catalog. The screen
check covers track-transition following without taking over manual browsing,
and OLED label hiding, motion, blanking and wake-up. Archive reports separate
initial loading from steady frame times and check redraw cadence and missing glyphs.

## What has been checked on a console

The owner confirmed startup, SuperDrive playback, high-level listening controls,
artwork, album notes, and the interface on firmware 13.60. The published executable and runtime match the last verified FTP transfer.
The package documentation was updated for publication. Host tests do not establish
compatibility with every drive, firmware or HDMI audio setup.

The session log reports actual display mode, frame rate, audio underruns and
network requests. The tested Media configuration uses approximately 60 Hz.

## Data and privileges

The exact-title one-shot helper grants filesystem access for this title only.
It is not a resident service. Its target and provenance are recorded in the
packaged `lapy-manifest.json`.

CD data is read from external USB optical drives using CAM/SCSI. The internal
console drive is excluded. Settings and caches live at `/data/cdplayer-PPSA98427`,
outside the app folder, with a sandbox fallback when unavailable. Network requests
are bounded and HTTPS-only; certificate and hostname validation stay enabled.
