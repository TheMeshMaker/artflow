# ArtFlow architecture (Phase 0–1, Pencil2D base)

ArtFlow is a **private** project: a beginner drawing and animation app
with three modes (Flip, Motion, Studio) over one engine. It is a fork of
Pencil2D (GPL-2.0-only); ArtFlow's own files are GPL-2.0-or-later so the
whole stays GPL-2.0. Nothing is published. Licence files and copyright
notices stay intact. UX ideas from Flipaclip and Alight Motion are used,
never their code, assets or names, and the app carries no Pencil2D or
Krita branding in its own chrome.

The first spike used the Krita 5.2 engine (see
`../artflow-krita-spike/docs/architecture.md`); the user then chose a
lighter base. The engine-independent parts were carried over.

## Module graph

```
shell/   (artflow.exe, artflow-console.exe)
  main.cpp            Qt app, ArtFlow settings/defaults, MainWindow2 + shell
  ArtflowShell        top bar (modes, tools, undo/redo, pen, ?), docks,
                      hides the engine's menus/toolbars, docks engine panels
  ModeManager         ModeDescriptor registry; remembers each mode's tool
  ArtflowModes.cpp    Flip / Motion / Studio descriptors
  BeginnerShortcuts   key map written into the engine's [shortcuts] settings, "?" sheet
  PenButtonsDialog    actions per side button + "press each button" detection
  ArtflowSelfTest     ARTFLOW_SELFTEST: synthetic input, report + screenshots
  panels/             FrameStripPanel (Flip), TracksPanel (Motion),
                      LayerStackPanel (Studio), BrushSlidersPanel, TransportBar,
                      BrushLibrary (the brush picker)
      |
studiocore/   facade over the engine (compiled into the same program)
  StudioCore          owns the facades; wraps MainWindow2::mEditor + ActionCommands
  StudioHistory       UndoRedoManager::createUndoAction/createRedoAction
  StudioTools         ToolIntent -> ToolManager::setCurrentTool(ToolType); width via
                      StrokeTool::setWidth, opacity via ColorManager front colour alpha
  StudioLayers        LayerManager (top-first list, add without prompt, visibility,
                      onion skin = PREV_ONION/NEXT_ONION preferences)
  StudioTimeline      Editor::scrubTo, ActionCommands::addNewKey/duplicateKey/removeKey
  StudioTimelineModel QAbstractTableModel, rows = layers, column c = frame c+1
  StudioPlayback      PlaybackManager play/stop, ActionCommands goto next/prev
  StudioPenButtons    native WM_POINTER filter + application event filter
  StudioBrushes       brush library (16 styles), style per tool, sizes per style;
                      installed as the core's BitmapBrushEngine
  BrushStroke         renders one stroke: curve, dabs, coverage mask, grain
      |
app/       Pencil2D's MainWindow2, docks (ColorWheel, ColorPalette, ToolOption,
           TimeLine...), dialogs, ActionCommands (reused as is)
core_lib/  Pencil2D engine: Editor, managers, ScribbleArea canvas, tools, layers
```

## Verified engine names (Pencil2D master 408563a3)

* `MainWindow2::mEditor` (public), `Editor::getScribbleArea()`,
  `Editor::objectLoaded`, `scrubbed(int)`, `frameModified`, `updateTimeLine`.
* Tools: `ToolType` `BRUSH, PENCIL, ERASER, SMUDGE, BUCKET, EYEDROPPER,
  SELECT, MOVE, POLYLINE, CAMERA, PEN, HAND`; `ToolManager::toolChanged`;
  `StrokeTool::strokeToolProperties().width()`, `widthChanged`.
* Layers: `LayerManager::getLayer(i)` (index 0 = bottom), `createBitmapLayer`,
  `nameSuggestLayer`, `currentLayerChanged`, `layerCountChanged`;
  `Layer::keyExists`, `foreachKeyFrame`, `getKeyFrameWhichCovers`.
