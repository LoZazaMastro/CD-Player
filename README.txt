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
L3 on the album page: turn the case over; press again to return to its front.
Right stick up/down: adjust the master volume from any screen.
Hold Options + Circle to close the application.
A Play icon in the accent color replaces the number of the track currently playing.
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

Speaker setup opens within the settings drawer and controls all eight speakers
independently: front left/right, center, surround left/right, side left/right,
and subwoofer (LFE). Each ranges from mute to +24 dB. 0 dB keeps its original
level; fronts stay unchanged unless their own controls are adjusted.
Previous upmix gains carry over to the individual speakers, including mute.
Front controls also work in stereo; additional speakers require 7.1 upmix.
Gain changes are smoothed, with peak protection on generated or boosted channels.

LYRICS
Press R3 in the fullscreen visualizer to switch to lyrics, or back again.
Timed lyrics highlight the current phrase and scroll it smoothly to the
center, following playback, pause and seeking. When only untimed lyrics are
available, use the D-pad to scroll. Availability depends on the track.
Lyrics are provided by LRCLIB and cached after the first online lookup.
Temporary server errors trigger automatic retries while the loading message
stays visible. Leaving lyrics or changing tracks cancels further retries.
No song lyrics or recordings are bundled with the app.
Returning to the current track's loaded lyrics opens them without a loading flash.
Circle returns directly to the album page from either fullscreen mode.
Reopening fullscreen restores the last lyrics or visualizer choice.

Autoplay disc starts the first audio track after a disc is detected. It is off
by default and can be enabled in settings. Changing the toggle does not start
a disc that is already present.

Equalizer bands and enable state, upmix and its channel gains, gapless playback, autoplay, volume, reduce
motion, selected visualizer, shuffle, Eject button visibility and accent color are saved
automatically.
Changes are checked for saving once per second and restored at the next launch.
Application updates preserve these settings outside the installation folder.
Accent color offers 40 named colors with a swatch. Changes apply immediately.
Visualizer colors follow the same accent. Right-stick volume changes leave
open settings and album panels in place.

ALBUM METADATA
Internet access is needed for online album information and covers.
MusicBrainz identifies the disc and its editions; Cover Art Archive supplies
artwork. CD-Text offers local titles when present. Album notes use MusicBrainz
annotations or the English Wikipedia page linked through MusicBrainz/Wikidata.
The cover is selectable only when a real description is available.
Online responses and artwork are cached. Source attribution is retained in
the cache metadata and project documentation.
No recordings or cover-art files from the test CDs are bundled in the app.
Printed disc labels, back covers and spines are downloaded for the selected
edition when available. Each image fades in independently. Missing images
leave that surface blank. A combined back/spine scan is split into its
central inlay and a narrow folded edge for the three-dimensional case.

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

VERSION 1.1.0
The track list follows the audible track only when it changes; manual browsing
remains free. The Eject hint follows the Show Eject button setting.
OLED mode hides fullscreen text after two seconds and slowly shifts effects.
After one idle minute with playback stopped it shows a black screen; any input
restores the picture. OLED mode is remembered between launches.
Visualizer timing follows the display, with a compiled frame function and
reusable buffers. Terminal effects include monospaced box-drawing characters.

VERSION 1.1.1
Music keeps playing when the controller turns off or disconnects, including
when the system temporarily takes controller input. Play/Pause remains manual.
Speaker setup provides saved, independent gain controls for all eight outputs.
R3 switches between the fullscreen visualizer and lyrics. Timed phrases scroll
with playback when available; untimed lyrics can be scrolled manually.
