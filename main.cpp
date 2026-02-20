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
                                   ? (int)canvas.elements.size() - 1
                                   : canvas.selectedIndex - 1;
      }

      // Mouse Selection
      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mousePos = GetMousePosition();
        canvas.selectedIndex = -1;
        for (int i = (int)canvas.elements.size() - 1; i >= 0; i--) {
          Element &element = canvas.elements[i];
          bool isHit = false;

          if (element.type == CIRCLE_MODE) {
            float radius = Vector2Distance(element.start, element.end);
            if (CheckCollisionPointCircle(mousePos, element.start, radius))
              isHit = true;
          } else if (element.type == PEN_MODE) {
            float minX = element.start.x, minY = element.start.y,
                  maxX = element.start.x, maxY = element.start.y;
            for (auto &p : element.path) {
              minX = min(minX, p.x);
              minY = min(minY, p.y);
              maxX = max(maxX, p.x);
              maxY = max(maxY, p.y);
            }
            if (CheckCollisionPointRec(mousePos, {minX - 10, minY - 10,
                                                  (maxX - minX) + 20,
                                                  (maxY - minY) + 20}))
              isHit = true;
          } else {
            float minX = min(element.start.x, element.end.x);
            float minY = min(element.start.y, element.end.y);
            float maxX = max(element.start.x, element.end.x);
            float maxY = max(element.start.y, element.end.y);
            if (CheckCollisionPointRec(mousePos, {minX - 10, minY - 10,
                                                  (maxX - minX) + 20,
                                                  (maxY - minY) + 20}))
              isHit = true;
          }

          if (isHit) {
            canvas.selectedIndex = i;
            canvas.isDragging = true;
            break;
          }
        }
      }

      // Dragging Logic
      if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && canvas.isDragging &&
          canvas.selectedIndex != -1) {
        Vector2 delta = GetMouseDelta();
        Element &selectedElement = canvas.elements[canvas.selectedIndex];
        selectedElement.start = Vector2Add(selectedElement.start, delta);
        selectedElement.end = Vector2Add(selectedElement.end, delta);
        for (auto &p : selectedElement.path)
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

        //  Prevent zero-size ghost elements
        float dist = Vector2Distance(canvas.startPoint, canvas.currentMouse);
        if (canvas.mode == PEN_MODE || dist > 1.0f) {
          Element newElement = {canvas.mode, canvas.startPoint,
                                canvas.currentMouse, canvas.strokeWidth,
                                canvas.modeColor};
          if (canvas.mode == PEN_MODE)
            newElement.path = canvas.currentPath;
          canvas.elements.push_back(newElement);
        }
      }
    }

    // Drawing
    BeginDrawing();
    ClearBackground(WHITE);

    for (size_t i = 0; i < canvas.elements.size(); i++) {
      const Element &element = canvas.elements[i];

      if (element.type == LINE_MODE)
        DrawLineEx(element.start, element.end, element.strokeWidth,
                   element.color);
      else if (element.type == CIRCLE_MODE) {
        float r = Vector2Distance(element.start, element.end);
        DrawRing(element.start, r - element.strokeWidth / 2,
                 r + element.strokeWidth / 2, 0, 360, 60, element.color);
      } else if (element.type == RECTANGLE_MODE) {
        DrawRectangleLinesEx({min(element.start.x, element.end.x),
                              min(element.start.y, element.end.y),
                              abs(element.end.x - element.start.x),
                              abs(element.end.y - element.start.y)},
                             element.strokeWidth, element.color);
      } else if (element.type == PEN_MODE) {
        if (element.path.size() == 1)
          DrawCircleV(element.path[0], element.strokeWidth / 2, element.color);
        else
          DrawSplineCatmullRom(element.path.data(), (int)element.path.size(),
                               element.strokeWidth, element.color);
      }

      // SELECTION HIGHLIGHT
      if (canvas.mode == SELECTION_MODE && (int)i == canvas.selectedIndex) {
        Rectangle bounds;
        if (element.type == CIRCLE_MODE) {
          float r = Vector2Distance(element.start, element.end);
          bounds = {element.start.x - r - 8, element.start.y - r - 8,
                    (r * 2) + 16, (r * 2) + 16};
        } else if (element.type == PEN_MODE) {
          float minX = element.start.x, minY = element.start.y,
                maxX = element.start.x, maxY = element.start.y;
          for (auto &p : element.path) {
            minX = min(minX, p.x);
            minY = min(minY, p.y);
            maxX = max(maxX, p.x);
            maxY = max(maxY, p.y);
          }
          bounds = {minX - 8, minY - 8, (maxX - minX) + 16, (maxY - minY) + 16};
        } else {
          float minX = min(element.start.x, element.end.x);
          float minY = min(element.start.y, element.end.y);
          float maxX = max(element.start.x, element.end.x);
          float maxY = max(element.start.y, element.end.y);
          bounds = {minX - 8, minY - 8, (maxX - minX) + 16, (maxY - minY) + 16};
        }
        DrawRectangleLinesEx(bounds, 2, MAGENTA);
      }

      if (canvas.showTags) {
        DrawRectangle(element.start.x, element.start.y - 20, 20, 20, YELLOW);
        DrawRectangleLines(element.start.x, element.start.y - 20, 20, 20,
                           BLACK);
        DrawText(TextFormat("%d", i), element.start.x + 5, element.start.y - 15,
                 10, BLACK);
      }
    }

    // Preview
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
