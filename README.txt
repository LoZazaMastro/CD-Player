CD Player - PPSA98427
====================

A native PS5 audio CD player with a minimal digital interface.
CD Player appears in the Media section of the PS5 home screen.
Its title ID is independent of other homebrew applications.

INSTALLATION
Import PPSA98427.zip using your usual homebrew installation method.
The ZIP contains one PPSA98427 folder. Copy it into /data/homebrew so that
eboot.bin is at /data/homebrew/PPSA98427/eboot.bin, without another nested folder.
Update the application only while CD Player is closed.
The package preserves the open permissions required by the homebrew loader.

USB CD DRIVES AND APPLE SUPERDRIVE
CD Player reads audio CDs from USB optical drives through FreeBSD CAM.
Apple SuperDrive initialization and software disc ejection are included.
The Eject button is provided especially for Apple USB SuperDrive, which has
no physical eject button. Press Triangle or select the Eject icon to remove
the disc. The same command remains available for other compatible USB drives.
No Apple Boot Camp driver binaries are included.

Start the console with the USB drive empty, then insert an audio CD.
PSSwanStation documents drive lockups during console boot with a disc inserted.
An active compatible PS5 homebrew environment is required. The included
one-shot access helper is restricted to PPSA98427. A local ELF loader on
port 9021 may be needed by the helper. Firmware compatibility is separate
from successful compilation.

CONTROLS
D-pad: navigate. Cross: select. Square: play/pause. Triangle: eject.
L1/R1: previous/next track. L2/R2: seek backward/forward by 10 seconds.
Touchpad: fullscreen visualizer. Options: settings. Circle: back.
Hold Options + Circle to close the application.
A red Play icon replaces the number of the track currently playing.
The six icon buttons offer previous track, play/pause, next track,
visualizer, settings and eject. The Show Eject button toggle hides the Eject
icon and lets the other five buttons fill the same total width without
resizing their icons. The Triangle eject shortcut remains available.

Select the album cover to open album notes when a description is available.
Up/down scroll the cover, headings and description together with smooth motion.
Back stays fixed in the footer. Reduce motion disables
the scrolling animation as well as other optional interface motion.

VISUALIZERS
The catalog contains 152 original iwrzwr drawing studies and nine individual
studies derived from its compositions. One visualizer appears at a time.
Left/right changes the effect in fullscreen; settings also offer a selector.
Shuffle visualizers chooses another effect when the audible track changes,
without repeating the current effect. The choice and toggle are saved.
The effect name and controller hints fade together after two seconds idle.
Original drawing programs run locally in QuickJS and are rendered through
Prospero Homebrew UI/OpenGL. No browser is required.

AUDIO AND SETTINGS
Audio CDs provide stereo PCM at 44.1 kHz, resampled to 48 kHz for AudioOut.
The ten-band equalizer, volume, gapless playback and optional 7.1 upmix can
be adjusted during playback. Upmix derives surround sound from stereo; it
does not turn the CD into an original multichannel recording. The upmix
control is unavailable when AudioOut only accepts stereo output.

Autoplay disc starts the first audio track after a disc is detected. It is off
by default and can be enabled in settings. Changing the toggle does not start
a disc that is already present.

Equalizer bands and enable state, upmix, gapless playback, autoplay, volume, reduce
motion, selected visualizer, shuffle and Eject button visibility are saved
automatically.
Changes are checked for saving once per second and restored at the next launch.
Application updates preserve these settings outside the installation folder.

ALBUM METADATA
Internet access is needed for online album information and covers.
MusicBrainz identifies the disc and its editions; Cover Art Archive supplies
artwork. CD-Text offers local titles when present. Album notes use MusicBrainz
annotations or the English Wikipedia page linked through MusicBrainz/Wikidata.
The cover is selectable only when a real description is available.
Online responses and artwork are cached. Source attribution is retained in
the cache metadata and project documentation.
No recordings or cover-art files from the test CDs are bundled in the app.

VIDEO AND LOGS
The tested Media configuration runs at approximately 60 Hz on the console.
The renderer requests a higher rate, but a 120 Hz request is not a guarantee
of 120 fps. The session log records the accepted mode and measured frame rate.
Settings, caches and session.log are kept at /data/cdplayer-PPSA98427.
Without access to /data, the fallback is /download0/cdplayer in the app sandbox.
The startup log is at:
/mnt/sandbox/PPSA98427_000/download0/hui/dev/app.log

VALIDATION
Apple SuperDrive playback, online artwork and album descriptions have been
confirmed on the owner's PS5 running firmware 13.60. This is not a claim of
compatibility with every firmware, USB drive or surround audio setup.
Host checks cover CD commands and parsing, metadata, DSP, resampling,
gapless playback, ejection, animation and saved-preference restoration.
SHA256SUMS.txt lists the files in this package.

CREDITS
Based on PSSwanStation by Press5elect.
Prospero Homebrew UI and PS5 Native App Boilerplate: BlackBearReloaded.
Graphics: ps5-opengl, Mesa and OpenGNM PSBC.
Visualizers: iwrzwr Visual Archive by Kagan Yaldizkaya.
Doto: Oliver Lalan. Inter fallback font: Rasmus Andersson.
SuperDrive initialization protocol: Alfredo Sandoval.
Metadata: MusicBrainz, Cover Art Archive, Wikidata and Wikipedia contributors.
See THIRD_PARTY_NOTICES.md and licenses/ for all component licenses.
