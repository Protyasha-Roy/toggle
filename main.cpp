#include "raylib.h"
#include "raymath.h"

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

struct Canvas {
  Mode mode = SELECTION_MODE;
  float strokeWidth = 2.0f;
  Vector2 startPoint = {0};
  Vector2 currentMouse = {0};
  bool isDragging = false;
  Font font;
  const char *modeText;
  Color modeColor;
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

    if (canvas.mode == LINE_MODE || canvas.mode == CIRCLE_MODE) {
      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        canvas.startPoint = GetMousePosition();
        canvas.isDragging = true;
      }

      if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        canvas.isDragging = false;
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
    }

    DrawTextEx(canvas.font, canvas.modeText, {180, 10}, 24, 2,
               canvas.modeColor);

    if (canvas.isDragging) {
      canvas.currentMouse = GetMousePosition();
      if (canvas.mode == LINE_MODE) {
        DrawLineEx(canvas.startPoint, canvas.currentMouse, canvas.strokeWidth,
                   BLACK);
      } else if (canvas.mode == CIRCLE_MODE) {
          float radius = Vector2Distance(canvas.startPoint, canvas.currentMouse);

          DrawRing(canvas.startPoint, radius - canvas.strokeWidth/2, radius + canvas.strokeWidth/2, 0, 360, 128, BLACK);
      }
    }

    EndDrawing();
  }

  UnloadFont(canvas.font);
  CloseWindow();
  return 0;
}
