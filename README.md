# toggle

Straightforward drawing app with fast keyboard workflows.

**Quick Start**
1. Build: `g++ main.cpp -lraylib -lGL -lm -lpthread -ldl -lrt -o toggle`
2. Run: `./toggle`
3. Enter command mode: `Shift+;` then type `:w` to save.

**Modes**
- `S` Selection
- `M` Move (pan)
- `P` Pen
- `L` Line (use `D` for dotted, `A` for arrow before `L`)
- `C` Circle (use `D` for dotted before `C`)
- `Ctrl+R` Rectangle (use `D` for dotted before `Ctrl+R`)
- `R` or `Shift+R` Resize/Rotate
- `E` Eraser
- `T` Text

**Mouse Basics**
- Selection mode: click to select, drag selection to move, drag empty space to box‑select.
- Resize/Rotate mode: click element to select, drag handles to resize, drag rotate handle to rotate.
- Text mode: click existing text to edit, click empty space to place new text.
- Move mode: drag to pan.
- Pen/shape modes: click‑drag to draw.

**Selection & Ordering**
- `Ctrl+A` select all
- `X` delete selection
- `[` move selection backward
- `]` move selection forward
- `G` group selected
- `Shift+G` ungroup
- `F` toggle element ID tags
- `J` / `K` select next/previous tag
- Number keys select element by ID (when tags are visible)

**Edit & History**
- `U` or `Ctrl+Z` undo
- `Shift+U`, `Ctrl+Y`, or `Ctrl+Shift+Z` redo
- `Y` copy
- `Shift+P` paste
- `=` / `-` increase/decrease stroke width

**View**
- Mouse wheel zoom
- `Shift+=` / `Shift+-` zoom in/out (also `KP+` / `KP-`)
- `Tab` toggle status bar

**Command Mode**
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

**Notes**
- In resize/rotate mode, handles are drawn on the selected element. Drag the top handle to rotate.
- The default background is blank. Use `:type grid` or `:type dotted` if you want guides.
