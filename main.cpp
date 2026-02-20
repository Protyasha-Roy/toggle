#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <string>
#include <vector>

using namespace std;

enum Mode {
  SELECTION_MODE,
  MOVE_MODE,
  LINE_MODE,
  DOTTEDLINE_MODE,
  ARROWLINE_MODE,
  CIRCLE_MODE,
  DOTTEDCIRCLE_MODE,
  RECTANGLE_MODE,
  DOTTEDRECT_MODE,
  TEXT_MODE,
  ERASER_MODE,
  PEN_MODE,
  GROUP_MODE,
};

struct Element {
  Mode type;
  Vector2 start;
  Vector2 end;
  float strokeWidth;
  Color color;
  vector<Vector2> path;
  int originalIndex = -1;
  int uniqueID = -1;
  vector<Element> children;
  string text;

  Rectangle GetBounds() const {
    float minX, minY, maxX, maxY;
    if (type == GROUP_MODE && !children.empty()) {
      Rectangle b = children[0].GetBounds();
      minX = b.x;
      minY = b.y;
      maxX = b.x + b.width;
      maxY = b.y + b.height;
      for (const auto &child : children) {
        Rectangle cb = child.GetBounds();
        minX = min(minX, cb.x);
        minY = min(minY, cb.y);
        maxX = max(maxX, cb.x + cb.width);
        maxY = max(maxY, cb.y + cb.height);
      }
    } else if (type == PEN_MODE && !path.empty()) {
      minX = maxX = path[0].x;
      minY = maxY = path[0].y;
      for (auto &p : path) {
        minX = min(minX, p.x);
        minY = min(minY, p.y);
        maxX = max(maxX, p.x);
        maxY = max(maxY, p.y);
      }
  } else if (type == CIRCLE_MODE || type == DOTTEDCIRCLE_MODE) {
    float r = Vector2Distance(start, end);
    minX = start.x - r;
    minY = start.y - r;
    maxX = start.x + r;
    maxY = start.y + r;
  } else if (type == TEXT_MODE) {
    minX = start.x;
    minY = start.y;
    maxX = end.x;
    maxY = end.y;
  } else {
      minX = min(start.x, end.x);
      minY = min(start.y, end.y);
      maxX = max(start.x, end.x);
      maxY = max(start.y, end.y);
    }
    return {minX, minY, maxX - minX, maxY - minY};
  }
};

struct Canvas {
  Mode mode = SELECTION_MODE;
  float strokeWidth = 2.0f;
  Vector2 startPoint = {0};
  Vector2 currentMouse = {0};
  bool isDragging = false;
  Font font;
  const char *modeText = "SELECTION";
  Color modeColor = MAROON;
  vector<Element> elements;
  vector<vector<Element>> undoStack;
  vector<Element> clipboard;
  vector<vector<Element>> redoStack;
  vector<Vector2> currentPath;
  bool showTags = false;
  vector<int> selectedIndices;
  bool isTypingNumber = false;
  int inputNumber = 0;
  double lastInputTime = 0.0;
  bool hasMoved = false;
  bool isBoxSelecting = false;
  int lastKey = 0;
  int nextElementId = 0;
  bool isTextEditing = false;
  string textBuffer;
  Vector2 textPos = {0};
  float textSize = 24.0f;
  int editingIndex = -1;
  string editingOriginalText;
  Color editingColor = BLACK;
  bool textEditBackedUp = false;
  double lastClickTime = 0.0;
  Vector2 lastClickPos = {0};
  int pasteOffsetIndex = 0;
  Camera2D camera = {};
};

void SaveBackup(Canvas &canvas) {
  canvas.undoStack.push_back(canvas.elements);
  canvas.redoStack.clear();
}

// helper: ensure element (and its children if group) has a non-negative
// uniqueID
void EnsureUniqueIDRecursive(Element &el, Canvas &canvas) {
  if (el.uniqueID < 0) {
    el.uniqueID = canvas.nextElementId++;
  }
  if (el.type == GROUP_MODE) {
    for (auto &c : el.children) {
      if (c.uniqueID < 0) {
        EnsureUniqueIDRecursive(c, canvas);
      }
    }
  }
}

int FindElementIndexByID(const Canvas &canvas, int id) {
  for (int i = 0; i < (int)canvas.elements.size(); ++i) {
    if (canvas.elements[i].uniqueID == id)
      return i;
  }
  return -1;
}

vector<int> GetSelectedIDs(const Canvas &canvas) {
  vector<int> ids;
  for (int idx : canvas.selectedIndices) {
    if (idx >= 0 && idx < (int)canvas.elements.size()) {
      int id = canvas.elements[idx].uniqueID;
      if (id >= 0 && find(ids.begin(), ids.end(), id) == ids.end())
        ids.push_back(id);
    }
  }
  return ids;
}