* Undo: `UndoRedoManager`, legacy backups vs the newer system
  (`SETTING::NEW_UNDO_REDO_SYSTEM_ON`, settings key `NewUndoRedoOn`).
* Keys: settings group `shortcuts`, commands such as `CmdUndo`,
  `CmdToolBrush`; defaults in `:resources/kb.ini`.
* Docks (object names): `TimeLine`, `ColorWheel`, `Color Inspector`,
  `ColorPalette`, `Onion Skin`, `ToolOption`, `ToolBox`.

## Decisions

* **Modes change chrome only.** The Editor, the object and the
  ScribbleArea are never recreated. Verified: same object, canvas, pixel
  hash and undo state after Flip > Motion > Studio > Flip.
* **Reuse MainWindow2** for file handling, export, autosave and dialogs.
  Its menus stay reachable through the "☰" button; its toolbars are
  hidden and its panels are docked on the right per mode (they come up
  floating otherwise).
* **Newer undo system on by default.** With the legacy system, a deleted
  frame cannot be redone. With the new one, strokes, fills, moves, frame
  add and delete undo and redo correctly.
* **Frame editing uses the engine's semantics.** "New frame" takes the
  first free cell after the current one. "Delete" removes the key, and the
  previous drawing holds longer (no sliding). "Duplicate" copies into the
  next free cell and is not an undo step in the engine yet.
* **Pen buttons (ASUS Pen 2.0 / MPP):** same router as the Krita spike,
  ported to Qt 6. The native filter reads `GetPointerPenInfo()` flags
  (`PEN_FLAG_BARREL`, `PEN_FLAG_ERASER/INVERTED`) because hover presses
  never reach Qt as buttons. A press fires once (150 ms debounce). An
  application filter swallows what the press causes: right-button
  presses, Eraser pointer events (so the canvas never flips to the
  eraser), context menus, and tip presses until the pen lifts. Defaults:
  upper = eraser flag -> Redo, lower = barrel -> Undo. Both are remappable,
  with the detection step in "Pen". **Not yet tried on the real pen**; the
  self-test drives the code path through `injectInkFlags()`.
