#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <vector>

using namespace std;

enum Mode {
  SELECTION_MODE,
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
};

struct Element {
  Mode type;
  Vector2 start;
  Vector2 end;
  float strokeWidth;
  Color color;
  vector<Vector2> path;
  int originalIndex = -1;

  Rectangle GetBounds() const {
    float minX, minY, maxX, maxY;
    if (type == PEN_MODE && !path.empty()) {
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
};

void SaveBackup(Canvas &canvas) {
  canvas.undoStack.push_back(canvas.elements);
  canvas.redoStack.clear();
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

void RestoreZOrder(Canvas &canvas) {
  if (canvas.selectedIndices.empty())
    return;
  vector<Element> selectedElements;
  for (int idx : canvas.selectedIndices)
    selectedElements.push_back(canvas.elements[idx]);
  sort(canvas.selectedIndices.begin(), canvas.selectedIndices.end(),
       greater<int>());
  for (int idx : canvas.selectedIndices)
    canvas.elements.erase(canvas.elements.begin() + idx);
  for (auto &el : selectedElements) {
    int target = el.originalIndex;
    if (target >= (int)canvas.elements.size())
      canvas.elements.push_back(el);
    else
      canvas.elements.insert(canvas.elements.begin() + target, el);
  }
  canvas.selectedIndices.clear();
}

int main() {
  const int screenWidth = 1000;
  const int screenHeight = 800;
  Canvas canvas;
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(screenWidth, screenHeight, "TOGGLE");
  SetTargetFPS(60);
  canvas.font = LoadFont("IosevkaNerdFontMono-Regular.ttf");
  SetTextureFilter(canvas.font.texture, TEXTURE_FILTER_BILINEAR);

  while (!WindowShouldClose()) {
    int key = GetKeyPressed();
    if (key == KEY_S) {
      canvas.mode = SELECTION_MODE;
      canvas.modeText = "SELECTION";
      canvas.modeColor = MAROON;
      canvas.isTypingNumber = false;
    }
    if (key == KEY_L) {
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
    if (key == KEY_C) {
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
    if (key == KEY_R) {
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
    if (key == KEY_P) {
      canvas.mode = PEN_MODE;
      canvas.modeText = "PEN";
      canvas.modeColor = BLACK;
      canvas.isTypingNumber = false;
    }
    if (key == KEY_E) {
      canvas.mode = ERASER_MODE;
      canvas.modeText = "ERASER";
      canvas.modeColor = ORANGE;
      canvas.isTypingNumber = false;
    }
    if (key == KEY_F)
      canvas.showTags = !canvas.showTags;
    if (key != 0)
      canvas.lastKey = key;

    if (IsKeyPressed(KEY_U)) {
      if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
        if (!canvas.redoStack.empty()) {
          canvas.undoStack.push_back(canvas.elements);
          canvas.elements = canvas.redoStack.back();
          canvas.redoStack.pop_back();
        }
      } else if (!canvas.undoStack.empty()) {
        canvas.redoStack.push_back(canvas.elements);
        canvas.elements = canvas.undoStack.back();
        canvas.undoStack.pop_back();
        canvas.selectedIndices.clear();
      }
    }

    if (canvas.mode == SELECTION_MODE) {
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
        RestoreZOrder(canvas);
        if (canvas.inputNumber < (int)canvas.elements.size()) {
          Element selected = canvas.elements[canvas.inputNumber];
          selected.originalIndex = canvas.inputNumber;
          canvas.elements.erase(canvas.elements.begin() + canvas.inputNumber);
          canvas.elements.push_back(selected);
          canvas.selectedIndices = {(int)canvas.elements.size() - 1};
        }
      }

      if ((IsKeyPressed(KEY_J) || IsKeyPressed(KEY_K)) &&
          !canvas.elements.empty()) {
        int currentOrig = -1;
        if (!canvas.selectedIndices.empty()) {
          currentOrig =
              canvas.elements[canvas.selectedIndices[0]].originalIndex;
          if (currentOrig == -1)
            currentOrig = canvas.selectedIndices[0];
        }
        RestoreZOrder(canvas);
        int nextIdx = (IsKeyPressed(KEY_J))
                          ? (currentOrig + 1) % (int)canvas.elements.size()
                          : (currentOrig <= 0 ? (int)canvas.elements.size() - 1
                                              : currentOrig - 1);
        Element selected = canvas.elements[nextIdx];
        selected.originalIndex = nextIdx;
        canvas.elements.erase(canvas.elements.begin() + nextIdx);
        canvas.elements.push_back(selected);
        canvas.selectedIndices = {(int)canvas.elements.size() - 1};
        canvas.isTypingNumber = false;
      }

      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        canvas.startPoint = GetMousePosition();
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
        canvas.currentMouse = GetMousePosition();
        Vector2 delta = GetMouseDelta();
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
            Element &el = canvas.elements[idx];
            el.start = Vector2Add(el.start, delta);
            el.end = Vector2Add(el.end, delta);
            for (auto &p : el.path)
              p = Vector2Add(p, delta);
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
        Vector2 m = GetMousePosition();
        for (int i = (int)canvas.elements.size() - 1; i >= 0; i--) {
          Rectangle b = canvas.elements[i].GetBounds();
          if (CheckCollisionPointRec(
                  m, {b.x - 2, b.y - 2, b.width + 4, b.height + 4})) {
            SaveBackup(canvas);
            canvas.elements.erase(canvas.elements.begin() + i);
            break;
          }
        }
      }
    } else {
      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        canvas.startPoint = GetMousePosition();
        canvas.isDragging = true;
        if (canvas.mode == PEN_MODE) {
          canvas.currentPath.clear();
          canvas.currentPath.push_back(canvas.startPoint);
        }
      }
      if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && canvas.isDragging) {
        canvas.currentMouse = GetMousePosition();
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
          Element newEl = {canvas.mode, canvas.startPoint, canvas.currentMouse,
                           canvas.strokeWidth, canvas.modeColor};
          if (canvas.mode == PEN_MODE)
            newEl.path = canvas.currentPath;
          canvas.elements.push_back(newEl);
        }
      }
    }

    BeginDrawing();
    ClearBackground(WHITE);
    for (size_t i = 0; i < canvas.elements.size(); i++) {
      const Element &el = canvas.elements[i];
      if (el.type == LINE_MODE)
        DrawLineEx(el.start, el.end, el.strokeWidth, el.color);
      else if (el.type == DOTTEDLINE_MODE)
        DrawDashedLine(el.start, el.end, el.strokeWidth, el.color);
      else if (el.type == ARROWLINE_MODE)
        DrawArrowLine(el.start, el.end, el.strokeWidth, el.color);
      else if (el.type == CIRCLE_MODE)
        DrawRing(el.start,
                 Vector2Distance(el.start, el.end) - el.strokeWidth / 2,
                 Vector2Distance(el.start, el.end) + el.strokeWidth / 2, 0, 360,
                 60, el.color);
      else if (el.type == DOTTEDCIRCLE_MODE)
        DrawDashedRing(el.start, Vector2Distance(el.start, el.end),
                       el.strokeWidth, el.color);
      else if (el.type == RECTANGLE_MODE)
        DrawRectangleLinesEx(
            {min(el.start.x, el.end.x), min(el.start.y, el.end.y),
             abs(el.end.x - el.start.x), abs(el.end.y - el.start.y)},
            el.strokeWidth, el.color);
      else if (el.type == DOTTEDRECT_MODE) {
        Rectangle r = {min(el.start.x, el.end.x), min(el.start.y, el.end.y),
                       abs(el.end.x - el.start.x), abs(el.end.y - el.start.y)};
        DrawDashedLine({r.x, r.y}, {r.x + r.width, r.y}, el.strokeWidth,
                       el.color);
        DrawDashedLine({r.x + r.width, r.y}, {r.x + r.width, r.y + r.height},
                       el.strokeWidth, el.color);
        DrawDashedLine({r.x + r.width, r.y + r.height}, {r.x, r.y + r.height},
                       el.strokeWidth, el.color);
        DrawDashedLine({r.x, r.y + r.height}, {r.x, r.y}, el.strokeWidth,
                       el.color);
      } else if (el.type == PEN_MODE) {
        if (el.path.size() == 1)
          DrawCircleV(el.path[0], el.strokeWidth / 2, el.color);
        else
          DrawSplineCatmullRom(el.path.data(), (int)el.path.size(),
                               el.strokeWidth, el.color);
      }
      bool isSelected = false;
      for (int idx : canvas.selectedIndices)
        if (idx == (int)i)
          isSelected = true;
      if (canvas.mode == SELECTION_MODE && isSelected) {
        Rectangle b = el.GetBounds();
        DrawRectangleLinesEx({b.x - 5, b.y - 5, b.width + 10, b.height + 10}, 2,
                             MAGENTA);
      }
      if (canvas.showTags) {
        int displayIndex = (el.originalIndex != -1) ? el.originalIndex : (int)i;
        DrawRectangle(el.start.x, el.start.y - 20, 20, 20, YELLOW);
        DrawRectangleLines(el.start.x, el.start.y - 20, 20, 20, BLACK);
        DrawText(TextFormat("%d", displayIndex), el.start.x + 5,
                 el.start.y - 15, 10, BLACK);
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
        Vector2 m = GetMousePosition();
        if (canvas.mode == LINE_MODE)
          DrawLineEx(canvas.startPoint, m, canvas.strokeWidth,
                     Fade(canvas.modeColor, 0.5f));
        else if (canvas.mode == DOTTEDLINE_MODE)
          DrawDashedLine(canvas.startPoint, m, canvas.strokeWidth,
                         Fade(canvas.modeColor, 0.5f));
        else if (canvas.mode == ARROWLINE_MODE)
          DrawArrowLine(canvas.startPoint, m, canvas.strokeWidth,
                        Fade(canvas.modeColor, 0.5f));
        else if (canvas.mode == CIRCLE_MODE)
          DrawRing(
              canvas.startPoint,
              Vector2Distance(canvas.startPoint, m) - canvas.strokeWidth / 2,
              Vector2Distance(canvas.startPoint, m) + canvas.strokeWidth / 2, 0,
              360, 60, Fade(canvas.modeColor, 0.5f));
        else if (canvas.mode == DOTTEDCIRCLE_MODE)
          DrawDashedRing(canvas.startPoint,
                         Vector2Distance(canvas.startPoint, m),
                         canvas.strokeWidth, Fade(canvas.modeColor, 0.5f));
        else if (canvas.mode == RECTANGLE_MODE)
          DrawRectangleLinesEx(
              {min(canvas.startPoint.x, m.x), min(canvas.startPoint.y, m.y),
               abs(m.x - canvas.startPoint.x), abs(m.y - canvas.startPoint.y)},
              canvas.strokeWidth, Fade(canvas.modeColor, 0.5f));
        else if (canvas.mode == DOTTEDRECT_MODE) {
          Rectangle r = {
              min(canvas.startPoint.x, m.x), min(canvas.startPoint.y, m.y),
              abs(m.x - canvas.startPoint.x), abs(m.y - canvas.startPoint.y)};
          DrawDashedLine({r.x, r.y}, {r.x + r.width, r.y}, canvas.strokeWidth,
                         Fade(canvas.modeColor, 0.5f));
          DrawDashedLine({r.x + r.width, r.y}, {r.x + r.width, r.y + r.height},
                         canvas.strokeWidth, Fade(canvas.modeColor, 0.5f));
          DrawDashedLine({r.x + r.width, r.y + r.height}, {r.x, r.y + r.height},
                         canvas.strokeWidth, Fade(canvas.modeColor, 0.5f));
          DrawDashedLine({r.x, r.y + r.height}, {r.x, r.y}, canvas.strokeWidth,
                         Fade(canvas.modeColor, 0.5f));
        } else if (canvas.mode == PEN_MODE && canvas.currentPath.size() > 1)
          DrawSplineCatmullRom(canvas.currentPath.data(),
                               (int)canvas.currentPath.size(),
                               canvas.strokeWidth, canvas.modeColor);
      }
    }
    if (canvas.mode == ERASER_MODE) {
      DrawCircleLinesV(GetMousePosition(), 10, ORANGE);
    }
    DrawTextEx(canvas.font, "Current Mode:", {10, 10}, 24, 2, DARKGRAY);
    DrawTextEx(canvas.font, canvas.modeText, {180, 10}, 24, 2,
               canvas.modeColor);
    EndDrawing();
  }
  UnloadFont(canvas.font);
  CloseWindow();
  return 0;
}
