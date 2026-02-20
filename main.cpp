#include "raylib.h"
#include "raymath.h"
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
  const char *modeText;
  Color modeColor;
  vector<Element> elements;
  vector<Vector2> currentPath;
  bool showTags = false;
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
    ClearBackground(WHITE);

    // Update
    if (IsKeyPressed(KEY_S))
      canvas.mode = SELECTION_MODE;
    if (IsKeyPressed(KEY_L))
      canvas.mode = LINE_MODE;
    if (IsKeyPressed(KEY_C))
      canvas.mode = CIRCLE_MODE;
    if (IsKeyPressed(KEY_T))
      canvas.mode = TEXT_MODE;
    if (IsKeyPressed(KEY_R))
      canvas.mode = RECTANGLE_MODE;
    if (IsKeyPressed(KEY_P))
      canvas.mode = PEN_MODE;
    if (IsKeyPressed(KEY_F))
      canvas.showTags = !canvas.showTags;

    if (canvas.mode == LINE_MODE || canvas.mode == CIRCLE_MODE ||
        canvas.mode == RECTANGLE_MODE || canvas.mode == PEN_MODE) {
      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        canvas.startPoint = GetMousePosition();
        canvas.isDragging = true;

        if (canvas.mode == PEN_MODE) {
          canvas.currentPath.clear();
          canvas.currentPath.push_back(canvas.startPoint);
        }
      }

      if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && canvas.isDragging) {
        canvas.currentMouse = GetMousePosition();
        if (canvas.mode == PEN_MODE) {
          if (canvas.currentPath.empty() ||
              Vector2Distance(canvas.currentPath.back(), canvas.currentMouse) >
                  2.0f) {
            canvas.currentPath.push_back(canvas.currentMouse);
          }
        }
      }

      if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        canvas.isDragging = false;

        Element newElement = {canvas.mode, canvas.startPoint,
                              canvas.currentMouse, canvas.strokeWidth,
                              canvas.modeColor};

        if (canvas.mode == PEN_MODE) {
          newElement.path = canvas.currentPath;
        }

        canvas.elements.push_back(newElement);
      }
    }

    // Drawing
    BeginDrawing();
    DrawTextEx(canvas.font, "Current Mode:", {10, 10}, 24, 2, DARKGRAY);

    switch (canvas.mode) {
    case SELECTION_MODE:
      canvas.modeText = "SELECTION";
      canvas.modeColor = MAROON;
      break;
    case LINE_MODE:
      canvas.modeText = "LINE";
      canvas.modeColor = BLUE;
      break;
    case CIRCLE_MODE:
      canvas.modeText = "CIRCLE";
      canvas.modeColor = DARKGREEN;
      break;
    case TEXT_MODE:
      canvas.modeText = "TEXT";
      canvas.modeColor = PURPLE;
      break;
    case RECTANGLE_MODE:
      canvas.modeText = "RECTANGLE";
      canvas.modeColor = RED;
      break;
    case PEN_MODE:
      canvas.modeText = "PEN";
      canvas.modeColor = BLACK;
      break;
    }

    // Save drawing
    for (size_t i = 0; i < canvas.elements.size(); i++) {
      const Element &element = canvas.elements[i];

      if (element.type == LINE_MODE) {
        DrawLineEx(element.start, element.end, element.strokeWidth,
                   element.color);
      } else if (element.type == CIRCLE_MODE) {
        float radius = Vector2Distance(element.start, element.end);
        DrawRing(element.start, radius - element.strokeWidth / 2,
                 radius + element.strokeWidth / 2, 0, 360, 128, element.color);
      } else if (element.type == RECTANGLE_MODE) {
        float x = min(element.start.x, element.end.x);
        float y = min(element.start.y, element.end.y);
        float width = abs(element.end.x - element.start.x);
        float height = abs(element.end.y - element.start.y);
        DrawRectangleLinesEx({x, y, width, height}, element.strokeWidth,
                             element.color);
      } else if (element.type == PEN_MODE) {
        if (element.path.size() == 1) {
          DrawCircleV(element.path[0], element.strokeWidth / 2, element.color);
        } else if (element.path.size() > 1) {
          DrawSplineCatmullRom(element.path.data(), element.path.size(),
                               element.strokeWidth, element.color);
        }
      }

      // Tagging elements
      if (canvas.showTags) {
        DrawRectangle(element.start.x, element.start.y - 20, 20, 20, YELLOW);
        DrawRectangleLines(element.start.x, element.start.y - 20, 20, 20,
                           BLACK);
        DrawText(TextFormat("%d", i), element.start.x + 5, element.start.y - 15,
                 10, BLACK);
      }
    }

    DrawTextEx(canvas.font, canvas.modeText, {180, 10}, 24, 2,
               canvas.modeColor);

    // Preview drawing
    if (canvas.isDragging) {
      canvas.currentMouse = GetMousePosition();
      if (canvas.mode == LINE_MODE) {
        DrawLineEx(canvas.startPoint, canvas.currentMouse, canvas.strokeWidth,
                   canvas.modeColor);
      } else if (canvas.mode == CIRCLE_MODE) {
        float radius = Vector2Distance(canvas.startPoint, canvas.currentMouse);

        DrawRing(canvas.startPoint, radius - canvas.strokeWidth / 2,
                 radius + canvas.strokeWidth / 2, 0, 360, 128,
                 canvas.modeColor);
      } else if (canvas.mode == RECTANGLE_MODE) {
        float x = min(canvas.startPoint.x, canvas.currentMouse.x);
        float y = min(canvas.startPoint.y, canvas.currentMouse.y);

        float width = abs(canvas.currentMouse.x - canvas.startPoint.x);
        float height = abs(canvas.currentMouse.y - canvas.startPoint.y);

        Rectangle rect = {x, y, width, height};
        DrawRectangleLinesEx(rect, canvas.strokeWidth, canvas.modeColor);
      } else if (canvas.mode == PEN_MODE) {
        if (canvas.currentPath.size() > 1) {
          DrawSplineCatmullRom(canvas.currentPath.data(),
                               canvas.currentPath.size(), canvas.strokeWidth,
                               canvas.modeColor);
        }
      }
    }

    EndDrawing();
  }

  UnloadFont(canvas.font);
  CloseWindow();
  return 0;
}