* **Beginner key map:** Ctrl+Z, Ctrl+Y / Ctrl+Shift+Z, B brush, N pencil,
  E eraser, S smudge, G fill, I picker, M select, V move, U lines,
  C camera, [ ] brush size, ← → frames, Ctrl+Return play, F1 / ? sheet,
  Ctrl+Alt+1/2/3 modes. Every other engine command has no key. Space+drag
  pans and the wheel zooms (the engine's defaults).

## Look and feel

The user found the first shell "like a Half-Life UI" and asked for
something cool and sleek. The redesign lives entirely in `shell/`:

* **One theme** (`ArtflowTheme`): Fusion style, a dark palette and one
  app-wide style sheet built from colour tokens (window, panel, raised,
  hover, line, text, muted, faint). One accent, violet `#8b7cff`; the
  camera is amber `#ffb454` wherever it appears. The engine panels'
  inline grey styles are stripped so the sheet reaches them.
* **Own icons** (`shell/icons/`, `ArtflowIcons`): 46 line icons drawn on a
  24 px grid, recoloured at runtime for normal, hover, checked and
  disabled. They replace the engine's glossy icons in the panels we show.
* **Custom controls** (`ShellControls`): the mode switch is one rounded
  track with a sliding highlight; brush size and opacity are tall pill
  sliders with the value written inside (empty when the tool has no
  size); palette swatches are rounded tiles with a ring around the chosen
  one (`SwatchDelegate`).
* **Canvas:** white paper on a dark desk. `CameraPainter::setOutsideShade()`
  (the one core patch for the look) darkens everything outside the camera
  frame, and the grey border around the canvas is gone.
* **Right column, same order in every mode:** layers (Studio), colour
  wheel, swatches, tool options. The layers and tool options end right
  below their content and grow back when a tool or a new layer needs more
  room; the wheel stays about square; the swatches take the rest.
* **Timelines fill the width:** Flip shows frame tiles with thumbnails of
  each drawing and empty tiles to the right edge; Motion's lanes, ruler
  and playhead run to the right edge too.
* **Defaults version 2** turns the palette into a swatch grid
  (`ColorPaletteViewMode`, `PreferredColorGridSize` = 28), once, so a
  later change in the preferences sticks.

## Brushes

The user found drawing "bland… not smooth… no brush styles… pixely".
Three causes, three fixes:

* **Pixely:** new drawings were 800 x 600 bitmaps blown up 1.5 x on a
  150 % screen with square pixels. Defaults version 3 makes new drawings
  1920 x 1080; `CanvasPainter::setSmoothScaling(true)` scales them
  smoothly (square pixels only above 400 % zoom, to inspect pixels); and
  `StudioCore::fitView()` fits the page to the window, again whenever the
  canvas changes size, until the view is zoomed or moved by hand.
* **Not smooth:** the engine joined pointer positions with straight lines
  of half-transparent dabs, one pressure per event. ArtFlow's own engine
  (`BrushStroke`) joins them with a centripetal Catmull-Rom curve, blends
  pressure along it, lays dabs at an even spacing with an anti-aliased
  (or soft) edge, and collects them in a coverage mask for the whole
  stroke, so a stroke does not get darker where its own dabs overlap
  (unless the brush builds up, like the airbrush). With a mouse, inking
  styles thin out with speed instead of pressure. The patches it hands
  back replace the same area of the stroke buffer; undo, commit and the
  eraser's cut-out work as before.
* **No styles:** 16 curated styles, original names. Pencil tool:
  Pencil, Soft Pencil, Clean Line, Charcoal, Crayon. Brush tool: Ink Pen,
  Fine Liner, Brush Pen, Calligraphy (inking); Round Brush, Soft Brush,
  Marker, Flat Paint, Watercolor (painting); Airbrush, Spray Can
  (effects). A style is a `BrushPreset`: hardness, spacing, flow, build-up,
  pressure to size and opacity, paper grain (tileable noise anchored to
  the canvas; light strokes catch only the ridges), flat-nib roundness and
  angle, jitter, scatter, spray particles, start taper. Each style
  remembers its own size. The picker (`BrushLibrary`) opens from the
  style button at the top of the left rail, or by clicking the chosen
  Brush or Pencil again; every card shows a sample stroke drawn by the
  engine itself. The eraser uses the same engine with a fixed round tip.
  Feather and anti-aliasing switches are hidden for these tools, since
  the styles shape their own edges.

## Tools, adjustments and layers

The user asked for more tools, then for a layers system. ArtFlow's own
tools live in `studiocore/tools/` and join the engine's through
`ToolManager::addTool()`; `ArtTool` gives them a live preview in the
stroke buffer and one undo step per change, like the brushes.

* **Shapes** (U): line, rectangle, ellipse, with or without a fill. Shift
  makes a square, a circle or a 45-degree line; Alt draws from the centre.
  The outline is drawn with the Brush tool's style and size, so a pencil
  box or an ink circle is one drag.
* **Lasso Fill** (L): draw around an area and it fills with the colour,
  self-crossing loops included (winding fill).
* **Text** (T): click, type, Enter (Shift+Enter for a new line, Escape to
  drop it). Five lettering styles, size in canvas pixels, bold. The text
  is drawn into the drawing, so it animates and erases like anything else.
* **Symmetry** (top bar): brush, pencil and eraser strokes are mirrored
  across the vertical centre line of the page, flat nibs and spray too; a
  dashed guide shows the line.
* **Adjust** (top bar): hue, saturation, brightness, contrast, blur,
  invert, black & white, on the current drawing or only its selected part,
  previewed live, one undo step on Apply (`StudioAdjust`, `AdjustPanel`).
  The blur has room to spread past the drawing's edge.
* **Options bar:** the chosen tool's options sit right after the tools
  (shape kind and fill; lettering, size, bold) and follow changes made
  elsewhere without being rebuilt.
* **Layers in every mode:** a thumbnail of each layer's drawing on the
  current frame, visibility, double-click to rename, drag to reorder (the
  camera layer stays at the bottom, an engine rule), duplicate, delete, and
  an opacity slider for the chosen layer. Layer opacity is a small core
  addition, saved in the file.
* **Window layout:** the engine's saved arrangement of bars and panels is
  no longer restored (only the window's size and place); the shell
  arranges them itself, and an arrangement saved by an older ArtFlow made
  the window's layout break and crash as it first showed.
* Custom controls stay custom: `ValueSlider` (name, value and a flat track
  that fills from zero) for Adjust and layer opacity.

## Krita-lite request vs this base

| Requested | Status |
|---|---|
| ~10 tools | 10 intents: brush, pencil, eraser, smudge, fill, picker, select, move, lines, camera |
| Lasso, shapes, text | Lasso Fill, Shapes (line, rectangle, ellipse, fill), Text; a lasso *selection* is still Phase 2 |
| Layers: paint, vector, group; blend modes; clipping | bitmap, vector, camera, sound layers with thumbnails, rename, reorder, duplicate, opacity; no groups, blend modes, clipping or merge yet |
| A few filters | Adjust: hue, saturation, brightness, contrast, blur, invert, black & white |
| Simple brush editor | ArtFlow's size/opacity sliders plus the engine panel (width, stabilizer, pressure) |
| Curated 20–30 brushes | 16 styles in ArtFlow's own brush engine, with a picker that shows a sample stroke of each |
| Beginner shortcuts with tooltips and a cheat sheet | done |
| On-screen undo/redo and size/opacity sliders | done |
| Touch: pinch, two-finger undo, three-finger redo | not in the engine; Phase 2 |
| Build only what's used | the engine is small (one program, ~80 MB deployed); unused panels are hidden |

## Self-test

71 of 71 checks pass (`H:\afdev\selftest\export-run1\report.txt`): window
title, brush with pressure 0.7 and tilt, eraser, fill + undo, selection, mode round
trip, Motion's camera tool, new/duplicate/delete frame, every pen-button
check (undo, redo, bounce, no stroke, no menu, eraser events blocked), the
key map, all 9 Studio tools, mouse drawing, size and opacity sliders, and
the tool options fitting without a scroll bar (before and after trying
every tool), and the brushes: 1920 x 1080 pages that fit the window,
every style painting a pressure stroke (`brush-sampler.png`), smooth ink
edges, a marker that does not darken over itself, an airbrush that
builds up, a sample stroke for every style, and both pickers opening
(`brush-library.png`, `pencil-library.png`), then the new tools: a filled
rectangle, an outline-only ellipse, a 45-degree line with Shift, Lasso
Fill, Text with undo, symmetry, Adjust with undo, blur, and layers
(rename, opacity, move down to the camera and back, duplicate, layers in
Flip mode); `new-tools.png`, `layers-panel.png`, `adjust-panel.png` and
`shapes-bar.png` show them. With ffmpeg bundled, a GIF and an MP4 are
exported and ffmpeg reads the MP4 back as H.264. The test keeps the person's
brush settings and puts them back at the end. The self-test maximizes the window and waits until its size
stops changing before the first stroke, and saves screenshots of every
mode next to the report.

## Phase 2

1. Real-pen test with the ASUS Pen 2.0, then tune the debounce and defaults.
2. Lasso selection, layer blend modes, clipping, merge down and groups.
3. Motion: per-layer transform/opacity keys with easing (today only the
   camera layer animates), plus a keyframe gizmo on the canvas.
4. Flip: frame thumbnails, insert-and-slide editing, per-layer onion skin.
5. Touch gestures (pinch zoom/rotate, two/three-finger taps).
6. Branding pass: About dialog and engine dialog strings, translations,
   icon, installer.
7. A simple brush editor on top of the 16 styles.
