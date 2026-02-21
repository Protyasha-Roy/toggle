# toggle

Fast, keyboard‑driven drawing app built with raylib.

## Install (Linux/macOS)

### Dependencies
- CMake 3.16+
- C++17 compiler
- raylib (dynamic, system‑installed)

Clone the repo first.
Go to the directory with:
```bash
cd toggle
```


On Arch:
```bash
sudo pacman -S raylib cmake ninja
```

On Ubuntu/Debian:
```bash
sudo apt install libraylib-dev cmake ninja-build
```

On macOS (Homebrew):
```bash
brew install raylib cmake ninja
```

### Build
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run
```bash
./toggle
```

### Install system‑wide (optional)
```bash
sudo cmake --install build --prefix /usr/local
```

### App launcher integration (rofi, dmenu, GNOME/KDE)
After installing, the app is discoverable by launchers via the desktop entry.

If you installed system‑wide:
```bash
sudo cmake --install build --prefix /usr/local
```

If you want a user‑only install:
```bash
cmake --install build --prefix ~/.local
update-desktop-database ~/.local/share/applications
```

Then search for `toggle` in your launcher.

## Cheatsheet

### Modes
- `S` Selection
- `M` Move (pan)
- `P` Pen
- `L` Line
- `C` Circle
- `Ctrl+R` Rectangle
- `R` or `Shift+R` Resize/Rotate
- `E` Eraser
- `T` Text

### Mode prefixes (press before the mode key)
- `D` dotted (line, circle, rectangle)
- `A` arrow (line only)

### Mouse basics
- Selection: click to select, drag selected to move, drag empty space to box‑select.
- Resize/Rotate: drag handles to resize, drag rotate handle to rotate, drag inside to move.
- Text: click text to edit, click empty space to place.
- Move mode: drag to pan.
- Pen/shape modes: click‑drag to draw.

### Selection & order
- `Ctrl+A` select all
- `X` delete selection
- `[` move selection backward
- `]` move selection forward
- `G` group selected
- `Shift+G` ungroup
- `F` toggle element ID tags
- `J` / `K` select next/previous tag
- Number keys select element by ID (when tags are visible)

### Edit & history
- `U` or `Ctrl+Z` undo
- `Shift+U`, `Ctrl+Y`, or `Ctrl+Shift+Z` redo
- `Y` copy
- `Shift+P` paste
- `=` / `-` increase/decrease stroke width

### View
- Mouse wheel zoom
- `Shift+=` / `Shift+-` zoom in/out (also `KP+` / `KP-`)
- `Tab` toggle status bar

### Command mode
Press `Shift+;` to open command mode, then type commands:

**File**
- `:w [filename] [dir]` save
- `:wq` save and quit
- `:q` quit
- `:open` or `:o` open last saved
- `:open <filename> [dir]` open file

**Appearance**
- `:theme dark|light`
- `:type blank|grid|dotted`
- `:gridw <6-200>` grid spacing
- `:color <#RRGGBB|#RRGGBBAA|name>` set color (applies to selection if any)
- `:strokew <min-max>` set stroke width (applies to selection if any)
- `:font <size>` set text size (applies to selected text if any)
- `:font-family <path>` load a font file

**Export**
- `:export [png|jpg|svg] [all|selected|frame] [name] [dir]`

**Window**
- `:resizet <px>` / `:resizeb <px>` / `:resizel <px>` / `:resizer <px>`

**Config**
- `:reloadconfig`
- `:writeconfig`

## Features (complete)
- Freehand pen drawing.
- Lines (solid, dotted, arrow).
- Circles (solid, dotted).
- Rectangles (solid, dotted).
- Text tool with in‑place editing.
- Eraser tool.
- Selection, multi‑selection, box select.
- Move/pan mode.
- Resize and rotate any element.
- Group / ungroup elements.
- Z‑order control (forward/backward).
- Copy/paste with offset.
- Tag IDs for fast selection (J/K and numeric jump).
- Undo/redo.
- Zoom in/out.
- Status bar toggle.
- Dark/light themes.
- Background types: blank, grid, dotted.
- Export to PNG/JPG/SVG (all/selected/frame).
- Config reload/write and runtime commands.
