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
                      LayerStackPanel (Studio), BrushSlidersPanel, TransportBar
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

## Krita-lite request vs this base

| Requested | Status |
|---|---|
| ~10 tools | 10 intents: brush, pencil, eraser, smudge, fill, picker, select, move, lines, camera |
| Lasso, shapes, text | not in the engine; Phase 2 |
| Layers: paint, vector, group; blend modes; clipping | bitmap, vector, camera, sound layers; no groups, blend modes or clipping yet |
| A few filters | none in the engine; Phase 2 |
| Simple brush editor | engine tool options panel (width, feather, stabilizer, pressure) plus ArtFlow's size/opacity sliders |
| Curated 20–30 brushes | the engine has tools, not preset libraries; Phase 2 |
| Beginner shortcuts with tooltips and a cheat sheet | done |
| On-screen undo/redo and size/opacity sliders | done |
| Touch: pinch, two-finger undo, three-finger redo | not in the engine; Phase 2 |
| Build only what's used | the engine is small (one program, ~80 MB deployed); unused panels are hidden |

## Self-test

41 of 41 checks pass (`H:\afdev\selftest\ui-run8\report.txt`): brush
with pressure 0.7 and tilt, eraser, fill + undo, selection, mode round
trip, Motion's camera tool, new/duplicate/delete frame, every pen-button
check (undo, redo, bounce, no stroke, no menu, eraser events blocked), the
key map, all 9 Studio tools, mouse drawing, size and opacity sliders, and
the tool options fitting without a scroll bar (before and after trying
every tool). The self-test maximizes the window first and saves
screenshots of every mode next to the report.

## Phase 2

1. Real-pen test with the ASUS Pen 2.0, then tune the debounce and defaults.
2. Lasso and shape tools, a text tool, a few filters (blur, hue/saturation,
   brightness/contrast), layer blend modes, opacity and clipping.
3. Motion: per-layer transform/opacity keys with easing (today only the
   camera layer animates), plus a keyframe gizmo on the canvas.
4. Flip: frame thumbnails, insert-and-slide editing, per-layer onion skin.
5. Touch gestures (pinch zoom/rotate, two/three-finger taps).
6. Branding pass: About dialog and engine dialog strings, translations,
   icon, installer.
7. Curated brush presets and a simple brush editor.
