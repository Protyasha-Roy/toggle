# toggle

Fast, keyboard‑driven drawing app built with raylib.

## Installation
First install dependencies. You must have raylib. Gcc compiler.
Compilation command:
g++ main.cpp -lraylib -lGL -lm -lpthread -ldl -lrt -o toggle

Next steps:
1. Clone the repo
2. cd into the folder toggle
3. type ./toggle to quickly run the project
4. In order to make a desktop entry for app launcher:
  a. Copy paste the toggle.desktop to ~/.local/share/applications
  b. Open the .desktop entry and replace paths with actual path to the toggle executable and the toggle folder
  c. Search for the project from app launcher.

## Cheatsheet

### Modes
- `s` Selection
- `m` Move (pan)
- `p` Pen
- `l` Line
- `c` Circle
- `r` Rectangle
- `R` or `Shift+R` Resize/Rotate
- `e` Eraser
- `t` Text

### Mode prefixes (press before the mode key)
- `d` dotted (line, circle, rectangle)
- `a` arrow (line only)
dt : dotted line
dc : dotted circle
al : arrow line
dr : dotted rectangle

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



## Summary
### Toggle: a modal, keyboard-driven,  minimalist graphics tool

Key-features with respective keybindings:
s: selection mode - DONE 
m: move mode - DONE
dl: dotted line mode - DONE
l: straight line mode - DONE
al: arrow line mode - DONE
c: circle mode - DONE
dc: dotted circle mode - DONE
r: rectangle mode - DONE
dr: dotted rectangle mode - DONE
t: text mode - DONE
+: zoom in - DONE
_: zoom out - DONE
g: group selection - DONE
G: ungroup selection - DONE
y: yank/copy selection - DONE
u: undo - DONE 
U: redo - DONE
x: delete selection - DONE
p: pen/pencil mode - DONE
P: paste yanked element - DONE
e: eraser mode - DONE
[: z-index bring backward - DONE
]: z-index bring forward - DONE
esc: to stop quit command mode/to stop taking input a keybinding/to escape text mode and anything else - DONE
=: increase stroke width - DONE
-: decrease stroke width - DONE
f: show/hide numbered hints - DONE
shift+R: resize/rotate mode - DONE

While in selection mode:
j: go to the next numbered element - DONE 
k: go to the previous numbered element - DONE
[number]: takes to that numbered element directly - DONE


Other command driven features:
:q // quit app
:theme dark/light // change theme
:font [number] // font-size for text
:font-family [name-of-the-font] // font-family
:resizet [number] // resize top
:resizeb [number] // resize bottom
:resizer [number] // resize right
:resizel [number] // resize left
:color [hex-code] // set color
:strokew [number] // stroke width
:type [string]    // Three types: blank, grid, dotted
:gridw [number]   // grid size
:w [filename]     // Save as 'filename' only if not already saved, otherwise :w should save it with the default name, or the previous name(if given before).
:wq               // Save and quit
:export [png/svg] // Export
:open 'filepath/name' // open toggle file

Other mouse related features:
resizing elements with mouse - DONE
rotating elements with mouse - DONE
moving elements with mouse - DONE
selecting to perform actions(with or without-keyboard(command drivens)) - DONE
zooming in and out - DONE
moving the canvas - DONE

Other:
One config file to config everything(the defaults). - DONE
This config file should also let user change default keybindings - DONE
and all other defaults.

Each element when inserted will show a number tag by default. So that it's easier to navigate between the elements. - DONE
But while exporting these numbered tags will not longer be visible in the exported image. 
Vim-like Status bar - DONE

