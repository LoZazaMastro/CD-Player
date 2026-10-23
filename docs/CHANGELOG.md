# Changes

## 1.1.2

Fixes the oversized storage reservation that made CD Player occupy about 18 GB
on PS5. The app now reserves 256 MiB for its data instead of 16 GiB.

Tested on PS5 firmware 13.60. To reclaim space from an older installation,
remove CD Player through the PS5's storage settings and install this release
again. Replacing the app files alone does not shrink the existing allocation.
Reinstallation may reset settings and cached artwork.

## 1.1.1

More ways to enjoy your music, with lyrics, finer sound controls and artwork that makes each CD feel like your own.

### Added

- Lyrics in fullscreen. Press R3 to switch between lyrics and the visualizer. Timed phrases follow the music and scroll into the centre of the screen when available.
- Speaker setup, with separate levels for all eight outputs: front left and right, centre, surround left and right, side left and right, and subwoofer. The front pair keeps its original level unless you adjust it.
- Master volume control with the right stick, from any screen.
- Forty named accent colours, applied instantly to the interface and visualizers and remembered between launches.
- Printed CD artwork, back covers and spines for editions that have them online. Each image fades in when ready; missing artwork stays blank.
- A reflective 3D album case. Press L3 to turn it over and see the back, or press it again to return to the front.

### Fixed

- Music keeps playing when the DualSense turns off or disconnects.
- Changing the volume leaves open settings and album panels in place.
- Lyrics retry temporary server errors automatically. Reopening lyrics that are already loaded no longer flashes a loading message.
- Circle returns directly from lyrics to the album page. Reopening fullscreen restores your last lyrics or visualizer choice.
- Two-digit speaker levels remain fully readable.
- CD artwork fits the printed circle more closely, without adding the scan's margins to the edge of the 3D disc.

### Updating

Close CD Player before replacing the PPSA98427 folder with the one in the release ZIP. Your saved settings and artwork cache are kept separately and carry over.

## 1.1.0

- Bring the playing track into view at each track change, while leaving manual browsing free during playback and pause.
- Hide the Eject hint when the Eject button is hidden, and restore both when enabled.
- Add a saved OLED mode: hide fullscreen labels after two seconds, slowly shift the effect, and show a black screen after one idle minute with playback stopped. Any input brings the picture back.
- Follow display-frame timing in imported visualizers instead of the gallery's preview rate and audio-dependent clock.
- Compile the visualizer frame function once and reuse audio buffers and drawing-command storage.
- Render terminal symbols with a monospaced font including box-drawing glyphs, and respect Canvas text baselines.

## 1.0.0

First public release.
