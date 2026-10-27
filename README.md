# ArtFlow

A beginner-friendly drawing and animation app for Windows, with three
modes over one engine:

* **Flip**: frame-by-frame animation. Draw a frame, add the next one,
  flip through them with onion skin and frame thumbnails.
* **Motion**: tracks and keyframes. Move, zoom and turn the camera over
  time, with every layer as a track.
* **Studio**: drawing and painting with layers, brushes and colours.

## What is inside

* **A smooth brush engine** with 16 styles. Pencil tool: Pencil, Soft
  Pencil, Clean Line, Charcoal and Crayon. Brush tool: Ink Pen, Fine Liner,
  Brush Pen, Calligraphy, Round Brush, Soft Brush, Marker, Flat Paint,
  Watercolor, Airbrush and Spray Can. Strokes follow a curve, pressure
  blends along it, edges are smooth and paper grain stays on the page. Each
  style remembers its own size.
* **Tools:** Brush, Pencil, Eraser, Smudge, Fill, Lasso Fill, Picker,
  Select, Move, Shapes (line, rectangle, ellipse, filled or not), Text and
  Camera.
* **Symmetry** drawing and **Adjust** (hue, saturation, brightness,
  contrast, blur, invert, black & white).
* **Layers** in every mode: thumbnails, rename, drag to reorder,
  duplicate, hide and layer opacity.
* **Pen side buttons** (made for the ASUS Pen 2.0): upper = Redo,
  lower = Undo, remappable with a "press each button" step.
* **Few, familiar keys:** B brush, N pencil, E eraser, G fill, L lasso
  fill, I picker, M select, V move, U shapes, T text, C camera, [ ] brush
  size, Ctrl+Z / Ctrl+Y, Space+drag to pan, the wheel to zoom, F1 or ? for
  the full list.

## Running it

Download the zip from the latest release, unpack it anywhere and start
`artflow.exe`. Nothing is installed; settings live under
`HKEY_CURRENT_USER\Software\ArtFlow`.

## Building

See [docs/build.md](docs/build.md) (Qt 6.8 with llvm-mingw, CMake and
Ninja) and [docs/architecture.md](docs/architecture.md) for how the
pieces fit. `ARTFLOW_SELFTEST=<folder>` runs the built-in self-test.

## Credits and licence

ArtFlow is built on [Pencil2D](https://www.pencil2d.org/) (its original
README is in [docs/pencil2d-README.md](docs/pencil2d-README.md)) and on
Qt. Pencil2D is GPL-2.0-only; ArtFlow's own files (`shell/`,
`studiocore/`) are GPL-2.0-or-later, so the whole program is distributed
under the GPL version 2. See [LICENSE.TXT](LICENSE.TXT) and
[LICENSE.QT.TXT](LICENSE.QT.TXT).

This is a private project.
