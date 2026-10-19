# Changes

## 1.1.1

- Keep the music playing when the controller turns off, disconnects, or the system temporarily takes controller input. Play/Pause still works as usual when the controller is connected.

## 1.1.0

- Bring the playing track into view at each track change, while leaving manual browsing free during playback and pause.
- Hide the Eject hint when the Eject button is hidden, and restore both when enabled.
- Add a saved OLED mode: hide fullscreen labels after two seconds, slowly shift the effect, and show a black screen after one idle minute with playback stopped. Any input brings the picture back.
- Follow display-frame timing in imported visualizers instead of the gallery's preview rate and audio-dependent clock.
- Compile the visualizer frame function once and reuse audio buffers and drawing-command storage.
- Render terminal symbols with a monospaced font including box-drawing glyphs, and respect Canvas text baselines.

## 1.0.0

First public release.
