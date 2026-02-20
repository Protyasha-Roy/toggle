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

    Rectangle GetBounds() const {
        float minX, minY, maxX, maxY;
        if (type == PEN_MODE && !path.empty()) {
            minX = maxX = path[0].x; minY = maxY = path[0].y;
            for (auto &p : path) {
                minX = min(minX, p.x); minY = min(minY, p.y);
                maxX = max(maxX, p.x); maxY = max(maxY, p.y);
            }
        } else if (type == CIRCLE_MODE) {
            float r = Vector2Distance(start, end);
            minX = start.x - r; minY = start.y - r;
            maxX = start.x + r; maxY = start.y + r;
        } else {
            minX = min(start.x, end.x); minY = min(start.y, end.y);
            maxX = max(start.x, end.x); maxY = max(start.y, end.y);
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
    bool hasMoved = false;
    bool isBoxSelecting = false;
};

void SaveBackup(Canvas &canvas) {
    canvas.undoStack.push_back(canvas.elements);
    canvas.redoStack.clear();
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
        if (IsKeyPressed(KEY_S)) { canvas.mode = SELECTION_MODE; canvas.modeText = "SELECTION"; canvas.modeColor = MAROON; }
        if (IsKeyPressed(KEY_L)) { canvas.mode = LINE_MODE; canvas.modeText = "LINE"; canvas.modeColor = BLUE; }
        if (IsKeyPressed(KEY_C)) { canvas.mode = CIRCLE_MODE; canvas.modeText = "CIRCLE"; canvas.modeColor = DARKGREEN; }
        if (IsKeyPressed(KEY_R)) { canvas.mode = RECTANGLE_MODE; canvas.modeText = "RECTANGLE"; canvas.modeColor = RED; }
        if (IsKeyPressed(KEY_P)) { canvas.mode = PEN_MODE; canvas.modeText = "PEN"; canvas.modeColor = BLACK; }
        if (IsKeyPressed(KEY_F)) canvas.showTags = !canvas.showTags;

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
            int key = GetKeyPressed();
            if (key >= KEY_ZERO && key <= KEY_NINE) {
                int digit = key - KEY_ZERO;
                if (!canvas.isTypingNumber) { canvas.inputNumber = digit; canvas.isTypingNumber = true; }
                else { canvas.inputNumber = (canvas.inputNumber * 10 + digit) % (int)canvas.elements.size(); }
                canvas.selectedIndices = {canvas.inputNumber};
            }

            if (IsKeyPressed(KEY_J) && !canvas.elements.empty()) {
                canvas.isTypingNumber = false;
                int current = canvas.selectedIndices.empty() ? -1 : canvas.selectedIndices[0];
                int next = (current + 1) % (int)canvas.elements.size();
                canvas.selectedIndices = {next};
            }
            if (IsKeyPressed(KEY_K) && !canvas.elements.empty()) {
                canvas.isTypingNumber = false;
                int current = canvas.selectedIndices.empty() ? 0 : canvas.selectedIndices[0];
                int prev = (current <= 0) ? (int)canvas.elements.size() - 1 : current - 1;
                canvas.selectedIndices = {prev};
            }

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                canvas.startPoint = GetMousePosition();
                canvas.isDragging = true;
                bool hitAnything = false;

                for (int i = (int)canvas.elements.size() - 1; i >= 0; i--) {
                    Rectangle b = canvas.elements[i].GetBounds();
                    if (CheckCollisionPointRec(canvas.startPoint, {b.x-5, b.y-5, b.width+10, b.height+10})) {
                        if (find(canvas.selectedIndices.begin(), canvas.selectedIndices.end(), i) == canvas.selectedIndices.end()) {
                            canvas.selectedIndices = {i};
                        }
                        hitAnything = true;
                        canvas.isBoxSelecting = false;
                        SaveBackup(canvas);
                        break;
                    }
                }

                if (!hitAnything) {
                    canvas.selectedIndices.clear();
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
                        abs(canvas.currentMouse.y - canvas.startPoint.y)
                    };
                    for (int i = 0; i < (int)canvas.elements.size(); i++) {
                        if (CheckCollisionRecs(selectionBox, canvas.elements[i].GetBounds())) {
                            canvas.selectedIndices.push_back(i);
                        }
                    }
                } else if (!canvas.selectedIndices.empty()) {
                    if (delta.x != 0 || delta.y != 0) {
                        canvas.hasMoved = true;
                        for (int idx : canvas.selectedIndices) {
                            Element &el = canvas.elements[idx];
                            el.start = Vector2Add(el.start, delta);
                            el.end = Vector2Add(el.end, delta);
                            for (auto &p : el.path) p = Vector2Add(p, delta);
                        }
                    }
                }
            }

            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                if (!canvas.isBoxSelecting && !canvas.hasMoved && !canvas.undoStack.empty()) {
                    canvas.undoStack.pop_back();
                }
                canvas.isDragging = false;
                canvas.isBoxSelecting = false;
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
                if (canvas.mode == PEN_MODE && Vector2Distance(canvas.currentPath.back(), canvas.currentMouse) > 2.0f) {
                    canvas.currentPath.push_back(canvas.currentMouse);
                }
            }
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && canvas.isDragging) {
                canvas.isDragging = false;
                float dist = Vector2Distance(canvas.startPoint, canvas.currentMouse);
                if (canvas.mode == PEN_MODE || dist > 1.0f) {
                    SaveBackup(canvas);
                    Element newElement = {canvas.mode, canvas.startPoint, canvas.currentMouse, canvas.strokeWidth, canvas.modeColor};
                    if (canvas.mode == PEN_MODE) newElement.path = canvas.currentPath;
                    canvas.elements.push_back(newElement);
                }
            }
        }

        BeginDrawing();
        ClearBackground(WHITE);

        for (size_t i = 0; i < canvas.elements.size(); i++) {
            const Element &el = canvas.elements[i];
            if (el.type == LINE_MODE) DrawLineEx(el.start, el.end, el.strokeWidth, el.color);
            else if (el.type == CIRCLE_MODE) {
                float r = Vector2Distance(el.start, el.end);
                DrawRing(el.start, r - el.strokeWidth / 2, r + el.strokeWidth / 2, 0, 360, 60, el.color);
            } else if (el.type == RECTANGLE_MODE) {
                DrawRectangleLinesEx({min(el.start.x, el.end.x), min(el.start.y, el.end.y), abs(el.end.x - el.start.x), abs(el.end.y - el.start.y)}, el.strokeWidth, el.color);
            } else if (el.type == PEN_MODE) {
                if (el.path.size() == 1) DrawCircleV(el.path[0], el.strokeWidth / 2, el.color);
                else DrawSplineCatmullRom(el.path.data(), (int)el.path.size(), el.strokeWidth, el.color);
            }

            if (canvas.mode == SELECTION_MODE && find(canvas.selectedIndices.begin(), canvas.selectedIndices.end(), (int)i) != canvas.selectedIndices.end()) {
                Rectangle b = el.GetBounds();
                DrawRectangleLinesEx({b.x - 5, b.y - 5, b.width + 10, b.height + 10}, 2, MAGENTA);
            }

            if (canvas.showTags) {
                DrawRectangle(el.start.x, el.start.y - 20, 20, 20, YELLOW);
                DrawRectangleLines(el.start.x, el.start.y - 20, 20, 20, BLACK);
                DrawText(TextFormat("%d", (int)i), el.start.x + 5, el.start.y - 15, 10, BLACK);
            }
        }

        if (canvas.isDragging) {
            if (canvas.mode == SELECTION_MODE && canvas.isBoxSelecting) {
                Rectangle box = { min(canvas.startPoint.x, canvas.currentMouse.x), min(canvas.startPoint.y, canvas.currentMouse.y), abs(canvas.currentMouse.x - canvas.startPoint.x), abs(canvas.currentMouse.y - canvas.startPoint.y) };
                DrawRectangleRec(box, Fade(BLUE, 0.2f));
                DrawRectangleLinesEx(box, 1, BLUE);
            } else if (canvas.mode != SELECTION_MODE) {
                Vector2 m = GetMousePosition();
                if (canvas.mode == LINE_MODE) DrawLineEx(canvas.startPoint, m, canvas.strokeWidth, Fade(canvas.modeColor, 0.5f));
                else if (canvas.mode == CIRCLE_MODE) {
                    float r = Vector2Distance(canvas.startPoint, m);
                    DrawRing(canvas.startPoint, r - canvas.strokeWidth/2, r + canvas.strokeWidth/2, 0, 360, 60, Fade(canvas.modeColor, 0.5f));
                } else if (canvas.mode == RECTANGLE_MODE) {
                    DrawRectangleLinesEx({min(canvas.startPoint.x, m.x), min(canvas.startPoint.y, m.y), abs(m.x - canvas.startPoint.x), abs(m.y - canvas.startPoint.y)}, canvas.strokeWidth, Fade(canvas.modeColor, 0.5f));
                } else if (canvas.mode == PEN_MODE && canvas.currentPath.size() > 1) {
                    DrawSplineCatmullRom(canvas.currentPath.data(), (int)canvas.currentPath.size(), canvas.strokeWidth, canvas.modeColor);
                }
            }
        }

        DrawTextEx(canvas.font, "Current Mode:", {10, 10}, 24, 2, DARKGRAY);
        DrawTextEx(canvas.font, canvas.modeText, {180, 10}, 24, 2, canvas.modeColor);
        EndDrawing();
    }
    UnloadFont(canvas.font);
    CloseWindow();
    return 0;
}