void DrawDashedLine(Vector2 start, Vector2 end, float width, Color color) {
  float dashLen = 4.0f;
  float totalLen = Vector2Distance(start, end);
  if (totalLen < 1.0f)
    return;
  Vector2 dir = Vector2Normalize(Vector2Subtract(end, start));
  for (float i = 0; i < totalLen; i += dashLen * 2) {
    Vector2 s = Vector2Add(start, Vector2Scale(dir, i));
    Vector2 e =
        Vector2Add(start, Vector2Scale(dir, min(i + dashLen, totalLen)));
    DrawLineEx(s, e, width, color);
  }
}

void DrawArrowLine(Vector2 start, Vector2 end, float width, Color color) {
  DrawLineEx(start, end, width, color);
  float angle = atan2f(end.y - start.y, end.x - start.x);
  float headSize = 15.0f;
  Vector2 p1 = {end.x - headSize * cosf(angle - PI / 6),
                end.y - headSize * sinf(angle - PI / 6)};
  Vector2 p2 = {end.x - headSize * cosf(angle + PI / 6),
                end.y - headSize * sinf(angle + PI / 6)};
  DrawLineEx(end, p1, width, color);
  DrawLineEx(end, p2, width, color);
}

void DrawDashedRing(Vector2 center, float radius, float width, Color color) {
  int segments = (int)(radius * 1.5f);
  if (segments < 40)
    segments = 40;
  float step = (2 * PI) / segments;
  for (int i = 0; i < segments; i += 2) {
    Vector2 s = {center.x + cosf(i * step) * radius,
                 center.y + sinf(i * step) * radius};
    Vector2 e = {center.x + cosf((i + 1) * step) * radius,
                 center.y + sinf((i + 1) * step) * radius};
    DrawLineEx(s, e, width, color);
  }
}

void DrawElement(const Element &el, const Font &font, float textSize) {
  if (el.type == LINE_MODE)
    DrawLineEx(el.start, el.end, el.strokeWidth, el.color);
  else if (el.type == DOTTEDLINE_MODE)
    DrawDashedLine(el.start, el.end, el.strokeWidth, el.color);
  else if (el.type == ARROWLINE_MODE)
    DrawArrowLine(el.start, el.end, el.strokeWidth, el.color);
  else if (el.type == CIRCLE_MODE)
    DrawRing(el.start, Vector2Distance(el.start, el.end) - el.strokeWidth / 2,
             Vector2Distance(el.start, el.end) + el.strokeWidth / 2, 0, 360, 60,
             el.color);
  else if (el.type == DOTTEDCIRCLE_MODE)
    DrawDashedRing(el.start, Vector2Distance(el.start, el.end), el.strokeWidth,
                   el.color);
  else if (el.type == RECTANGLE_MODE)
    DrawRectangleLinesEx({min(el.start.x, el.end.x), min(el.start.y, el.end.y),
                          abs(el.end.x - el.start.x),
                          abs(el.end.y - el.start.y)},
                         el.strokeWidth, el.color);
  else if (el.type == DOTTEDRECT_MODE) {
    Rectangle r = {min(el.start.x, el.end.x), min(el.start.y, el.end.y),
                   abs(el.end.x - el.start.x), abs(el.end.y - el.start.y)};
    DrawDashedLine({r.x, r.y}, {r.x + r.width, r.y}, el.strokeWidth, el.color);
    DrawDashedLine({r.x + r.width, r.y}, {r.x + r.width, r.y + r.height},
                   el.strokeWidth, el.color);
    DrawDashedLine({r.x + r.width, r.y + r.height}, {r.x, r.y + r.height},
                   el.strokeWidth, el.color);
    DrawDashedLine({r.x, r.y + r.height}, {r.x, r.y}, el.strokeWidth, el.color);
  } else if (el.type == PEN_MODE) {
    int pointCount = (int)el.path.size();

    if (pointCount == 1) {
      DrawCircleV(el.path[0], el.strokeWidth / 2, el.color);
    }
    // Catmull-Rom requires at least 4 points to calculate the curve
    else if (pointCount >= 4) {
      DrawSplineCatmullRom(el.path.data(), pointCount, el.strokeWidth,
                           el.color);
    }
    // Fallback for 2 or 3 points: Draw straight lines
    else if (pointCount > 1) {
      DrawLineStrip(el.path.data(), pointCount, el.color);
    }
  } else if (el.type == GROUP_MODE) {
    for (const auto &child : el.children)
      DrawElement(child, font, textSize);
  } else if (el.type == TEXT_MODE) {
    DrawTextEx(font, el.text.c_str(), el.start, textSize, 2, el.color);
  }
}

