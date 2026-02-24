# Toggle : A simple and keyboard-friendly graphics drawing tool.
Built with C++ and Raylib.

1. Clone the repo

2. Compile with `g++ main.cpp -lraylib -lGL -lm -lpthread -ldl -lrt -o toggle` [install necessary dependencies]

3. Run ./toggle 

## Features summary
- Move cursor and elements without using the mouse on anti-mouse mode
- Freehand pen drawing.
- Lines (solid, dotted, arrow).
- Circles (solid, dotted).
- Rectangles (solid, dotted).
- Triangles (equilateral, dotted).
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


# Toggle Cheatsheet

### Modes

| Key | Action | Key | Action |
| --- | --- | --- | --- |
| `s` | Selection mode | `p` | Pen/Pencil mode |
| `m` | Move mode | `e` | Eraser mode |
| `t` | Triangle mode | `shift+R` | Resize/Rotate mode |
| `l` | Straight line | `dl` | Dotted line |
| `al` | Arrow line | `c` | Circle |
| `dc` | Dotted circle | `r` | Rectangle |
| `dr` | Dotted rectangle | `dt` | Dotted triangle |
| `T` | Text mode | `f` | Toggle numbered hints |
| `o` | anti-mouse mode |  |  |


### Anti-mouse mode
On anti mouse mode you can move your cursor as well as any element(s) without the mouse.

In order to move the cursor type 'o' and then you will see a red + on the top-left corner of the canvas.

Now use ctrl + WASD to move around. And comma(,) to press left. In case of creating rectangles and selecting in selection mode, you need to trigger selection with two movements after pressing ctrl + ,

So it's like ctrl + , + DW (you can go anyside after this).

And to move elements like this just use Shift + WASD.

### Actions

| Key | Action | Key | Action |
| --- | --- | --- | --- |
| `y` | Yank (Copy) | `P` | Paste |
| `x` | Delete selection | `g` | Group selection |
| `u` | Undo | `G` | Ungroup selection |
| `U` | Redo | `[` | Send backward |
| `+` | Zoom in | `]` | Bring forward |
| `_` | Zoom out | `=` | Increase stroke width |
| `esc` | Stop/Quit/Exit | `-` | Decrease stroke width |
| `tab` | hide/show status bar | `[` | Send backwards |
| `]`  | Send forwareds |


### Selection Mode (Active)

| Key | Action |
| --- | --- |
| `j` | Next numbered element |
| `k` | Previous numbered element |
| `[number]` | Jump to specific element |

### Commands (prefix `:`)

| Command | Action |
| --- | --- |
| `:w [filename] [dir]` | Save (optional filename) |
| `:wq` | Save and quit |
| `:q` | Quit |
| `:open [path]` | Open Toggle file |
| `:export [png/svg/jpeg] 'filename'` | Export canvas |
| `:color [#RRGGBB//RRGGBBAA/name]` | Set stroke color |
| `:strokew [n]` | Set stroke width |
| `:font [number]` | Set font size |
| `:font-family [path]` | Set font family |
| `:theme [dark/light]` | Change UI theme |
| `:type [type]` | Background: `blank`, `grid`, `dotted` |
| `:gridw [n]` | Set grid size |
| `:resize[t/b/r/l] [px]` | Resize side (top/bottom/right/left) |

### Mouse & UI

* **Drag Elements:** Move selection.
* **Drag Handles:** Resize/Rotate.
* **Scroll:** Zoom in/out.
* **Click + Drag Space:** Pan canvas.
* **Numbered Tags:** Auto-assigned to elements for `[number]` navigation (hidden on export).
* **Status Bar:** Vim-style mode/command display.
* **Config:** Single file for all default behaviors and keybindings.

---
