# Changes

## 1.1.1

- Keep the music playing when the controller turns off, disconnects, or the system temporarily takes controller input. Play/Pause still works as usual when the controller is connected.
- Balance all eight outputs independently in Speaker setup, within the settings drawer. Saved upmix gains carry over; fronts remain at their original level unless their own controls are adjusted. Two-digit gain labels remain fully visible.
- Press R3 in the fullscreen visualizer to switch to lyrics. Timed phrases follow playback with smooth scrolling; untimed lyrics can be scrolled manually.
- Retry temporary lyrics-service errors automatically while keeping the loading screen visible.
- Reopen already loaded lyrics without a loading flash.
- Adjust master volume with the right stick from any screen, with a brief level display.
- Pick one of 40 named accent colors in settings. Changes apply immediately and are saved.

## 1.1.0

- Bring the playing track into view at each track change, while leaving manual browsing free during playback and pause.
- Hide the Eject hint when the Eject button is hidden, and restore both when enabled.
- Add a saved OLED mode: hide fullscreen labels after two seconds, slowly shift the effect, and show a black screen after one idle minute with playback stopped. Any input brings the picture back.
- Follow display-frame timing in imported visualizers instead of the gallery's preview rate and audio-dependent clock.
- Compile the visualizer frame function once and reuse audio buffers and drawing-command storage.
- Render terminal symbols with a monospaced font including box-drawing glyphs, and respect Canvas text baselines.

## 1.0.0

First public release.