void MoveElement(Element &el, Vector2 delta) {
  el.start = Vector2Add(el.start, delta);
  el.end = Vector2Add(el.end, delta);
  for (auto &p : el.path)
    p = Vector2Add(p, delta);
  for (auto &child : el.children)
    MoveElement(child, delta);
}

void RestoreZOrder(Canvas &canvas) {
  if (canvas.selectedIndices.empty())
    return;

  struct PendingRestore {
    Element el;
    int target;
  };
  vector<PendingRestore> toRestore;

  // Process selected indices from top to bottom so erase doesn't shift
  // yet-to-be-processed indices
  sort(canvas.selectedIndices.begin(), canvas.selectedIndices.end(),
       greater<int>());

  for (int idx : canvas.selectedIndices) {
    if (idx >= 0 && idx < (int)canvas.elements.size() &&
        canvas.elements[idx].originalIndex != -1) {
      toRestore.push_back(
          {canvas.elements[idx], canvas.elements[idx].originalIndex});
      canvas.elements.erase(canvas.elements.begin() + idx);
    }
  }

  sort(toRestore.begin(), toRestore.end(),
       [](const PendingRestore &a, const PendingRestore &b) {
         return a.target < b.target;
       });

  for (auto &item : toRestore) {
    item.el.originalIndex = -1;
    if (item.target >= (int)canvas.elements.size()) {
      canvas.elements.push_back(item.el);
    } else {
      canvas.elements.insert(canvas.elements.begin() + item.target, item.el);
    }
  }

  canvas.selectedIndices.clear();
}

