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
  vector<Vector2> currentPath;
  bool showTags = false;
  int selectedIndex = -1;
  bool isTypingNumber = false;
  int inputNumber = 0;
};

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
    // Update
    // Mode Switching
    if (IsKeyPressed(KEY_S)) {
      canvas.mode = SELECTION_MODE;
      canvas.modeText = "SELECTION";
      canvas.modeColor = MAROON;
    }
    if (IsKeyPressed(KEY_L)) {
      canvas.mode = LINE_MODE;
      canvas.modeText = "LINE";
      canvas.modeColor = BLUE;
    }
    if (IsKeyPressed(KEY_C)) {
      canvas.mode = CIRCLE_MODE;
      canvas.modeText = "CIRCLE";
      canvas.modeColor = DARKGREEN;
    }
    if (IsKeyPressed(KEY_T)) {
      canvas.mode = TEXT_MODE;
      canvas.modeText = "TEXT";
      canvas.modeColor = PURPLE;
    }
    if (IsKeyPressed(KEY_R)) {
      canvas.mode = RECTANGLE_MODE;
      canvas.modeText = "RECTANGLE";
      canvas.modeColor = RED;
    }
    if (IsKeyPressed(KEY_P)) {
      canvas.mode = PEN_MODE;
      canvas.modeText = "PEN";
      canvas.modeColor = BLACK;
    }
    if (IsKeyPressed(KEY_F))
      canvas.showTags = !canvas.showTags;

    if (canvas.mode == SELECTION_MODE) {
      // Keyboard Selection
      int key = GetKeyPressed();
      if (key >= KEY_ZERO && key <= KEY_NINE) {
        int digit = key - KEY_ZERO;
        if (!canvas.isTypingNumber) {
          canvas.inputNumber = digit;
          canvas.isTypingNumber = true;
        } else {
          int potential = canvas.inputNumber * 10 + digit;
          canvas.inputNumber =
              (potential < (int)canvas.elements.size()) ? potential : digit;
        }
        if (canvas.inputNumber < (int)canvas.elements.size())
          canvas.selectedIndex = canvas.inputNumber;
      }

      if (IsKeyPressed(KEY_J) && !canvas.elements.empty()) {
        canvas.isTypingNumber = false;
        canvas.selectedIndex =
            (canvas.selectedIndex + 1) % canvas.elements.size();
      }
      if (IsKeyPressed(KEY_K) && !canvas.elements.empty()) {
        canvas.isTypingNumber = false;
        canvas.selectedIndex = (canvas.selectedIndex <= 0)
                                   ? canvas.elements.size() - 1
                                   : canvas.selectedIndex - 1;
      }

      // Mouse Selection (Clicking an element)
      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mPos = GetMousePosition();
        canvas.selectedIndex = -1;

        // Iterate backwards to select the "top" element first
        for (int i = (int)canvas.elements.size() - 1; i >= 0; i--) {
          Element &el = canvas.elements[i];
          bool isHit = false;

          if (el.type == CIRCLE_MODE) {
            // Check distance from center (start) to mouse
            float radius = Vector2Distance(el.start, el.end);
            if (CheckCollisionPointCircle(mPos, el.start, radius)) {
              isHit = true;
            }
          } else if (el.type == PEN_MODE) {
            // Check bounding box of the whole path
            float minX = el.start.x, minY = el.start.y, maxX = el.start.x,
                  maxY = el.start.y;
            for (auto &p : el.path) {
              minX = min(minX, p.x);
              minY = min(minY, p.y);
              maxX = max(maxX, p.x);
              maxY = max(maxY, p.y);
            }
            if (CheckCollisionPointRec(mPos, {minX - 10, minY - 10,
                                              (maxX - minX) + 20,
                                              (maxY - minY) + 20})) {
              isHit = true;
            }
          } else {
            // Lines and Rectangles
            float minX = min(el.start.x, el.end.x);
            float minY = min(el.start.y, el.end.y);
            float maxX = max(el.start.x, el.end.x);
            float maxY = max(el.start.y, el.end.y);
            if (CheckCollisionPointRec(mPos, {minX - 10, minY - 10,
                                              (maxX - minX) + 20,
                                              (maxY - minY) + 20})) {
              isHit = true;
            }
          }

          if (isHit) {
            canvas.selectedIndex = i;
            canvas.isDragging = true;
            break;
          }
        }
      }
      if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && canvas.isDragging &&
          canvas.selectedIndex != -1) {
        Vector2 delta = GetMouseDelta();
        Element &el = canvas.elements[canvas.selectedIndex];
        el.start = Vector2Add(el.start, delta);
        el.end = Vector2Add(el.end, delta);
        for (auto &p : el.path)
          p = Vector2Add(p, delta);
      }
      if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
        canvas.isDragging = false;

    } else {
      // Drawing logic (Non-selection modes)
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
        if (canvas.mode == PEN_MODE) {
          if (Vector2Distance(canvas.currentPath.back(), canvas.currentMouse) >
              2.0f) {
            canvas.currentPath.push_back(canvas.currentMouse);
          }
        }
      }
      if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && canvas.isDragging) {
        canvas.isDragging = false;
        Element newElement = {canvas.mode, canvas.startPoint,
                              canvas.currentMouse, canvas.strokeWidth,
                              canvas.modeColor};
        if (canvas.mode == PEN_MODE)
          newElement.path = canvas.currentPath;
        canvas.elements.push_back(newElement);
      }
    }

    // Drawing
    BeginDrawing();
    ClearBackground(WHITE);

    // Draw saved elements
    for (size_t i = 0; i < canvas.elements.size(); i++) {
      const Element &el = canvas.elements[i];

      if (el.type == LINE_MODE)
        DrawLineEx(el.start, el.end, el.strokeWidth, el.color);
      else if (el.type == CIRCLE_MODE) {
        float r = Vector2Distance(el.start, el.end);
        DrawRing(el.start, r - el.strokeWidth / 2, r + el.strokeWidth / 2, 0,
                 360, 60, el.color);
      } else if (el.type == RECTANGLE_MODE) {
        DrawRectangleLinesEx(
            {min(el.start.x, el.end.x), min(el.start.y, el.end.y),
             abs(el.end.x - el.start.x), abs(el.end.y - el.start.y)},
            el.strokeWidth, el.color);
      } else if (el.type == PEN_MODE) {
        if (el.path.size() == 1)
          DrawCircleV(el.path[0], el.strokeWidth / 2, el.color);
        else
          DrawSplineCatmullRom(el.path.data(), (int)el.path.size(),
                               el.strokeWidth, el.color);
      }

      // SELECTION HIGHLIGHT
      if (canvas.mode == SELECTION_MODE && (int)i == canvas.selectedIndex) {
        Rectangle bounds;

        if (el.type == CIRCLE_MODE) {
          float r = Vector2Distance(el.start, el.end);
          bounds = {el.start.x - r - 8, el.start.y - r - 8, (r * 2) + 16,
                    (r * 2) + 16};
        } else if (el.type == PEN_MODE) {
          float minX = el.start.x, minY = el.start.y, maxX = el.start.x,
                maxY = el.start.y;
          for (auto &p : el.path) {
            minX = min(minX, p.x);
            minY = min(minY, p.y);
            maxX = max(maxX, p.x);
            maxY = max(maxY, p.y);
          }
          bounds = {minX - 8, minY - 8, (maxX - minX) + 16, (maxY - minY) + 16};
        } else {
          float minX = min(el.start.x, el.end.x);
          float minY = min(el.start.y, el.end.y);
          float maxX = max(el.start.x, el.end.x);
          float maxY = max(el.start.y, el.end.y);
          bounds = {minX - 8, minY - 8, (maxX - minX) + 16, (maxY - minY) + 16};
        }

        DrawRectangleLinesEx(bounds, 2, MAGENTA);
      }
      if (canvas.showTags) {
        DrawRectangle(el.start.x, el.start.y - 20, 20, 20, YELLOW);
        DrawRectangleLines(el.start.x, el.start.y - 20, 20, 20, BLACK);
        DrawText(TextFormat("%d", i), el.start.x + 5, el.start.y - 15, 10,
                 BLACK);
      }
    }

    // Preview Drawing
    if (canvas.isDragging && canvas.mode != SELECTION_MODE) {
      Vector2 m = GetMousePosition();
      if (canvas.mode == LINE_MODE)
        DrawLineEx(canvas.startPoint, m, canvas.strokeWidth,
                   Fade(canvas.modeColor, 0.5f));
      else if (canvas.mode == CIRCLE_MODE) {
        float r = Vector2Distance(canvas.startPoint, m);
        DrawRing(canvas.startPoint, r - canvas.strokeWidth / 2,
                 r + canvas.strokeWidth / 2, 0, 360, 60,
                 Fade(canvas.modeColor, 0.5f));
      } else if (canvas.mode == RECTANGLE_MODE) {
        DrawRectangleLinesEx(
            {min(canvas.startPoint.x, m.x), min(canvas.startPoint.y, m.y),
             abs(m.x - canvas.startPoint.x), abs(m.y - canvas.startPoint.y)},
            canvas.strokeWidth, Fade(canvas.modeColor, 0.5f));
      } else if (canvas.mode == PEN_MODE && canvas.currentPath.size() > 1) {
        DrawSplineCatmullRom(canvas.currentPath.data(),
                             (int)canvas.currentPath.size(), canvas.strokeWidth,
                             canvas.modeColor);
      }
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