int main() {
  const int screenWidth = 1000;
  const int screenHeight = 800;
  Canvas canvas;
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(screenWidth, screenHeight, "Toggle : no more toggling");
  SetExitKey(KEY_NULL);
  SetTargetFPS(60);
  canvas.camera.offset = {0.0f, 0.0f};
  canvas.camera.target = {0.0f, 0.0f};
  canvas.camera.rotation = 0.0f;
  canvas.camera.zoom = 1.0f;
  canvas.font = LoadFont("IosevkaNerdFontMono-Regular.ttf");
  SetTextureFilter(canvas.font.texture, TEXTURE_FILTER_BILINEAR);

  while (true) {
    bool escPressed = IsKeyPressed(KEY_ESCAPE);
    if (WindowShouldClose() && !escPressed)
      break;

    int key = GetKeyPressed();
    bool shiftDown = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool ctrlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    Vector2 mouseScreen = GetMousePosition();
    Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, canvas.camera);

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
      float oldZoom = canvas.camera.zoom;
      float nextZoom = oldZoom + wheel * 0.1f * oldZoom;
      if (nextZoom < 0.1f)
        nextZoom = 0.1f;
      if (nextZoom > 10.0f)
        nextZoom = 10.0f;
      if (nextZoom != oldZoom) {
        Vector2 before = GetScreenToWorld2D(mouseScreen, canvas.camera);
        canvas.camera.zoom = nextZoom;
        Vector2 after = GetScreenToWorld2D(mouseScreen, canvas.camera);
        canvas.camera.target =
            Vector2Subtract(canvas.camera.target, Vector2Subtract(before, after));
      }
    }

    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
      float oldZoom = canvas.camera.zoom;
      float nextZoom = oldZoom * 1.1f;
      if (nextZoom > 10.0f)
        nextZoom = 10.0f;
      if (nextZoom != oldZoom) {
        Vector2 before = GetScreenToWorld2D(mouseScreen, canvas.camera);
        canvas.camera.zoom = nextZoom;
        Vector2 after = GetScreenToWorld2D(mouseScreen, canvas.camera);
        canvas.camera.target =
            Vector2Subtract(canvas.camera.target, Vector2Subtract(before, after));
      }
    }
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
      float oldZoom = canvas.camera.zoom;
      float nextZoom = oldZoom / 1.1f;
      if (nextZoom < 0.1f)
        nextZoom = 0.1f;
      if (nextZoom != oldZoom) {
        Vector2 before = GetScreenToWorld2D(mouseScreen, canvas.camera);
        canvas.camera.zoom = nextZoom;
        Vector2 after = GetScreenToWorld2D(mouseScreen, canvas.camera);
        canvas.camera.target =
            Vector2Subtract(canvas.camera.target, Vector2Subtract(before, after));
      }
    }

    if (canvas.isTextEditing && escPressed) {
      canvas.isTextEditing = false;
      canvas.textBuffer.clear();
      canvas.editingIndex = -1;
      canvas.textEditBackedUp = false;
    }
    if (canvas.isTextEditing)
      key = 0;

    // --- key handling: NOTE: P is handled in a single branch to avoid both
    // paste and pen activation on Shift+P
    if (!canvas.isTextEditing && IsKeyPressed(KEY_Y) && !ctrlDown) {
      if (!canvas.selectedIndices.empty()) {
        canvas.clipboard.clear();
        for (int idx : canvas.selectedIndices) {
          if (idx >= 0 && idx < (int)canvas.elements.size())
            canvas.clipboard.push_back(canvas.elements[idx]);
        }
        canvas.pasteOffsetIndex = 0;
      }
    }

    if (!canvas.isTextEditing && IsKeyPressed(KEY_P)) {
      // SHIFT+P => Paste (only if clipboard not empty)
      if (shiftDown) {
        if (!canvas.clipboard.empty()) {
          SaveBackup(canvas);
          RestoreZOrder(canvas);
          canvas.selectedIndices.clear();

          float step = 20.0f * (canvas.pasteOffsetIndex + 1);
          Vector2 pasteOffset = {step, step};
          for (const auto &item : canvas.clipboard) {
            Element cloned = item;

            // Assign a fresh ID to the copy (and ensure children have IDs)
            cloned.uniqueID = canvas.nextElementId++;
            if (cloned.type == GROUP_MODE) {
              for (auto &c : cloned.children)
                EnsureUniqueIDRecursive(c, canvas);
            }

            cloned.originalIndex = -1;

            MoveElement(cloned, pasteOffset);
            canvas.elements.push_back(cloned);
            canvas.selectedIndices.push_back((int)canvas.elements.size() - 1);
          }
          canvas.pasteOffsetIndex++;
        }
      } else {
        // plain 'p' => PEN mode
        canvas.mode = PEN_MODE;
        canvas.modeText = "PEN";
        canvas.modeColor = BLACK;
        canvas.isTypingNumber = false;
      }
    }

    // Other mode keys
    if (!canvas.isTextEditing && IsKeyPressed(KEY_S)) {
      canvas.mode = SELECTION_MODE;
      canvas.modeText = "SELECTION";
      canvas.modeColor = MAROON;
      canvas.isTypingNumber = false;
    }
    if (!canvas.isTextEditing && IsKeyPressed(KEY_M)) {
      canvas.mode = MOVE_MODE;
      canvas.modeText = "MOVE";
      canvas.modeColor = DARKBROWN;
      canvas.isTypingNumber = false;
    }
    if (!canvas.isTextEditing && IsKeyPressed(KEY_L)) {
      if (canvas.lastKey == KEY_D) {
        canvas.mode = DOTTEDLINE_MODE;
        canvas.modeText = "DOTTED LINE";
      } else if (canvas.lastKey == KEY_A) {
        canvas.mode = ARROWLINE_MODE;
        canvas.modeText = "ARROW LINE";
      } else {
        canvas.mode = LINE_MODE;
        canvas.modeText = "LINE";
      }
      canvas.modeColor = BLUE;
      canvas.isTypingNumber = false;
    }
    if (!canvas.isTextEditing && IsKeyPressed(KEY_C)) {
      if (canvas.lastKey == KEY_D) {
        canvas.mode = DOTTEDCIRCLE_MODE;
        canvas.modeText = "DOTTED CIRCLE";
      } else {
        canvas.mode = CIRCLE_MODE;
        canvas.modeText = "CIRCLE";
      }
      canvas.modeColor = DARKGREEN;
      canvas.isTypingNumber = false;
    }
    if (!canvas.isTextEditing && IsKeyPressed(KEY_R)) {
      if (canvas.lastKey == KEY_D) {
        canvas.mode = DOTTEDRECT_MODE;
        canvas.modeText = "DOTTED RECT";
      } else {
        canvas.mode = RECTANGLE_MODE;
        canvas.modeText = "RECTANGLE";
      }
      canvas.modeColor = RED;
      canvas.isTypingNumber = false;
    }
    if (!canvas.isTextEditing && IsKeyPressed(KEY_E)) {
      canvas.mode = ERASER_MODE;
      canvas.modeText = "ERASER";
      canvas.modeColor = ORANGE;
      canvas.isTypingNumber = false;
    }
    if (!canvas.isTextEditing && IsKeyPressed(KEY_T)) {
      canvas.mode = TEXT_MODE;
      canvas.modeText = "TEXT";
      canvas.modeColor = DARKBLUE;
      canvas.isTypingNumber = false;
      if (!canvas.isTextEditing) {
        canvas.textBuffer.clear();
        canvas.editingIndex = -1;
      }
    }

    if (!canvas.isTextEditing && IsKeyPressed(KEY_G)) {
      if (shiftDown) {
        // Ungroup selected group(s)
        if (!canvas.selectedIndices.empty()) {
          SaveBackup(canvas);
          vector<int> sorted = canvas.selectedIndices;
          sort(sorted.begin(), sorted.end(), greater<int>());
          for (int idx : sorted) {
            if (idx >= 0 && idx < (int)canvas.elements.size() &&
                canvas.elements[idx].type == GROUP_MODE) {
              Element g = canvas.elements[idx];
              canvas.elements.erase(canvas.elements.begin() + idx);
              // push children back to canvas (children already have IDs)
              for (auto &child : g.children) {
                canvas.elements.push_back(child);
              }
            }
          }
          canvas.selectedIndices.clear();
        }
      } else if (canvas.selectedIndices.size() > 1) {
        // Group selected elements into a single group
        SaveBackup(canvas);
        Element group;
        group.type = GROUP_MODE;
        group.strokeWidth = canvas.strokeWidth;
        group.color = canvas.modeColor;
        group.uniqueID =
            canvas.nextElementId++; // assign ID to the group itself

        vector<int> sorted = canvas.selectedIndices;
        sort(sorted.begin(), sorted.end(), greater<int>());
        for (int idx : sorted) {
          if (idx >= 0 && idx < (int)canvas.elements.size()) {
            group.children.push_back(canvas.elements[idx]);
            canvas.elements.erase(canvas.elements.begin() + idx);
          }
        }
        Rectangle gb = group.GetBounds();
        group.start = {gb.x, gb.y};
        // Ensure children have valid IDs
        for (auto &c : group.children)
          EnsureUniqueIDRecursive(c, canvas);

        canvas.elements.push_back(group);
        canvas.selectedIndices = {(int)canvas.elements.size() - 1};
      }
    }

    if (!canvas.isTextEditing && IsKeyPressed(KEY_F))
      canvas.showTags = !canvas.showTags;
    if (key != 0)
      canvas.lastKey = key;

    if (!canvas.isTextEditing) {
      bool redoPressed = (IsKeyPressed(KEY_U) && shiftDown) ||
                         (ctrlDown && IsKeyPressed(KEY_Y)) ||
                         (ctrlDown && shiftDown && IsKeyPressed(KEY_Z));
      bool undoPressed =
          (!redoPressed && IsKeyPressed(KEY_U)) || (ctrlDown && IsKeyPressed(KEY_Z));

      if (redoPressed) {
        if (!canvas.redoStack.empty()) {
          canvas.undoStack.push_back(canvas.elements);
          canvas.elements = canvas.redoStack.back();
          canvas.redoStack.pop_back();
          canvas.selectedIndices.clear();
        }
      } else if (undoPressed && !canvas.undoStack.empty()) {
        canvas.redoStack.push_back(canvas.elements);
        canvas.elements = canvas.undoStack.back();
        canvas.undoStack.pop_back();
        canvas.selectedIndices.clear();
      }
    }
    if (!canvas.isTextEditing && IsKeyPressed(KEY_X)) {
      vector<int> selectedIDs = GetSelectedIDs(canvas);
      if (!selectedIDs.empty()) {
        SaveBackup(canvas);
        RestoreZOrder(canvas);

        vector<int> sorted;
        for (int id : selectedIDs) {
          int idx = FindElementIndexByID(canvas, id);
          if (idx != -1)
            sorted.push_back(idx);
        }
        sort(sorted.begin(), sorted.end(), greater<int>());
        for (int idx : sorted) {
          if (idx >= 0 && idx < (int)canvas.elements.size())
            canvas.elements.erase(canvas.elements.begin() + idx);
        }
        canvas.selectedIndices.clear();
      }
    }

    // SELECTION MODE specific behavior
    if (canvas.mode == MOVE_MODE) {
      if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 delta = GetMouseDelta();
        canvas.camera.target.x -= delta.x / canvas.camera.zoom;
        canvas.camera.target.y -= delta.y / canvas.camera.zoom;
      }
    } else if (canvas.mode == SELECTION_MODE) {
      if (key >= KEY_ZERO && key <= KEY_NINE) {
        double currentTime = GetTime();
        int digit = key - KEY_ZERO;
        if (!canvas.isTypingNumber ||
            (currentTime - canvas.lastInputTime > 1.0)) {
          canvas.inputNumber = digit;
          canvas.isTypingNumber = true;
        } else {
          canvas.inputNumber = (canvas.inputNumber * 10 + digit);
        }
        canvas.lastInputTime = currentTime;

        // We use uniqueID for tagging and selection. Find element with that
        // uniqueID.
        RestoreZOrder(canvas);
        int foundIdx = FindElementIndexByID(canvas, canvas.inputNumber);
        if (foundIdx != -1) {
          SaveBackup(canvas);
          Element selected = canvas.elements[foundIdx];
          selected.originalIndex = foundIdx;
          canvas.elements.erase(canvas.elements.begin() + foundIdx);
          canvas.elements.push_back(selected);
          canvas.selectedIndices = {(int)canvas.elements.size() - 1};
        }
      }

      bool jPressed = IsKeyPressed(KEY_J);
      bool kPressed = IsKeyPressed(KEY_K);
      if ((jPressed || kPressed) && !canvas.elements.empty()) {
        vector<int> ids;
        ids.reserve(canvas.elements.size());
        for (const auto &el : canvas.elements) {
          if (el.uniqueID >= 0)
            ids.push_back(el.uniqueID);
        }
        sort(ids.begin(), ids.end());
        ids.erase(unique(ids.begin(), ids.end()), ids.end());
        if (ids.empty())
          continue;

        int currentID = -1;
        if (!canvas.selectedIndices.empty()) {
          int selIdx = canvas.selectedIndices[0];
          if (selIdx >= 0 && selIdx < (int)canvas.elements.size())
            currentID = canvas.elements[selIdx].uniqueID;
        }

        int targetID = ids[0];
        auto zeroIt = lower_bound(ids.begin(), ids.end(), 0);
        if (zeroIt != ids.end() && *zeroIt == 0)
          targetID = 0;

        auto it = find(ids.begin(), ids.end(), currentID);
        if (it != ids.end()) {
          int pos = (int)(it - ids.begin());
          if (jPressed) {
            pos = (pos + 1) % (int)ids.size();
          } else {
            pos = (pos == 0 ? (int)ids.size() - 1 : pos - 1);
          }
          targetID = ids[pos];
        } else {
          if (!jPressed && zeroIt != ids.end() && *zeroIt == 0)
            targetID = ids.back();
        }

        int targetIdx = FindElementIndexByID(canvas, targetID);
        if (targetIdx != -1)
          canvas.selectedIndices = {targetIdx};
        canvas.isTypingNumber = false;
      }

      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        canvas.startPoint = mouseWorld;
        canvas.currentMouse = mouseWorld;
        canvas.isDragging = true;
        bool hit = false;
        int hitIndex = -1;
        for (int i = (int)canvas.elements.size() - 1; i >= 0; i--) {
          Rectangle b = canvas.elements[i].GetBounds();
          Rectangle tagHit = {canvas.elements[i].start.x,
                              canvas.elements[i].start.y - 20, 20, 20};
          if (CheckCollisionPointRec(
                  canvas.startPoint,
                  {b.x - 5, b.y - 5, b.width + 10, b.height + 10}) ||
              CheckCollisionPointRec(canvas.startPoint, tagHit)) {
            hitIndex = i;
            hit = true;
            break;
          }
        }
        if (hit) {
          bool alreadySelected = false;
          for (int idx : canvas.selectedIndices)
            if (idx == hitIndex)
              alreadySelected = true;
          if (!alreadySelected) {
            RestoreZOrder(canvas);
            SaveBackup(canvas);
            Element selected = canvas.elements[hitIndex];
            selected.originalIndex = hitIndex;
            canvas.elements.erase(canvas.elements.begin() + hitIndex);
            canvas.elements.push_back(selected);
            canvas.selectedIndices = {(int)canvas.elements.size() - 1};
          } else
            SaveBackup(canvas);
          canvas.isBoxSelecting = false;
        } else {
          RestoreZOrder(canvas);
          canvas.isBoxSelecting = true;
        }
        canvas.hasMoved = false;
      }

      if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && canvas.isDragging) {
        Vector2 prevMouse = canvas.currentMouse;
        canvas.currentMouse = GetScreenToWorld2D(GetMousePosition(), canvas.camera);
        Vector2 delta = Vector2Subtract(canvas.currentMouse, prevMouse);
        if (canvas.isBoxSelecting) {
          canvas.selectedIndices.clear();
          Rectangle selectionBox = {
              min(canvas.startPoint.x, canvas.currentMouse.x),
              min(canvas.startPoint.y, canvas.currentMouse.y),
              abs(canvas.currentMouse.x - canvas.startPoint.x),
              abs(canvas.currentMouse.y - canvas.startPoint.y)};
          for (int i = 0; i < (int)canvas.elements.size(); i++) {
            if (CheckCollisionRecs(selectionBox,
                                   canvas.elements[i].GetBounds())) {
              canvas.selectedIndices.push_back(i);
              if (canvas.elements[i].originalIndex == -1)
                canvas.elements[i].originalIndex = i;
            }
          }
        } else if (!canvas.selectedIndices.empty() &&
                   (delta.x != 0 || delta.y != 0)) {
          canvas.hasMoved = true;
          for (int idx : canvas.selectedIndices) {
            if (idx >= 0 && idx < (int)canvas.elements.size())
              MoveElement(canvas.elements[idx], delta);
          }
        }
      }
      if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        if (!canvas.isBoxSelecting && !canvas.hasMoved &&
            !canvas.undoStack.empty())
          canvas.undoStack.pop_back();
        canvas.isDragging = false;
        canvas.isBoxSelecting = false;
      }
    } else if (canvas.mode == ERASER_MODE) {
      if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 m = mouseWorld;
        for (int i = (int)canvas.elements.size() - 1; i >= 0; i--) {
          Rectangle b = canvas.elements[i].GetBounds();
          if (CheckCollisionPointRec(
                  m, {b.x - 2, b.y - 2, b.width + 4, b.height + 4})) {
            SaveBackup(canvas);
            canvas.elements.erase(canvas.elements.begin() + i);
            canvas.selectedIndices.clear();
            break;
          }
        }
      }
    } else if (canvas.mode == TEXT_MODE) {
      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 m = mouseWorld;
        int hitIndex = -1;
        for (int i = (int)canvas.elements.size() - 1; i >= 0; i--) {
          if (canvas.elements[i].type != TEXT_MODE)
            continue;
          Rectangle b = canvas.elements[i].GetBounds();
          if (CheckCollisionPointRec(m, b)) {
            hitIndex = i;
            break;
          }
        }

        if (hitIndex != -1) {
          canvas.isTextEditing = true;
          canvas.editingIndex = hitIndex;
          canvas.editingOriginalText = canvas.elements[hitIndex].text;
          canvas.textBuffer = canvas.elements[hitIndex].text;
          canvas.textPos = canvas.elements[hitIndex].start;
          canvas.editingColor = canvas.elements[hitIndex].color;
          canvas.textEditBackedUp = false;
        } else {
          SaveBackup(canvas);
          Element newEl;
          newEl.type = TEXT_MODE;
          newEl.start = m;
          newEl.end = {m.x + 10.0f, m.y + canvas.textSize};
          newEl.strokeWidth = canvas.strokeWidth;
          newEl.color = canvas.modeColor;
          newEl.originalIndex = -1;
          newEl.uniqueID = canvas.nextElementId++;
          newEl.text = "";
          canvas.elements.push_back(newEl);

          canvas.textPos = m;
          canvas.isTextEditing = true;
          canvas.editingIndex = (int)canvas.elements.size() - 1;
          canvas.editingOriginalText.clear();
          canvas.textBuffer.clear();
          canvas.editingColor = canvas.modeColor;
          canvas.textEditBackedUp = true;
        }
      }

      if (canvas.isTextEditing) {
        bool changed = false;
        int ch = GetCharPressed();
        while (ch > 0) {
          if (ch >= 32 && ch < 127) {
            canvas.textBuffer.push_back((char)ch);
            changed = true;
          }
          ch = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE) && !canvas.textBuffer.empty()) {
          canvas.textBuffer.pop_back();
          changed = true;
        }

        if (changed) {
          if (!canvas.textEditBackedUp) {
            SaveBackup(canvas);
            canvas.textEditBackedUp = true;
          }
          if (canvas.editingIndex >= 0 &&
              canvas.editingIndex < (int)canvas.elements.size()) {
            Vector2 size =
                MeasureTextEx(canvas.font, canvas.textBuffer.c_str(),
                              canvas.textSize, 2);
            Element &el = canvas.elements[canvas.editingIndex];
            el.text = canvas.textBuffer;
            el.start = canvas.textPos;
            el.end = {canvas.textPos.x + max(10.0f, size.x),
                      canvas.textPos.y + max(canvas.textSize, size.y)};
          }
        }
      }
    } else {
      // Drawing modes (line, rect, circle, pen,...)
      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        canvas.startPoint = mouseWorld;
        canvas.currentMouse = canvas.startPoint;
        canvas.isDragging = true;
        if (canvas.mode == PEN_MODE) {
          canvas.currentPath.clear();
          canvas.currentPath.push_back(canvas.startPoint);
        }
      }
      if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && canvas.isDragging) {
        canvas.currentMouse = GetScreenToWorld2D(GetMousePosition(), canvas.camera);
        if (canvas.mode == PEN_MODE &&
            Vector2Distance(canvas.currentPath.back(), canvas.currentMouse) >
                2.0f)
          canvas.currentPath.push_back(canvas.currentMouse);
      }
      if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && canvas.isDragging) {
        canvas.isDragging = false;
        if (canvas.mode == PEN_MODE ||
            Vector2Distance(canvas.startPoint, canvas.currentMouse) > 1.0f) {
          SaveBackup(canvas);
          Element newEl;
          newEl.type = canvas.mode;
          newEl.start = canvas.startPoint;
          newEl.end = canvas.currentMouse;
          newEl.strokeWidth = canvas.strokeWidth;
          newEl.color = canvas.modeColor;
          newEl.originalIndex = -1;

          // Assign the permanent ID and increment the counter
          newEl.uniqueID = canvas.nextElementId++;

          if (canvas.mode == PEN_MODE)
            newEl.path = canvas.currentPath;
          canvas.elements.push_back(newEl);
        }
      }
    }

    // --- Drawing
    BeginDrawing();
    ClearBackground(WHITE);
    BeginMode2D(canvas.camera);

    for (size_t i = 0; i < canvas.elements.size(); i++) {
      if (canvas.mode == TEXT_MODE && canvas.isTextEditing &&
          (int)i == canvas.editingIndex)
        continue;
      DrawElement(canvas.elements[i], canvas.font, canvas.textSize);
      bool isSelected = false;
      for (int idx : canvas.selectedIndices)
        if (idx == (int)i)
          isSelected = true;
      if (canvas.mode == SELECTION_MODE && isSelected) {
        Rectangle b = canvas.elements[i].GetBounds();
        DrawRectangleLinesEx({b.x - 5, b.y - 5, b.width + 10, b.height + 10}, 2,
                             MAGENTA);
      }
      if (canvas.showTags) {
        // Stable displayed ID: uniqueID (always >= 0)
        int displayId = canvas.elements[i].uniqueID;
        if (displayId < 0) {
          // If somehow missing, ensure we assign one (defensive)
          Element &mut = const_cast<Element &>(canvas.elements[i]);
          EnsureUniqueIDRecursive(mut, canvas);
          displayId = mut.uniqueID;
        }

        // draw tag near element start
        DrawRectangle(canvas.elements[i].start.x,
                      canvas.elements[i].start.y - 20, 20, 20, YELLOW);
        DrawRectangleLines(canvas.elements[i].start.x,
                           canvas.elements[i].start.y - 20, 20, 20, BLACK);
        DrawText(TextFormat("%d", displayId), canvas.elements[i].start.x + 5,
                 canvas.elements[i].start.y - 15, 10, BLACK);
      }
    }

    if (canvas.isDragging) {
      if (canvas.mode == SELECTION_MODE && canvas.isBoxSelecting) {
        Rectangle box = {min(canvas.startPoint.x, canvas.currentMouse.x),
                         min(canvas.startPoint.y, canvas.currentMouse.y),
                         abs(canvas.currentMouse.x - canvas.startPoint.x),
                         abs(canvas.currentMouse.y - canvas.startPoint.y)};
        DrawRectangleRec(box, Fade(BLUE, 0.2f));
        DrawRectangleLinesEx(box, 1, BLUE);
      } else if (canvas.mode != SELECTION_MODE && canvas.mode != ERASER_MODE) {
        Vector2 m = GetScreenToWorld2D(GetMousePosition(), canvas.camera);
        Element preview;
        preview.type = canvas.mode;
        preview.start = canvas.startPoint;
        preview.end = m;
        preview.strokeWidth = canvas.strokeWidth;
        preview.color = Fade(canvas.modeColor, 0.5f);
        if (canvas.mode == PEN_MODE)
          preview.path = canvas.currentPath;
        DrawElement(preview, canvas.font, canvas.textSize);
      }
    }
    if (canvas.mode == ERASER_MODE)
      DrawCircleLinesV(GetScreenToWorld2D(GetMousePosition(), canvas.camera), 10,
                       ORANGE);
    if (canvas.mode == TEXT_MODE && canvas.isTextEditing) {
      DrawTextEx(canvas.font, canvas.textBuffer.c_str(), canvas.textPos,
                 canvas.textSize, 2, Fade(canvas.editingColor, 0.7f));
    }
    EndMode2D();
    DrawTextEx(canvas.font, "Current Mode:", {10, 10}, 24, 2, DARKGRAY);
    DrawTextEx(canvas.font, canvas.modeText, {180, 10}, 24, 2,
               canvas.modeColor);
    EndDrawing();
  }
  UnloadFont(canvas.font);
  CloseWindow();
  return 0;
}
