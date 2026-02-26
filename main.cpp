#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;

enum Mode {
  SELECTION_MODE,
  MOVE_MODE,
  RESIZE_ROTATE_MODE,
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
  TRIANGLE_MODE,
  DOTTEDTRIANGLE_MODE,
};

enum BackgroundType { BG_BLANK, BG_GRID, BG_DOTTED, BG_GRAPH };
enum ExportScope { EXPORT_ALL, EXPORT_SELECTED, EXPORT_FRAME };

struct KeyBinding {
  int key = KEY_NULL;
  bool shift = false;
  bool ctrl = false;
  bool alt = false;
};

struct AppConfig {
  string configPath = "config/toggle.conf";
  int windowWidth = 1000;
  int windowHeight = 800;
  bool startMaximized = true;
  int targetFps = 60;
  int minWindowWidth = 320;
  int minWindowHeight = 240;
  string windowTitle = "Toggle : no more toggling";
  string defaultFontPath = "IosevkaNerdFontMono-Regular.ttf";
  int fontAtlasSize = 96;
  string defaultSaveDir;
  string defaultExportDir;
  string defaultOpenDir;
  float exportRasterScale = 2.0f;
  bool defaultDarkTheme = false;
  bool defaultShowTags = false;
  float defaultStrokeWidth = 2.0f;
  float minStrokeWidth = 1.0f;
  float maxStrokeWidth = 50.0f;
  float defaultTextSize = 24.0f;
  float minTextSize = 6.0f;
  float maxTextSize = 200.0f;
  float defaultGridWidth = 24.0f;
  float defaultGraphUnit = 24.0f;
  float defaultGraphMinorSpacing = 12.0f;
  float defaultGraphLabelSize = 12.0f;
  float defaultGraphLabelMinPx = 24.0f;
  float defaultGraphLabelMaxPx = 72.0f;
  float minZoom = 0.1f;
  float maxZoom = 10.0f;
  float zoomStep = 0.1f;
  float zoomKeyScale = 1.1f;
  float penSampleDistance = 2.0f;
  float selectionBoxActivationPx = 6.0f;
  float defaultHitTolerance = 2.0f;
  float statusDurationSeconds = 2.0f;
  float pasteOffsetStep = 20.0f;
  BackgroundType defaultBgType = BG_BLANK;
  Color defaultDrawColor = BLACK;
  float triangleHeightRatio = 0.8660254f;
  Color lightBackground = {247, 243, 232, 255};
  Color darkBackground = {24, 24, 24, 255};
  Color lightUiText = {42, 42, 42, 255};
  Color darkUiText = {228, 228, 239, 255};
  Color lightTextureA = {247, 243, 232, 255};
  Color lightTextureB = {241, 235, 222, 255};
  Color darkTextureA = {31, 31, 31, 255};
  Color darkTextureB = {27, 27, 27, 255};
  Color lightGridColor = {216, 203, 178, 80};
  Color darkGridColor = {52, 52, 52, 64};
  Color lightGraphAxis = {90, 80, 64, 180};
  Color darkGraphAxis = {200, 200, 210, 160};
  Color lightGraphMajor = {180, 166, 142, 120};
  Color darkGraphMajor = {90, 90, 100, 90};
  Color lightGraphMinor = {216, 203, 178, 70};
  Color darkGraphMinor = {52, 52, 52, 45};
  Color lightGraphLabel = {42, 42, 42, 220};
  Color darkGraphLabel = {228, 228, 239, 220};
  Color lightStatusBg = {239, 230, 211, 255};
  Color darkStatusBg = {34, 34, 34, 255};
  Color lightStatusLabel = {107, 95, 74, 255};
  Color darkStatusLabel = {175, 175, 175, 255};
  Color lightStatusValue = {43, 37, 27, 255};
  Color darkStatusValue = {240, 240, 245, 255};
  Color modeSelection = MAROON;
  Color modeMove = DARKBROWN;
  Color modeLine = BLUE;
  Color modeCircle = DARKGREEN;
  Color modeRect = RED;
  Color modeTriangle = DARKPURPLE;
  Color modeTextColor = DARKBLUE;
  Color modeEraser = ORANGE;
  Color modePen = BLACK;
  unordered_map<string, vector<KeyBinding>> keymap;
};

struct Element {
  Mode type;
  Vector2 start;
  Vector2 end;
  float strokeWidth;
  Color color;
  float rotation = 0.0f;
  vector<Vector2> path;
  int originalIndex = -1;
  int uniqueID = -1;
  vector<Element> children;
  string text;
  float textSize = 24.0f;

  Rectangle GetLocalBounds() const {
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

  Rectangle GetBounds() const {
    Rectangle local = GetLocalBounds();
    if (rotation == 0.0f || type == CIRCLE_MODE || type == DOTTEDCIRCLE_MODE ||
        type == GROUP_MODE) {
      return local;
    }

    Vector2 center = {local.x + local.width * 0.5f,
                      local.y + local.height * 0.5f};
    float rad = rotation;
    auto rot = [&](Vector2 p) {
      float s = sinf(rad);
      float c = cosf(rad);
      Vector2 v = {p.x - center.x, p.y - center.y};
      return Vector2{center.x + v.x * c - v.y * s,
                     center.y + v.x * s + v.y * c};
    };

    Vector2 c1 = rot({local.x, local.y});
    Vector2 c2 = rot({local.x + local.width, local.y});
    Vector2 c3 = rot({local.x + local.width, local.y + local.height});
    Vector2 c4 = rot({local.x, local.y + local.height});

    float minX = min(min(c1.x, c2.x), min(c3.x, c4.x));
    float minY = min(min(c1.y, c2.y), min(c3.y, c4.y));
    float maxX = max(max(c1.x, c2.x), max(c3.x, c4.x));
    float maxY = max(max(c1.y, c2.y), max(c3.y, c4.y));
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
  bool boxSelectActive = false;
  int lastKey = 0;
  int nextElementId = 0;
  bool isTextEditing = false;
  string textBuffer;
  Vector2 textPos = {0};
  float textSize = 24.0f;
  int editingIndex = -1;
  string editingOriginalText;
  Color editingColor = BLACK;
  float editingTextSize = 24.0f;
  bool textEditBackedUp = false;
  double lastClickTime = 0.0;
  Vector2 lastClickPos = {0};
  int pasteOffsetIndex = 0;
  Camera2D camera = {};
  bool commandMode = false;
  string commandBuffer;
  string statusMessage;
  double statusUntil = 0.0;
  bool shouldQuit = false;
  bool showStatusBar = true;
  bool darkTheme = false;
  Color backgroundColor = WHITE;
  Color uiTextColor = DARKGRAY;
  Color textureColorA = {250, 250, 250, 255};
  Color textureColorB = {240, 240, 240, 255};
  Color gridColor = {90, 90, 90, 70};
  Color statusBarBg = {24, 24, 24, 255};
  Color statusLabelColor = {170, 170, 170, 255};
  Color statusValueColor = {245, 245, 245, 255};
  Color drawColor = BLACK;
  BackgroundType bgType = BG_BLANK;
  float gridWidth = 24.0f;
  float graphUnit = 24.0f;
  float graphMinorSpacing = 12.0f;
  float graphLabelSize = 12.0f;
  float graphLabelMinPx = 60.0f;
  float graphLabelMaxPx = 140.0f;
  Color graphAxisColor = {0, 0, 0, 200};
  Color graphMajorColor = {0, 0, 0, 120};
  Color graphMinorColor = {0, 0, 0, 70};
  Color graphLabelColor = {0, 0, 0, 220};
  string savePath;
  string fontFamilyPath = "IosevkaNerdFontMono-Regular.ttf";
  bool ownsFont = true;
  bool transformActive = false;
  int transformHandle = 0;
  int transformIndex = -1;
  Element transformStart;
  Vector2 transformCenter = {0.0f, 0.0f};
  Vector2 transformStartMouse = {0.0f, 0.0f};
  float transformStartAngle = 0.0f;
  bool antiMouseMode = false;
  Vector2 antiMousePos = {0.0f, 0.0f};
  Vector2 antiMouseVel = {0.0f, 0.0f};
  Vector2 lastMouseScreen = {0.0f, 0.0f};
  Vector2 keyMoveVel = {0.0f, 0.0f};
  bool keyMoveActive = false;
};

void RestoreZOrder(Canvas &canvas);
bool ParseHexColor(string hex, Color &outColor);
string ColorToHex(Color c);

void SaveBackup(Canvas &canvas) {
  canvas.undoStack.push_back(canvas.elements);
  canvas.redoStack.clear();
}

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

void NormalizeElementIDs(Element &el, unordered_set<int> &used, int &nextId) {
  if (el.uniqueID < 0 || used.count(el.uniqueID) > 0) {
    while (used.count(nextId) > 0)
      nextId++;
    el.uniqueID = nextId++;
  } else {
    used.insert(el.uniqueID);
    if (el.uniqueID >= nextId)
      nextId = el.uniqueID + 1;
  }

  used.insert(el.uniqueID);
  for (auto &child : el.children)
    NormalizeElementIDs(child, used, nextId);
}

void NormalizeCanvasIDs(Canvas &canvas) {
  unordered_set<int> used;
  int nextId = 0;
  for (auto &el : canvas.elements)
    NormalizeElementIDs(el, used, nextId);
  canvas.nextElementId = nextId;
}

Vector2 RotatePoint(Vector2 p, Vector2 center, float radians) {
  float s = sinf(radians);
  float c = cosf(radians);
  Vector2 v = {p.x - center.x, p.y - center.y};
  return {center.x + v.x * c - v.y * s, center.y + v.x * s + v.y * c};
}

void GetTriangleVerticesLocal(const Element &el, Vector2 &apex, Vector2 &left,
                              Vector2 &right) {
  float x0 = min(el.start.x, el.end.x);
  float x1 = max(el.start.x, el.end.x);
  float y0 = min(el.start.y, el.end.y);
  float y1 = max(el.start.y, el.end.y);
  apex = {(x0 + x1) * 0.5f, y0};
  left = {x0, y1};
  right = {x1, y1};
}

Vector2 ConstrainTriangleEnd(const AppConfig &cfg, Vector2 start,
                             Vector2 rawEnd) {
  float dx = rawEnd.x - start.x;
  float dy = rawEnd.y - start.y;
  if (fabsf(dx) < 0.001f && fabsf(dy) < 0.001f)
    return rawEnd;
  float ratio = max(0.05f, cfg.triangleHeightRatio);
  float side = max(fabsf(dx), fabsf(dy) / ratio);
  float sx = (dx >= 0.0f) ? 1.0f : -1.0f;
  float sy = (dy >= 0.0f) ? 1.0f : -1.0f;
  return {start.x + sx * side, start.y + sy * side * ratio};
}

Vector2 ElementCenterLocal(const Element &el) {
  if (el.type == LINE_MODE || el.type == DOTTEDLINE_MODE ||
      el.type == ARROWLINE_MODE) {
    return {(el.start.x + el.end.x) * 0.5f, (el.start.y + el.end.y) * 0.5f};
  }
  Rectangle b = el.GetLocalBounds();
  return {b.x + b.width * 0.5f, b.y + b.height * 0.5f};
}

bool LineIntersectsRect(Vector2 a, Vector2 b, Rectangle r) {
  Vector2 r1 = {r.x, r.y};
  Vector2 r2 = {r.x + r.width, r.y};
  Vector2 r3 = {r.x + r.width, r.y + r.height};
  Vector2 r4 = {r.x, r.y + r.height};
  if (CheckCollisionPointRec(a, r) || CheckCollisionPointRec(b, r))
    return true;
  if (CheckCollisionLines(a, b, r1, r2, nullptr))
    return true;
  if (CheckCollisionLines(a, b, r2, r3, nullptr))
    return true;
  if (CheckCollisionLines(a, b, r3, r4, nullptr))
    return true;
  if (CheckCollisionLines(a, b, r4, r1, nullptr))
    return true;
  return false;
}

bool PointInQuad(Vector2 p, Vector2 a, Vector2 b, Vector2 c, Vector2 d) {
  auto cross = [](Vector2 u, Vector2 v) { return u.x * v.y - u.y * v.x; };
  Vector2 ab = {b.x - a.x, b.y - a.y};
  Vector2 bc = {c.x - b.x, c.y - b.y};
  Vector2 cd = {d.x - c.x, d.y - c.y};
  Vector2 da = {a.x - d.x, a.y - d.y};
  Vector2 ap = {p.x - a.x, p.y - a.y};
  Vector2 bp = {p.x - b.x, p.y - b.y};
  Vector2 cp = {p.x - c.x, p.y - c.y};
  Vector2 dp = {p.x - d.x, p.y - d.y};
  float c1 = cross(ab, ap);
  float c2 = cross(bc, bp);
  float c3 = cross(cd, cp);
  float c4 = cross(da, dp);
  bool hasNeg = (c1 < 0) || (c2 < 0) || (c3 < 0) || (c4 < 0);
  bool hasPos = (c1 > 0) || (c2 > 0) || (c3 > 0) || (c4 > 0);
  return !(hasNeg && hasPos);
}

bool PointInTriangle(Vector2 p, Vector2 a, Vector2 b, Vector2 c) {
  auto cross = [](Vector2 u, Vector2 v) { return u.x * v.y - u.y * v.x; };
  Vector2 ab = {b.x - a.x, b.y - a.y};
  Vector2 bc = {c.x - b.x, c.y - b.y};
  Vector2 ca = {a.x - c.x, a.y - c.y};
  Vector2 ap = {p.x - a.x, p.y - a.y};
  Vector2 bp = {p.x - b.x, p.y - b.y};
  Vector2 cp = {p.x - c.x, p.y - c.y};
  float c1 = cross(ab, ap);
  float c2 = cross(bc, bp);
  float c3 = cross(ca, cp);
  bool hasNeg = (c1 < 0) || (c2 < 0) || (c3 < 0);
  bool hasPos = (c1 > 0) || (c2 > 0) || (c3 > 0);
  return !(hasNeg && hasPos);
}

bool IsPointInSelectionVisual(const Element &el, Vector2 p) {
  const float rectPad = 5.0f;
  const float linePad = 6.0f;
  Color selColor = {70, 140, 160, 255};
  (void)selColor;

  if (el.type == LINE_MODE || el.type == DOTTEDLINE_MODE ||
      el.type == ARROWLINE_MODE) {
    Vector2 s = el.start;
    Vector2 e = el.end;
    if (el.rotation != 0.0f) {
      Vector2 center = ElementCenterLocal(el);
      s = RotatePoint(s, center, el.rotation);
      e = RotatePoint(e, center, el.rotation);
    }
    float length = Vector2Distance(s, e);
    if (length < 0.01f) {
      Rectangle b = el.GetBounds();
      Rectangle expanded = {b.x - rectPad, b.y - rectPad, b.width + 2 * rectPad,
                            b.height + 2 * rectPad};
      return CheckCollisionPointRec(p, expanded);
    }
    float width = length + linePad * 2.0f;
    float height = el.strokeWidth + linePad * 2.0f;
    Vector2 center = {(s.x + e.x) * 0.5f, (s.y + e.y) * 0.5f};
    float rad = atan2f(e.y - s.y, e.x - s.x);
    Vector2 hx = {cosf(rad) * (width * 0.5f), sinf(rad) * (width * 0.5f)};
    Vector2 hy = {-sinf(rad) * (height * 0.5f), cosf(rad) * (height * 0.5f)};
    Vector2 c1 = Vector2Subtract(Vector2Subtract(center, hx), hy);
    Vector2 c2 = Vector2Add(Vector2Subtract(center, hx), hy);
    Vector2 c3 = Vector2Add(Vector2Add(center, hx), hy);
    Vector2 c4 = Vector2Subtract(Vector2Add(center, hx), hy);
    return PointInQuad(p, c1, c2, c3, c4);
  }

  if (el.type == TRIANGLE_MODE || el.type == DOTTEDTRIANGLE_MODE) {
    Vector2 localP = p;
    if (el.rotation != 0.0f) {
      Vector2 center = ElementCenterLocal(el);
      localP = RotatePoint(p, center, -el.rotation);
    }
    Vector2 apex, left, right;
    GetTriangleVerticesLocal(el, apex, left, right);
    float t = max(0.5f, el.strokeWidth * 0.5f + rectPad);
    if (PointInTriangle(localP, apex, left, right))
      return true;
    return CheckCollisionPointLine(localP, apex, left, t) ||
           CheckCollisionPointLine(localP, left, right, t) ||
           CheckCollisionPointLine(localP, right, apex, t);
  }

  Rectangle b = el.GetLocalBounds();
  Rectangle expanded = {b.x - rectPad, b.y - rectPad, b.width + 2 * rectPad,
                        b.height + 2 * rectPad};
  if (el.rotation == 0.0f ||
      el.type == CIRCLE_MODE || el.type == DOTTEDCIRCLE_MODE ||
      el.type == GROUP_MODE) {
    return CheckCollisionPointRec(p, expanded);
  }

  Vector2 center = ElementCenterLocal(el);
  Vector2 tl = RotatePoint({expanded.x, expanded.y}, center, el.rotation);
  Vector2 tr =
      RotatePoint({expanded.x + expanded.width, expanded.y}, center, el.rotation);
  Vector2 br = RotatePoint({expanded.x + expanded.width,
                            expanded.y + expanded.height},
                           center, el.rotation);
  Vector2 bl =
      RotatePoint({expanded.x, expanded.y + expanded.height}, center,
                  el.rotation);
  return PointInQuad(p, tl, tr, br, bl);
}

bool ElementIntersectsRect(const Element &el, Rectangle r, float tol) {
  Rectangle expanded = {r.x - tol, r.y - tol, r.width + 2 * tol,
                        r.height + 2 * tol};
  if (el.type == GROUP_MODE) {
    for (const auto &child : el.children) {
      if (ElementIntersectsRect(child, r, tol))
        return true;
    }
    return false;
  }

  if (el.type == LINE_MODE || el.type == DOTTEDLINE_MODE ||
      el.type == ARROWLINE_MODE) {
    Vector2 s = el.start;
    Vector2 e = el.end;
    if (el.rotation != 0.0f) {
      Vector2 center = ElementCenterLocal(el);
      s = RotatePoint(s, center, el.rotation);
      e = RotatePoint(e, center, el.rotation);
    }
    return LineIntersectsRect(s, e, expanded);
  }

  if (el.type == CIRCLE_MODE || el.type == DOTTEDCIRCLE_MODE) {
    float rads = Vector2Distance(el.start, el.end) + el.strokeWidth * 0.5f + tol;
    float cx = Clamp(el.start.x, expanded.x, expanded.x + expanded.width);
    float cy = Clamp(el.start.y, expanded.y, expanded.y + expanded.height);
    float dx = el.start.x - cx;
    float dy = el.start.y - cy;
    return (dx * dx + dy * dy) <= (rads * rads);
  }

  if (el.type == PEN_MODE) {
    Vector2 center = ElementCenterLocal(el);
    if (el.path.empty())
      return false;
    Vector2 prev = el.path[0];
    if (el.rotation != 0.0f)
      prev = RotatePoint(prev, center, el.rotation);
    if (CheckCollisionPointRec(prev, expanded))
      return true;
    for (size_t i = 1; i < el.path.size(); ++i) {
      Vector2 cur = el.path[i];
      if (el.rotation != 0.0f)
        cur = RotatePoint(cur, center, el.rotation);
      if (LineIntersectsRect(prev, cur, expanded))
        return true;
      prev = cur;
    }
    return false;
  }

  if (el.type == TRIANGLE_MODE || el.type == DOTTEDTRIANGLE_MODE) {
    Vector2 apex, left, right;
    GetTriangleVerticesLocal(el, apex, left, right);
    if (el.rotation != 0.0f) {
      Vector2 center = ElementCenterLocal(el);
      apex = RotatePoint(apex, center, el.rotation);
      left = RotatePoint(left, center, el.rotation);
      right = RotatePoint(right, center, el.rotation);
    }
    if (CheckCollisionPointRec(apex, expanded) ||
        CheckCollisionPointRec(left, expanded) ||
        CheckCollisionPointRec(right, expanded))
      return true;
    Vector2 r1 = {expanded.x, expanded.y};
    Vector2 r2 = {expanded.x + expanded.width, expanded.y};
    Vector2 r3 = {expanded.x + expanded.width, expanded.y + expanded.height};
    Vector2 r4 = {expanded.x, expanded.y + expanded.height};
    if (PointInTriangle(r1, apex, left, right) ||
        PointInTriangle(r2, apex, left, right) ||
        PointInTriangle(r3, apex, left, right) ||
        PointInTriangle(r4, apex, left, right))
      return true;
    if (LineIntersectsRect(apex, left, expanded) ||
        LineIntersectsRect(left, right, expanded) ||
        LineIntersectsRect(right, apex, expanded))
      return true;
    return false;
  }

  Rectangle b = el.GetLocalBounds();
  Vector2 center = ElementCenterLocal(el);
  Vector2 tl = {b.x, b.y};
  Vector2 tr = {b.x + b.width, b.y};
  Vector2 br = {b.x + b.width, b.y + b.height};
  Vector2 bl = {b.x, b.y + b.height};
  if (el.rotation != 0.0f &&
      (el.type == RECTANGLE_MODE || el.type == DOTTEDRECT_MODE ||
       el.type == TEXT_MODE || el.type == TRIANGLE_MODE ||
       el.type == DOTTEDTRIANGLE_MODE)) {
    tl = RotatePoint(tl, center, el.rotation);
    tr = RotatePoint(tr, center, el.rotation);
    br = RotatePoint(br, center, el.rotation);
    bl = RotatePoint(bl, center, el.rotation);
  }

  if (PointInQuad({expanded.x, expanded.y}, tl, tr, br, bl) ||
      PointInQuad({expanded.x + expanded.width, expanded.y}, tl, tr, br, bl) ||
      PointInQuad({expanded.x + expanded.width, expanded.y + expanded.height}, tl,
                  tr, br, bl) ||
      PointInQuad({expanded.x, expanded.y + expanded.height}, tl, tr, br, bl))
    return true;
  if (CheckCollisionPointRec(tl, expanded) ||
      CheckCollisionPointRec(tr, expanded) ||
      CheckCollisionPointRec(br, expanded) ||
      CheckCollisionPointRec(bl, expanded))
    return true;

  if (LineIntersectsRect(tl, tr, expanded) ||
      LineIntersectsRect(tr, br, expanded) ||
      LineIntersectsRect(br, bl, expanded) ||
      LineIntersectsRect(bl, tl, expanded))
    return true;

  return false;
}

bool IsPointOnElement(const Element &el, Vector2 p, float tolerance) {
  Vector2 localP = p;
  if (el.rotation != 0.0f && el.type != CIRCLE_MODE &&
      el.type != DOTTEDCIRCLE_MODE) {
    Vector2 center = ElementCenterLocal(el);
    localP = RotatePoint(p, center, -el.rotation);
  }
  float tol = max(0.5f, tolerance);
  if (el.type == LINE_MODE || el.type == DOTTEDLINE_MODE ||
      el.type == ARROWLINE_MODE) {
    if (Vector2Distance(el.start, el.end) < 0.001f) {
      return Vector2Distance(localP, el.start) <= (el.strokeWidth * 0.5f + tol);
    }
    return CheckCollisionPointLine(localP, el.start, el.end,
                                   el.strokeWidth * 0.5f + tol);
  }
  if (el.type == CIRCLE_MODE || el.type == DOTTEDCIRCLE_MODE) {
    float r = Vector2Distance(el.start, el.end);
    float d = Vector2Distance(localP, el.start);
    return d <= (r + el.strokeWidth * 0.5f + tol);
  }
  if (el.type == RECTANGLE_MODE || el.type == DOTTEDRECT_MODE) {
    float x0 = min(el.start.x, el.end.x);
    float y0 = min(el.start.y, el.end.y);
    float x1 = max(el.start.x, el.end.x);
    float y1 = max(el.start.y, el.end.y);
    Rectangle filled = {x0 - tol, y0 - tol, (x1 - x0) + 2 * tol,
                        (y1 - y0) + 2 * tol};
    if (CheckCollisionPointRec(localP, filled))
      return true;
    Vector2 a = {x0, y0};
    Vector2 b = {x1, y0};
    Vector2 c = {x1, y1};
    Vector2 d = {x0, y1};
    float t = el.strokeWidth * 0.5f + tol;
    return CheckCollisionPointLine(localP, a, b, t) ||
           CheckCollisionPointLine(localP, b, c, t) ||
           CheckCollisionPointLine(localP, c, d, t) ||
           CheckCollisionPointLine(localP, d, a, t);
  }
  if (el.type == TRIANGLE_MODE || el.type == DOTTEDTRIANGLE_MODE) {
    Vector2 apex, left, right;
    GetTriangleVerticesLocal(el, apex, left, right);
    float t = el.strokeWidth * 0.5f + tol;
    if (PointInTriangle(localP, apex, left, right))
      return true;
    return CheckCollisionPointLine(localP, apex, left, t) ||
           CheckCollisionPointLine(localP, left, right, t) ||
           CheckCollisionPointLine(localP, right, apex, t);
  }
  if (el.type == PEN_MODE) {
    if (el.path.empty())
      return false;
    if (el.path.size() == 1)
      return Vector2Distance(localP, el.path[0]) <=
             (el.strokeWidth * 0.5f + tol);
    float t = el.strokeWidth * 0.5f + tol;
    for (size_t i = 1; i < el.path.size(); i++) {
      if (CheckCollisionPointLine(localP, el.path[i - 1], el.path[i], t))
        return true;
    }
    return false;
  }
  if (el.type == GROUP_MODE) {
    for (const auto &child : el.children) {
      if (IsPointOnElement(child, p, tolerance))
        return true;
    }
    return false;
  }
  if (el.type == TEXT_MODE) {
    return CheckCollisionPointRec(localP, el.GetLocalBounds());
  }
  return false;
}

bool IsPointOnSelectedBounds(const Canvas &canvas, Vector2 p) {
  for (int i = (int)canvas.selectedIndices.size() - 1; i >= 0; --i) {
    int idx = canvas.selectedIndices[i];
    if (idx >= 0 && idx < (int)canvas.elements.size()) {
      if (IsPointInSelectionVisual(canvas.elements[idx], p))
        return true;
    }
  }
  return false;
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

void ReselectByIDs(Canvas &canvas, const vector<int> &ids) {
  canvas.selectedIndices.clear();
  for (int id : ids) {
    int idx = FindElementIndexByID(canvas, id);
    if (idx != -1)
      canvas.selectedIndices.push_back(idx);
  }
}

void MoveSelectionZOrder(Canvas &canvas, bool forward) {
  vector<int> selectedIDs = GetSelectedIDs(canvas);
  if (selectedIDs.empty())
    return;

  SaveBackup(canvas);
  RestoreZOrder(canvas);

  vector<bool> isSelected(canvas.elements.size(), false);
  for (int id : selectedIDs) {
    int idx = FindElementIndexByID(canvas, id);
    if (idx != -1)
      isSelected[idx] = true;
  }

  if (forward) {
    for (int i = (int)canvas.elements.size() - 2; i >= 0; --i) {
      if (isSelected[i] && !isSelected[i + 1]) {
        swap(canvas.elements[i], canvas.elements[i + 1]);
        swap(isSelected[i], isSelected[i + 1]);
      }
    }
  } else {
    for (int i = 1; i < (int)canvas.elements.size(); ++i) {
      if (isSelected[i] && !isSelected[i - 1]) {
        swap(canvas.elements[i], canvas.elements[i - 1]);
        swap(isSelected[i], isSelected[i - 1]);
      }
    }
  }

  ReselectByIDs(canvas, selectedIDs);
}

void DrawDashedLine(Vector2 start, Vector2 end, float width, Color color) {
  float totalLen = Vector2Distance(start, end);
  if (totalLen < 1.0f)
    return;
  Vector2 dir = Vector2Normalize(Vector2Subtract(end, start));
  float dashLen = max(width * 2.0f, 6.0f);
  float gapLen = max(width * 1.2f, 4.0f);
  for (float i = 0.0f; i < totalLen; i += (dashLen + gapLen)) {
    float endDist = min(i + dashLen, totalLen);
    Vector2 s = Vector2Add(start, Vector2Scale(dir, i));
    Vector2 e = Vector2Add(start, Vector2Scale(dir, endDist));
    DrawLineEx(s, e, width, color);
  }
}

void DrawArrowLine(Vector2 start, Vector2 end, float width, Color color) {
  DrawLineEx(start, end, width, color);
  float angle = atan2f(end.y - start.y, end.x - start.x);
  float headSize = max(15.0f, width * 3.0f);
  float lineLen = Vector2Distance(start, end);
  if (headSize > lineLen * 0.7f)
    headSize = lineLen * 0.7f;
  Vector2 p1 = {end.x - headSize * cosf(angle - PI / 6),
                end.y - headSize * sinf(angle - PI / 6)};
  Vector2 p2 = {end.x - headSize * cosf(angle + PI / 6),
                end.y - headSize * sinf(angle + PI / 6)};
  DrawLineEx(end, p1, width, color);
  DrawLineEx(end, p2, width, color);
}

void DrawDashedRing(Vector2 center, float radius, float width, Color color) {
  if (radius <= 0.5f)
    return;

  float circumference = 2.0f * PI * radius;
  float dashArcLen = max(width * 2.0f, 6.0f);
  float gapArcLen = max(width * 1.2f, 4.0f);
  float dashDeg = (dashArcLen / circumference) * 360.0f;
  float gapDeg = (gapArcLen / circumference) * 360.0f;

  if (dashDeg <= 0.0f)
    return;

  for (float a = 0.0f; a < 360.0f; a += (dashDeg + gapDeg)) {
    float aEnd = min(a + dashDeg, 360.0f);
    DrawRing(center, radius - width * 0.5f, radius + width * 0.5f, a, aEnd, 24,
             color);
  }
}

void DrawElement(const Element &el, const Font &font, float textSize) {
  Vector2 s = el.start;
  Vector2 e = el.end;
  if (el.rotation != 0.0f &&
      (el.type == LINE_MODE || el.type == DOTTEDLINE_MODE ||
       el.type == ARROWLINE_MODE)) {
    Vector2 center = ElementCenterLocal(el);
    s = RotatePoint(el.start, center, el.rotation);
    e = RotatePoint(el.end, center, el.rotation);
  }
  if (el.type == LINE_MODE)
    DrawLineEx(s, e, el.strokeWidth, el.color);
  else if (el.type == DOTTEDLINE_MODE)
    DrawDashedLine(s, e, el.strokeWidth, el.color);
  else if (el.type == ARROWLINE_MODE)
    DrawArrowLine(s, e, el.strokeWidth, el.color);
  else if (el.type == CIRCLE_MODE)
    DrawRing(el.start, Vector2Distance(el.start, el.end) - el.strokeWidth / 2,
             Vector2Distance(el.start, el.end) + el.strokeWidth / 2, 0, 360, 60,
             el.color);
  else if (el.type == DOTTEDCIRCLE_MODE)
    DrawDashedRing(el.start, Vector2Distance(el.start, el.end), el.strokeWidth,
                   el.color);
  else if (el.type == RECTANGLE_MODE) {
    Rectangle r = {min(el.start.x, el.end.x), min(el.start.y, el.end.y),
                   abs(el.end.x - el.start.x), abs(el.end.y - el.start.y)};
    if (el.rotation == 0.0f) {
      DrawRectangleLinesEx(r, el.strokeWidth, el.color);
    } else {
      Vector2 center = ElementCenterLocal(el);
      Vector2 origin = {r.width * 0.5f, r.height * 0.5f};
      Rectangle rect = {center.x, center.y, r.width, r.height};
      DrawRectanglePro(rect, origin, el.rotation * RAD2DEG, Fade(el.color, 0.0f));
      float rad = el.rotation;
      Vector2 hx = {cosf(rad) * (r.width * 0.5f),
                    sinf(rad) * (r.width * 0.5f)};
      Vector2 hy = {-sinf(rad) * (r.height * 0.5f),
                    cosf(rad) * (r.height * 0.5f)};
      Vector2 c1 = Vector2Subtract(Vector2Subtract(center, hx), hy);
      Vector2 c2 = Vector2Add(Vector2Subtract(center, hx), hy);
      Vector2 c3 = Vector2Add(Vector2Add(center, hx), hy);
      Vector2 c4 = Vector2Subtract(Vector2Add(center, hx), hy);
      DrawLineEx(c1, c2, el.strokeWidth, el.color);
      DrawLineEx(c2, c3, el.strokeWidth, el.color);
      DrawLineEx(c3, c4, el.strokeWidth, el.color);
      DrawLineEx(c4, c1, el.strokeWidth, el.color);
    }
  }
  else if (el.type == DOTTEDRECT_MODE) {
    Rectangle r = {min(el.start.x, el.end.x), min(el.start.y, el.end.y),
                   abs(el.end.x - el.start.x), abs(el.end.y - el.start.y)};
    float overlap = el.strokeWidth * 0.5f;
    if (el.rotation == 0.0f) {
      DrawDashedLine({r.x - overlap, r.y}, {r.x + r.width + overlap, r.y},
                     el.strokeWidth, el.color);
      DrawDashedLine({r.x + r.width, r.y - overlap},
                     {r.x + r.width, r.y + r.height + overlap}, el.strokeWidth,
                     el.color);
      DrawDashedLine({r.x + r.width + overlap, r.y + r.height},
                     {r.x - overlap, r.y + r.height}, el.strokeWidth, el.color);
      DrawDashedLine({r.x, r.y + r.height + overlap}, {r.x, r.y - overlap},
                     el.strokeWidth, el.color);
    } else {
      Vector2 center = ElementCenterLocal(el);
      float rad = el.rotation;
      Vector2 hx = {cosf(rad) * (r.width * 0.5f),
                    sinf(rad) * (r.width * 0.5f)};
      Vector2 hy = {-sinf(rad) * (r.height * 0.5f),
                    cosf(rad) * (r.height * 0.5f)};
      Vector2 c1 = Vector2Subtract(Vector2Subtract(center, hx), hy);
      Vector2 c2 = Vector2Add(Vector2Subtract(center, hx), hy);
      Vector2 c3 = Vector2Add(Vector2Add(center, hx), hy);
      Vector2 c4 = Vector2Subtract(Vector2Add(center, hx), hy);
      DrawDashedLine(c1, c2, el.strokeWidth, el.color);
      DrawDashedLine(c2, c3, el.strokeWidth, el.color);
      DrawDashedLine(c3, c4, el.strokeWidth, el.color);
      DrawDashedLine(c4, c1, el.strokeWidth, el.color);
    }
  } else if (el.type == TRIANGLE_MODE || el.type == DOTTEDTRIANGLE_MODE) {
    Vector2 apex, left, right;
    GetTriangleVerticesLocal(el, apex, left, right);
    if (el.rotation != 0.0f) {
      Vector2 center = ElementCenterLocal(el);
      apex = RotatePoint(apex, center, el.rotation);
      left = RotatePoint(left, center, el.rotation);
      right = RotatePoint(right, center, el.rotation);
    }
    if (el.type == DOTTEDTRIANGLE_MODE) {
      DrawDashedLine(apex, left, el.strokeWidth, el.color);
      DrawDashedLine(left, right, el.strokeWidth, el.color);
      DrawDashedLine(right, apex, el.strokeWidth, el.color);
    } else {
      DrawLineEx(apex, left, el.strokeWidth, el.color);
      DrawLineEx(left, right, el.strokeWidth, el.color);
      DrawLineEx(right, apex, el.strokeWidth, el.color);
    }
  } else if (el.type == PEN_MODE) {
    int pointCount = (int)el.path.size();

    if (pointCount == 1) {
      Vector2 p = el.path[0];
      if (el.rotation != 0.0f) {
        Vector2 center = ElementCenterLocal(el);
        p = RotatePoint(p, center, el.rotation);
      }
      DrawCircleV(p, el.strokeWidth / 2, el.color);
    }
    else if (pointCount >= 4) {
      if (el.rotation == 0.0f) {
        DrawSplineCatmullRom(el.path.data(), pointCount, el.strokeWidth,
                             el.color);
      } else {
        Vector2 center = ElementCenterLocal(el);
        vector<Vector2> rotated;
        rotated.reserve(el.path.size());
        for (const auto &p : el.path)
          rotated.push_back(RotatePoint(p, center, el.rotation));
        DrawSplineCatmullRom(rotated.data(), (int)rotated.size(),
                             el.strokeWidth, el.color);
      }
    }
    else if (pointCount > 1) {
      if (el.rotation == 0.0f) {
        DrawLineStrip(el.path.data(), pointCount, el.color);
      } else {
        Vector2 center = ElementCenterLocal(el);
        vector<Vector2> rotated;
        rotated.reserve(el.path.size());
        for (const auto &p : el.path)
          rotated.push_back(RotatePoint(p, center, el.rotation));
        DrawLineStrip(rotated.data(), (int)rotated.size(), el.color);
      }
    }
  } else if (el.type == GROUP_MODE) {
    for (const auto &child : el.children)
      DrawElement(child, font, textSize);
  } else if (el.type == TEXT_MODE) {
    float size = (el.textSize > 0.0f) ? el.textSize : textSize;
    if (el.rotation == 0.0f) {
      DrawTextEx(font, el.text.c_str(), el.start, size, 2, el.color);
    } else {
      Vector2 center = ElementCenterLocal(el);
      Vector2 origin = {center.x - el.start.x, center.y - el.start.y};
      DrawTextPro(font, el.text.c_str(), center, origin, el.rotation * RAD2DEG,
                  size, 2, el.color);
    }
  }
}

void UpdateTextBounds(Element &el, const Font &font, float fallbackTextSize) {
  if (el.type != TEXT_MODE)
    return;
  float size = (el.textSize > 0.0f) ? el.textSize : fallbackTextSize;
  Vector2 measured = MeasureTextEx(font, el.text.c_str(), size, 2);
  el.end = {el.start.x + max(10.0f, measured.x), el.start.y + max(size, measured.y)};
}

void ApplyColorRecursive(Element &el, Color c) {
  el.color = c;
  for (auto &child : el.children)
    ApplyColorRecursive(child, c);
}

void ApplyStrokeRecursive(Element &el, float width) {
  if (el.type != TEXT_MODE)
    el.strokeWidth = width;
  for (auto &child : el.children)
    ApplyStrokeRecursive(child, width);
}

void ApplyTextSizeRecursive(Element &el, float size, const Font &font,
                            float fallbackTextSize) {
  if (el.type == TEXT_MODE) {
    el.textSize = size;
    UpdateTextBounds(el, font, fallbackTextSize);
  }
  for (auto &child : el.children)
    ApplyTextSizeRecursive(child, size, font, fallbackTextSize);
}

void RecomputeTextBoundsRecursive(Element &el, const Font &font,
                                  float fallbackTextSize) {
  if (el.type == TEXT_MODE)
    UpdateTextBounds(el, font, fallbackTextSize);
  for (auto &child : el.children)
    RecomputeTextBoundsRecursive(child, font, fallbackTextSize);
}

void MoveElement(Element &el, Vector2 delta) {
  el.start = Vector2Add(el.start, delta);
  el.end = Vector2Add(el.end, delta);
  for (auto &p : el.path)
    p = Vector2Add(p, delta);
  for (auto &child : el.children)
    MoveElement(child, delta);
}

void RotateElementGeometry(Element &el, Vector2 center, float radians) {
  el.start = RotatePoint(el.start, center, radians);
  el.end = RotatePoint(el.end, center, radians);
  for (auto &p : el.path)
    p = RotatePoint(p, center, radians);
  for (auto &child : el.children)
    RotateElementGeometry(child, center, radians);
  if (el.type != CIRCLE_MODE && el.type != DOTTEDCIRCLE_MODE)
    el.rotation += radians;
}

void ScaleElementGeometry(Element &el, Vector2 center, float sx, float sy,
                          const Font &font, float fallbackTextSize) {
  auto scalePoint = [&](Vector2 p) {
    return Vector2{center.x + (p.x - center.x) * sx,
                   center.y + (p.y - center.y) * sy};
  };
  el.start = scalePoint(el.start);
  el.end = scalePoint(el.end);
  for (auto &p : el.path)
    p = scalePoint(p);
  for (auto &child : el.children)
    ScaleElementGeometry(child, center, sx, sy, font, fallbackTextSize);
  if (el.type == TEXT_MODE) {
    float size = (el.textSize > 0.0f) ? el.textSize : fallbackTextSize;
    float scale = max(fabsf(sx), fabsf(sy));
    el.textSize = max(1.0f, size * scale);
    UpdateTextBounds(el, font, fallbackTextSize);
  }
}

void RestoreZOrder(Canvas &canvas) {
  if (canvas.selectedIndices.empty())
    return;

  struct PendingRestore {
    Element el;
    int target;
  };
  vector<PendingRestore> toRestore;

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

vector<string> Split(const string &s, char sep) {
  vector<string> parts;
  string part;
  stringstream ss(s);
  while (getline(ss, part, sep))
    parts.push_back(part);
  return parts;
}

string Trim(const string &s) {
  size_t begin = s.find_first_not_of(" \t\r\n");
  if (begin == string::npos)
    return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(begin, end - begin + 1);
}

string ToLower(string s) {
  for (char &c : s)
    c = (char)tolower((unsigned char)c);
  return s;
}

bool IsPathSeparator(char c) { return c == '/' || c == '\\'; }

string HomeDirectory() {
  const char *home = getenv("HOME");
  if (home && *home)
    return home;
  const char *userProfile = getenv("USERPROFILE");
  if (userProfile && *userProfile)
    return userProfile;
  return ".";
}

string ExpandUserPath(string path) {
  path = Trim(path);
  if (path == "~")
    return HomeDirectory();
  if (path.rfind("~/", 0) == 0 || path.rfind("~\\", 0) == 0)
    return HomeDirectory() + path.substr(1);
  return path;
}

string DefaultDownloadsDir() {
  filesystem::path p = filesystem::path(HomeDirectory()) / "Downloads";
  if (filesystem::exists(p) && filesystem::is_directory(p))
    return p.string();
  return HomeDirectory();
}

bool LooksLikeDirPath(const string &value) {
  if (value.empty())
    return false;
  if (value.back() == '/' || value.back() == '\\')
    return true;
  filesystem::path p(ExpandUserPath(value));
  return filesystem::exists(p) && filesystem::is_directory(p);
}

bool HasDirectoryPart(const string &value) {
  filesystem::path p(value);
  return p.has_parent_path();
}

string StripQuotes(const string &s) {
  string t = Trim(s);
  if (t.size() >= 2) {
    char a = t.front();
    char b = t.back();
    if ((a == '\'' && b == '\'') || (a == '"' && b == '"'))
      return t.substr(1, t.size() - 2);
  }
  return t;
}

vector<string> TokenizeQuotedArgs(const string &raw) {
  vector<string> tokens;
  string current;
  bool inQuote = false;
  char quoteChar = '\0';
  for (size_t i = 0; i < raw.size(); i++) {
    char c = raw[i];
    if (inQuote) {
      if (c == quoteChar) {
        inQuote = false;
      } else if (c == '\\' && i + 1 < raw.size()) {
        current.push_back(raw[++i]);
      } else {
        current.push_back(c);
      }
      continue;
    }
    if (c == '\'' || c == '"') {
      inQuote = true;
      quoteChar = c;
      continue;
    }
    if (isspace((unsigned char)c)) {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      continue;
    }
    current.push_back(c);
  }
  if (!current.empty())
    tokens.push_back(current);
  return tokens;
}

bool IsExportType(const string &v) {
  string t = ToLower(Trim(v));
  return t == "png" || t == "svg" || t == "jpg" || t == "jpeg";
}

bool IsExportScopeToken(const string &v) {
  string t = ToLower(Trim(v));
  return t == "all" || t == "selected" || t == "frame";
}

ExportScope ParseExportScope(const string &v) {
  string t = ToLower(Trim(v));
  if (t == "selected")
    return EXPORT_SELECTED;
  if (t == "frame")
    return EXPORT_FRAME;
  return EXPORT_ALL;
}

string NormalizeExportType(const string &v) {
  string t = ToLower(Trim(v));
  if (t == "jpeg")
    return "jpg";
  return t;
}

string EnsureExt(string filename, const string &extNoDot) {
  filesystem::path p(filename);
  string ext = "." + extNoDot;
  if (ToLower(p.extension().string()) != ToLower(ext))
    p.replace_extension(ext);
  return p.string();
}

Rectangle ExpandRect(const Rectangle &r, float pad) {
  return {r.x - pad, r.y - pad, r.width + pad * 2.0f, r.height + pad * 2.0f};
}

bool UnionBounds(const vector<Element> &elements, Rectangle &out) {
  if (elements.empty())
    return false;
  Rectangle b = elements[0].GetBounds();
  float minX = b.x;
  float minY = b.y;
  float maxX = b.x + b.width;
  float maxY = b.y + b.height;
  for (size_t i = 1; i < elements.size(); i++) {
    Rectangle e = elements[i].GetBounds();
    minX = min(minX, e.x);
    minY = min(minY, e.y);
    maxX = max(maxX, e.x + e.width);
    maxY = max(maxY, e.y + e.height);
  }
  out = {minX, minY, max(1.0f, maxX - minX), max(1.0f, maxY - minY)};
  return true;
}

string ResolveDefaultDir(const string &preferred, const string &fallback) {
  string p = ExpandUserPath(preferred);
  if (!p.empty())
    return p;
  return fallback;
}

bool EnsureDirectory(const string &dirPath) {
  filesystem::path dir(ExpandUserPath(dirPath));
  if (dir.empty())
    return false;
  if (filesystem::exists(dir))
    return filesystem::is_directory(dir);
  error_code ec;
  return filesystem::create_directories(dir, ec);
}

string JoinPath(const string &dirPath, const string &fileName) {
  filesystem::path p = filesystem::path(ExpandUserPath(dirPath)) / fileName;
  return p.string();
}

string DefaultSaveTargetPath(const AppConfig &cfg) {
  return JoinPath(ResolveDefaultDir(cfg.defaultSaveDir, DefaultDownloadsDir()),
                  "untitled.toggle");
}

string EllipsizeTail(const Font &font, const string &text, float size, float spacing,
                    float maxWidth) {
  if (MeasureTextEx(font, text.c_str(), size, spacing).x <= maxWidth)
    return text;
  string ell = "...";
  if (MeasureTextEx(font, ell.c_str(), size, spacing).x > maxWidth)
    return "";
  string out = text;
  while (!out.empty()) {
    out.pop_back();
    string candidate = out + ell;
    if (MeasureTextEx(font, candidate.c_str(), size, spacing).x <= maxWidth)
      return candidate;
  }
  return ell;
}

float DrawLabelValue(const Font &font, float x, float y, float size, float spacing,
                     const string &label, const string &value, Color labelColor,
                     Color valueColor) {
  DrawTextEx(font, label.c_str(), {x, y}, size, spacing, labelColor);
  float lx = MeasureTextEx(font, label.c_str(), size, spacing).x;
  DrawTextEx(font, value.c_str(), {x + lx, y}, size, spacing, valueColor);
  float vx = MeasureTextEx(font, value.c_str(), size, spacing).x;
  return x + lx + vx;
}

string BackgroundTypeToString(BackgroundType t) {
  if (t == BG_GRID)
    return "grid";
  if (t == BG_DOTTED)
    return "dotted";
  if (t == BG_GRAPH)
    return "graph";
  return "blank";
}

bool ParseBackgroundType(const string &s, BackgroundType &out) {
  string v = ToLower(Trim(s));
  if (v == "blank") {
    out = BG_BLANK;
    return true;
  }
  if (v == "grid") {
    out = BG_GRID;
    return true;
  }
  if (v == "dotted") {
    out = BG_DOTTED;
    return true;
  }
  if (v == "graph") {
    out = BG_GRAPH;
    return true;
  }
  return false;
}

bool ParsePositiveFloat(const string &s, float &value) {
  if (s.empty())
    return false;
  char *endPtr = nullptr;
  value = strtof(s.c_str(), &endPtr);
  return endPtr != s.c_str() && *endPtr == '\0';
}

bool ParseIntValue(const string &s, int &value) {
  if (s.empty())
    return false;
  char *endPtr = nullptr;
  long v = strtol(s.c_str(), &endPtr, 10);
  if (endPtr == s.c_str() || *endPtr != '\0')
    return false;
  value = (int)v;
  return true;
}

string FormatGraphNumber(float v) {
  int rounded = (int)roundf(v);
  if (rounded == 0)
    return "0";
  return to_string(rounded);
}

float ChooseGraphStepUnits(float unit, float zoom, float minPx, float maxPx) {
  float safeUnit = max(0.0001f, unit);
  float safeZoom = max(0.0001f, zoom);
  float lo = min(minPx, maxPx);
  float hi = max(minPx, maxPx);
  float targetPx = lo;
  float targetUnits = targetPx / (safeUnit * safeZoom);
  if (targetUnits <= 0.0f)
    targetUnits = 1.0f;
  int expBase = (int)floor(log10f(targetUnits));
  float best = 0.0f;
  float bestScore = numeric_limits<float>::max();
  for (int exp = expBase - 4; exp <= expBase + 4; ++exp) {
    float base = powf(10.0f, (float)exp);
    for (float mult : {1.0f, 2.0f, 5.0f}) {
      float step = mult * base;
      float px = step * safeUnit * safeZoom;
      float penalty = 0.0f;
      if (px < lo)
        penalty = (lo - px) * 2.0f;
      else if (px > hi)
        penalty = (px - hi) * 2.0f;
      float score = penalty + fabsf(px - targetPx);
      if (score < bestScore) {
        bestScore = score;
        best = step;
      }
    }
  }
  if (best <= 0.0f)
    best = 1.0f;
  return best;
}

bool ParseBool(const string &s, bool &value) {
  string v = ToLower(Trim(s));
  if (v == "1" || v == "true" || v == "yes" || v == "on") {
    value = true;
    return true;
  }
  if (v == "0" || v == "false" || v == "no" || v == "off") {
    value = false;
    return true;
  }
  return false;
}

int KeyFromToken(const string &token) {
  string k = ToLower(Trim(token));
  if (k.size() == 1) {
    char c = k[0];
    if (c >= 'a' && c <= 'z')
      return KEY_A + (c - 'a');
    if (c >= '0' && c <= '9')
      return KEY_ZERO + (c - '0');
  }
  if (k == "escape" || k == "esc")
    return KEY_ESCAPE;
  if (k == "enter" || k == "return")
    return KEY_ENTER;
  if (k == "backspace")
    return KEY_BACKSPACE;
  if (k == "space")
    return KEY_SPACE;
  if (k == "tab")
    return KEY_TAB;
  if (k == "semicolon" || k == ";")
    return KEY_SEMICOLON;
  if (k == "minus" || k == "-")
    return KEY_MINUS;
  if (k == "equal" || k == "=")
    return KEY_EQUAL;
  if (k == "left_bracket" || k == "[")
    return KEY_LEFT_BRACKET;
  if (k == "right_bracket" || k == "]")
    return KEY_RIGHT_BRACKET;
  if (k == "kp_add")
    return KEY_KP_ADD;
  if (k == "kp_subtract")
    return KEY_KP_SUBTRACT;
  return KEY_NULL;
}

string KeyToToken(int key) {
  if (key >= KEY_A && key <= KEY_Z) {
    char c = (char)('A' + (key - KEY_A));
    return string(1, c);
  }
  if (key >= KEY_ZERO && key <= KEY_NINE) {
    char c = (char)('0' + (key - KEY_ZERO));
    return string(1, c);
  }
  if (key == KEY_ESCAPE)
    return "Escape";
  if (key == KEY_ENTER)
    return "Enter";
  if (key == KEY_BACKSPACE)
    return "Backspace";
  if (key == KEY_SPACE)
    return "Space";
  if (key == KEY_TAB)
    return "Tab";
  if (key == KEY_SEMICOLON)
    return "Semicolon";
  if (key == KEY_MINUS)
    return "Minus";
  if (key == KEY_EQUAL)
    return "Equal";
  if (key == KEY_LEFT_BRACKET)
    return "Left_Bracket";
  if (key == KEY_RIGHT_BRACKET)
    return "Right_Bracket";
  if (key == KEY_KP_ADD)
    return "Kp_Add";
  if (key == KEY_KP_SUBTRACT)
    return "Kp_Subtract";
  return "Unknown";
}

bool ParseKeyBinding(const string &raw, KeyBinding &binding) {
  binding = {};
  vector<string> tokens = Split(raw, '+');
  if (tokens.empty())
    return false;
  for (string t : tokens) {
    t = ToLower(Trim(t));
    if (t.empty())
      continue;
    if (t == "shift")
      binding.shift = true;
    else if (t == "ctrl" || t == "control")
      binding.ctrl = true;
    else if (t == "alt")
      binding.alt = true;
    else {
      int k = KeyFromToken(t);
      if (k == KEY_NULL)
        return false;
      binding.key = k;
    }
  }
  return binding.key != KEY_NULL;
}

vector<KeyBinding> ParseBindingList(const string &raw) {
  vector<KeyBinding> out;
  for (const string &entry : Split(raw, '|')) {
    KeyBinding b;
    if (ParseKeyBinding(entry, b))
      out.push_back(b);
  }
  return out;
}

bool IsBindingPressed(const KeyBinding &b, bool shiftDown, bool ctrlDown,
                     bool altDown) {
  if (b.key == KEY_NULL || !IsKeyPressed(b.key))
    return false;
  return (b.shift == shiftDown) && (b.ctrl == ctrlDown) && (b.alt == altDown);
}

bool IsActionPressed(const AppConfig &cfg, const string &action, bool shiftDown,
                     bool ctrlDown, bool altDown) {
  auto it = cfg.keymap.find(action);
  if (it == cfg.keymap.end())
    return false;
  for (const auto &binding : it->second) {
    if (IsBindingPressed(binding, shiftDown, ctrlDown, altDown))
      return true;
  }
  return false;
}

int PrimaryKeyForAction(const AppConfig &cfg, const string &action) {
  auto it = cfg.keymap.find(action);
  if (it == cfg.keymap.end() || it->second.empty())
    return KEY_NULL;
  return it->second[0].key;
}

void AddDefaultBinding(AppConfig &cfg, const string &action,
                       const string &bindingSpec) {
  cfg.keymap[action] = ParseBindingList(bindingSpec);
}

void SetDefaultKeymap(AppConfig &cfg) {
  AddDefaultBinding(cfg, "open_command_mode", "Shift+Semicolon");
  AddDefaultBinding(cfg, "zoom_in", "Shift+Equal|Kp_Add");
  AddDefaultBinding(cfg, "zoom_out", "Shift+Minus|Kp_Subtract");
  AddDefaultBinding(cfg, "stroke_inc", "Equal");
  AddDefaultBinding(cfg, "stroke_dec", "Minus");
  AddDefaultBinding(cfg, "copy", "Y");
  AddDefaultBinding(cfg, "paste", "Shift+P");
  AddDefaultBinding(cfg, "mode_pen", "P");
  AddDefaultBinding(cfg, "mode_selection", "S");
  AddDefaultBinding(cfg, "mode_move", "M");
  AddDefaultBinding(cfg, "mode_line_base", "L");
  AddDefaultBinding(cfg, "mode_circle_base", "C");
  AddDefaultBinding(cfg, "mode_rect_base", "Ctrl+R");
  AddDefaultBinding(cfg, "mode_resize_rotate", "R|Shift+R");
  AddDefaultBinding(cfg, "prefix_dotted", "D");
  AddDefaultBinding(cfg, "prefix_arrow", "A");
  AddDefaultBinding(cfg, "mode_triangle", "T");
  AddDefaultBinding(cfg, "mode_eraser", "E");
  AddDefaultBinding(cfg, "mode_text", "Shift+T");
  AddDefaultBinding(cfg, "group_toggle", "G");
  AddDefaultBinding(cfg, "toggle_tags", "F");
  AddDefaultBinding(cfg, "graph_toggle", "G");
  AddDefaultBinding(cfg, "undo", "U|Ctrl+Z");
  AddDefaultBinding(cfg, "redo", "Shift+U|Ctrl+Y|Ctrl+Shift+Z");
  AddDefaultBinding(cfg, "delete_selection", "X");
  AddDefaultBinding(cfg, "z_backward", "Left_Bracket");
  AddDefaultBinding(cfg, "z_forward", "Right_Bracket");
  AddDefaultBinding(cfg, "select_next_tag", "J");
  AddDefaultBinding(cfg, "select_prev_tag", "K");
  AddDefaultBinding(cfg, "select_all", "Ctrl+O");
}

void WriteDefaultConfig(const AppConfig &cfg) {
  ofstream out(cfg.configPath);
  if (!out.is_open())
    return;
  out << "# Toggle config\n";
  out << "window.width=" << cfg.windowWidth << "\n";
  out << "window.height=" << cfg.windowHeight << "\n";
  out << "window.min_width=" << cfg.minWindowWidth << "\n";
  out << "window.min_height=" << cfg.minWindowHeight << "\n";
  out << "window.title=" << cfg.windowTitle << "\n";
  out << "window.start_maximized=" << (cfg.startMaximized ? "true" : "false")
      << "\n";
  out << "app.target_fps=" << cfg.targetFps << "\n";
  out << "app.status_seconds=" << cfg.statusDurationSeconds << "\n";
  out << "font.default_path=" << cfg.defaultFontPath << "\n";
  out << "font.atlas_size=" << cfg.fontAtlasSize << "\n";
  out << "path.default_save_dir=" << cfg.defaultSaveDir << "\n";
  out << "path.default_export_dir=" << cfg.defaultExportDir << "\n";
  out << "path.default_open_dir=" << cfg.defaultOpenDir << "\n";
  out << "export.raster_scale=" << cfg.exportRasterScale << "\n";
  out << "canvas.theme_dark=" << (cfg.defaultDarkTheme ? "true" : "false") << "\n";
  out << "canvas.show_tags=" << (cfg.defaultShowTags ? "true" : "false") << "\n";
  out << "canvas.stroke_width=" << cfg.defaultStrokeWidth << "\n";
  out << "canvas.min_stroke_width=" << cfg.minStrokeWidth << "\n";
  out << "canvas.max_stroke_width=" << cfg.maxStrokeWidth << "\n";
  out << "canvas.text_size=" << cfg.defaultTextSize << "\n";
  out << "canvas.min_text_size=" << cfg.minTextSize << "\n";
  out << "canvas.max_text_size=" << cfg.maxTextSize << "\n";
  out << "canvas.grid_width=" << cfg.defaultGridWidth << "\n";
  out << "canvas.type=" << BackgroundTypeToString(cfg.defaultBgType) << "\n";
  out << "graph.unit=" << cfg.defaultGraphUnit << "\n";
  out << "graph.minor_spacing=" << cfg.defaultGraphMinorSpacing << "\n";
  out << "graph.label_size=" << cfg.defaultGraphLabelSize << "\n";
  out << "graph.label_min_px=" << cfg.defaultGraphLabelMinPx << "\n";
  out << "graph.label_max_px=" << cfg.defaultGraphLabelMaxPx << "\n";
  out << "canvas.draw_color=" << ColorToHex(cfg.defaultDrawColor) << "\n";
  out << "triangle.height_ratio=" << cfg.triangleHeightRatio << "\n";
  out << "zoom.min=" << cfg.minZoom << "\n";
  out << "zoom.max=" << cfg.maxZoom << "\n";
  out << "zoom.wheel_step=" << cfg.zoomStep << "\n";
  out << "zoom.key_scale=" << cfg.zoomKeyScale << "\n";
  out << "interaction.pen_sample_distance=" << cfg.penSampleDistance << "\n";
  out << "interaction.selection_box_activation_px="
      << cfg.selectionBoxActivationPx << "\n";
  out << "interaction.hit_tolerance=" << cfg.defaultHitTolerance << "\n";
  out << "interaction.paste_offset_step=" << cfg.pasteOffsetStep << "\n";
  out << "theme.light.background=" << ColorToHex(cfg.lightBackground) << "\n";
  out << "theme.dark.background=" << ColorToHex(cfg.darkBackground) << "\n";
  out << "theme.light.ui_text=" << ColorToHex(cfg.lightUiText) << "\n";
  out << "theme.dark.ui_text=" << ColorToHex(cfg.darkUiText) << "\n";
  out << "theme.light.texture_a=" << ColorToHex(cfg.lightTextureA) << "\n";
  out << "theme.light.texture_b=" << ColorToHex(cfg.lightTextureB) << "\n";
  out << "theme.dark.texture_a=" << ColorToHex(cfg.darkTextureA) << "\n";
  out << "theme.dark.texture_b=" << ColorToHex(cfg.darkTextureB) << "\n";
  out << "theme.light.grid=" << ColorToHex(cfg.lightGridColor) << "\n";
  out << "theme.dark.grid=" << ColorToHex(cfg.darkGridColor) << "\n";
  out << "graph.light.axis=" << ColorToHex(cfg.lightGraphAxis) << "\n";
  out << "graph.dark.axis=" << ColorToHex(cfg.darkGraphAxis) << "\n";
  out << "graph.light.major=" << ColorToHex(cfg.lightGraphMajor) << "\n";
  out << "graph.dark.major=" << ColorToHex(cfg.darkGraphMajor) << "\n";
  out << "graph.light.minor=" << ColorToHex(cfg.lightGraphMinor) << "\n";
  out << "graph.dark.minor=" << ColorToHex(cfg.darkGraphMinor) << "\n";
  out << "graph.light.label=" << ColorToHex(cfg.lightGraphLabel) << "\n";
  out << "graph.dark.label=" << ColorToHex(cfg.darkGraphLabel) << "\n";
  out << "status.light.bg=" << ColorToHex(cfg.lightStatusBg) << "\n";
  out << "status.dark.bg=" << ColorToHex(cfg.darkStatusBg) << "\n";
  out << "status.light.label=" << ColorToHex(cfg.lightStatusLabel) << "\n";
  out << "status.dark.label=" << ColorToHex(cfg.darkStatusLabel) << "\n";
  out << "status.light.value=" << ColorToHex(cfg.lightStatusValue) << "\n";
  out << "status.dark.value=" << ColorToHex(cfg.darkStatusValue) << "\n";
  out << "mode_color.selection=" << ColorToHex(cfg.modeSelection) << "\n";
  out << "mode_color.move=" << ColorToHex(cfg.modeMove) << "\n";
  out << "mode_color.line=" << ColorToHex(cfg.modeLine) << "\n";
  out << "mode_color.circle=" << ColorToHex(cfg.modeCircle) << "\n";
  out << "mode_color.rect=" << ColorToHex(cfg.modeRect) << "\n";
  out << "mode_color.triangle=" << ColorToHex(cfg.modeTriangle) << "\n";
  out << "mode_color.text=" << ColorToHex(cfg.modeTextColor) << "\n";
  out << "mode_color.eraser=" << ColorToHex(cfg.modeEraser) << "\n";
  out << "mode_color.pen=" << ColorToHex(cfg.modePen) << "\n";
  for (const auto &kv : cfg.keymap) {
    out << "key." << kv.first << "=";
    for (size_t i = 0; i < kv.second.size(); i++) {
      const auto &b = kv.second[i];
      if (i > 0)
        out << "|";
      if (b.ctrl)
        out << "Ctrl+";
      if (b.shift)
        out << "Shift+";
      if (b.alt)
        out << "Alt+";
      out << KeyToToken(b.key);
    }
    out << "\n";
  }
}

void LoadConfig(AppConfig &cfg) {
  ifstream in(cfg.configPath);
  if (!in.is_open()) {
    WriteDefaultConfig(cfg);
    return;
  }

  string line;
  while (getline(in, line)) {
    line = Trim(line);
    if (line.empty() || line[0] == '#')
      continue;
    size_t eq = line.find('=');
    if (eq == string::npos)
      continue;
    string key = Trim(line.substr(0, eq));
    string value = Trim(line.substr(eq + 1));

    int iv;
    float fv;
    bool bv;
    Color cv;
    if (key == "window.width" && ParseIntValue(value, iv))
      cfg.windowWidth = max(320, iv);
    else if (key == "window.height" && ParseIntValue(value, iv))
      cfg.windowHeight = max(240, iv);
    else if (key == "window.min_width" && ParseIntValue(value, iv))
      cfg.minWindowWidth = max(200, iv);
    else if (key == "window.min_height" && ParseIntValue(value, iv))
      cfg.minWindowHeight = max(150, iv);
    else if (key == "window.title")
      cfg.windowTitle = value;
    else if (key == "window.start_maximized" && ParseBool(value, bv))
      cfg.startMaximized = bv;
    else if (key == "window.start_fullscreen" && ParseBool(value, bv))
      cfg.startMaximized = bv;
    else if (key == "app.target_fps" && ParseIntValue(value, iv))
      cfg.targetFps = max(1, iv);
    else if (key == "app.status_seconds" && ParsePositiveFloat(value, fv))
      cfg.statusDurationSeconds = max(0.2f, fv);
    else if (key == "font.default_path")
      cfg.defaultFontPath = value;
    else if (key == "font.atlas_size" && ParseIntValue(value, iv))
      cfg.fontAtlasSize = max(16, iv);
    else if (key == "path.default_save_dir")
      cfg.defaultSaveDir = ExpandUserPath(value);
    else if (key == "path.default_export_dir")
      cfg.defaultExportDir = ExpandUserPath(value);
    else if (key == "path.default_open_dir")
      cfg.defaultOpenDir = ExpandUserPath(value);
    else if (key == "export.raster_scale" && ParsePositiveFloat(value, fv))
      cfg.exportRasterScale = max(1.0f, min(8.0f, fv));
    else if (key == "canvas.theme_dark" && ParseBool(value, bv))
      cfg.defaultDarkTheme = bv;
    else if (key == "canvas.show_tags" && ParseBool(value, bv))
      cfg.defaultShowTags = bv;
    else if (key == "canvas.stroke_width" && ParsePositiveFloat(value, fv))
      cfg.defaultStrokeWidth = max(0.1f, fv);
    else if (key == "canvas.min_stroke_width" && ParsePositiveFloat(value, fv))
      cfg.minStrokeWidth = max(0.1f, fv);
    else if (key == "canvas.max_stroke_width" && ParsePositiveFloat(value, fv))
      cfg.maxStrokeWidth = max(cfg.minStrokeWidth, fv);
    else if (key == "canvas.text_size" && ParsePositiveFloat(value, fv))
      cfg.defaultTextSize = max(1.0f, fv);
    else if (key == "canvas.min_text_size" && ParsePositiveFloat(value, fv))
      cfg.minTextSize = max(1.0f, fv);
    else if (key == "canvas.max_text_size" && ParsePositiveFloat(value, fv))
      cfg.maxTextSize = max(cfg.minTextSize, fv);
    else if (key == "canvas.grid_width" && ParsePositiveFloat(value, fv))
      cfg.defaultGridWidth = max(6.0f, fv);
    else if (key == "canvas.type")
      ParseBackgroundType(value, cfg.defaultBgType);
    else if (key == "graph.unit" && ParsePositiveFloat(value, fv))
      cfg.defaultGraphUnit = max(0.1f, fv);
    else if (key == "graph.minor_spacing" && ParsePositiveFloat(value, fv))
      cfg.defaultGraphMinorSpacing = max(0.1f, fv);
    else if (key == "graph.label_size" && ParsePositiveFloat(value, fv))
      cfg.defaultGraphLabelSize = max(6.0f, fv);
    else if (key == "graph.label_min_px" && ParsePositiveFloat(value, fv))
      cfg.defaultGraphLabelMinPx = max(10.0f, fv);
    else if (key == "graph.label_max_px" && ParsePositiveFloat(value, fv))
      cfg.defaultGraphLabelMaxPx = max(10.0f, fv);
    else if (key == "canvas.draw_color" && ParseHexColor(value, cv))
      cfg.defaultDrawColor = cv;
    else if (key == "triangle.height_ratio" && ParsePositiveFloat(value, fv))
      cfg.triangleHeightRatio = max(0.05f, fv);
    else if (key == "zoom.min" && ParsePositiveFloat(value, fv))
      cfg.minZoom = max(0.01f, fv);
    else if (key == "zoom.max" && ParsePositiveFloat(value, fv))
      cfg.maxZoom = max(cfg.minZoom, fv);
    else if (key == "zoom.wheel_step" && ParsePositiveFloat(value, fv))
      cfg.zoomStep = max(0.001f, fv);
    else if (key == "zoom.key_scale" && ParsePositiveFloat(value, fv))
      cfg.zoomKeyScale = max(1.001f, fv);
    else if (key == "interaction.pen_sample_distance" &&
             ParsePositiveFloat(value, fv))
      cfg.penSampleDistance = max(0.2f, fv);
    else if (key == "interaction.selection_box_activation_px" &&
             ParsePositiveFloat(value, fv))
      cfg.selectionBoxActivationPx = max(0.2f, fv);
    else if (key == "interaction.hit_tolerance" && ParsePositiveFloat(value, fv))
      cfg.defaultHitTolerance = max(0.2f, fv);
    else if (key == "interaction.paste_offset_step" &&
             ParsePositiveFloat(value, fv))
      cfg.pasteOffsetStep = max(1.0f, fv);
    else if (key == "theme.light.background" && ParseHexColor(value, cv))
      cfg.lightBackground = cv;
    else if (key == "theme.dark.background" && ParseHexColor(value, cv))
      cfg.darkBackground = cv;
    else if (key == "theme.light.ui_text" && ParseHexColor(value, cv))
      cfg.lightUiText = cv;
    else if (key == "theme.dark.ui_text" && ParseHexColor(value, cv))
      cfg.darkUiText = cv;
    else if (key == "theme.light.texture_a" && ParseHexColor(value, cv))
      cfg.lightTextureA = cv;
    else if (key == "theme.light.texture_b" && ParseHexColor(value, cv))
      cfg.lightTextureB = cv;
    else if (key == "theme.dark.texture_a" && ParseHexColor(value, cv))
      cfg.darkTextureA = cv;
    else if (key == "theme.dark.texture_b" && ParseHexColor(value, cv))
      cfg.darkTextureB = cv;
    else if (key == "theme.light.grid" && ParseHexColor(value, cv))
      cfg.lightGridColor = cv;
    else if (key == "theme.dark.grid" && ParseHexColor(value, cv))
      cfg.darkGridColor = cv;
    else if (key == "graph.light.axis" && ParseHexColor(value, cv))
      cfg.lightGraphAxis = cv;
    else if (key == "graph.dark.axis" && ParseHexColor(value, cv))
      cfg.darkGraphAxis = cv;
    else if (key == "graph.light.major" && ParseHexColor(value, cv))
      cfg.lightGraphMajor = cv;
    else if (key == "graph.dark.major" && ParseHexColor(value, cv))
      cfg.darkGraphMajor = cv;
    else if (key == "graph.light.minor" && ParseHexColor(value, cv))
      cfg.lightGraphMinor = cv;
    else if (key == "graph.dark.minor" && ParseHexColor(value, cv))
      cfg.darkGraphMinor = cv;
    else if (key == "graph.light.label" && ParseHexColor(value, cv))
      cfg.lightGraphLabel = cv;
    else if (key == "graph.dark.label" && ParseHexColor(value, cv))
      cfg.darkGraphLabel = cv;
    else if (key == "status.light.bg" && ParseHexColor(value, cv))
      cfg.lightStatusBg = cv;
    else if (key == "status.dark.bg" && ParseHexColor(value, cv))
      cfg.darkStatusBg = cv;
    else if (key == "status.light.label" && ParseHexColor(value, cv))
      cfg.lightStatusLabel = cv;
    else if (key == "status.dark.label" && ParseHexColor(value, cv))
      cfg.darkStatusLabel = cv;
    else if (key == "status.light.value" && ParseHexColor(value, cv))
      cfg.lightStatusValue = cv;
    else if (key == "status.dark.value" && ParseHexColor(value, cv))
      cfg.darkStatusValue = cv;
    else if (key == "mode_color.selection" && ParseHexColor(value, cv))
      cfg.modeSelection = cv;
    else if (key == "mode_color.move" && ParseHexColor(value, cv))
      cfg.modeMove = cv;
    else if (key == "mode_color.line" && ParseHexColor(value, cv))
      cfg.modeLine = cv;
    else if (key == "mode_color.circle" && ParseHexColor(value, cv))
      cfg.modeCircle = cv;
    else if (key == "mode_color.rect" && ParseHexColor(value, cv))
      cfg.modeRect = cv;
    else if (key == "mode_color.triangle" && ParseHexColor(value, cv))
      cfg.modeTriangle = cv;
    else if (key == "mode_color.text" && ParseHexColor(value, cv))
      cfg.modeTextColor = cv;
    else if (key == "mode_color.eraser" && ParseHexColor(value, cv))
      cfg.modeEraser = cv;
    else if (key == "mode_color.pen" && ParseHexColor(value, cv))
      cfg.modePen = cv;
    else if (key.rfind("key.", 0) == 0)
      cfg.keymap[key.substr(4)] = ParseBindingList(value);
  }

  cfg.maxStrokeWidth = max(cfg.minStrokeWidth, cfg.maxStrokeWidth);
  cfg.defaultStrokeWidth =
      min(cfg.maxStrokeWidth, max(cfg.minStrokeWidth, cfg.defaultStrokeWidth));
  cfg.maxTextSize = max(cfg.minTextSize, cfg.maxTextSize);
  cfg.defaultTextSize =
      min(cfg.maxTextSize, max(cfg.minTextSize, cfg.defaultTextSize));
  if (cfg.defaultGraphLabelMaxPx < cfg.defaultGraphLabelMinPx)
    cfg.defaultGraphLabelMaxPx = cfg.defaultGraphLabelMinPx;
  if (cfg.defaultSaveDir.empty())
    cfg.defaultSaveDir = DefaultDownloadsDir();
  if (cfg.defaultExportDir.empty())
    cfg.defaultExportDir = cfg.defaultSaveDir;
  if (cfg.defaultOpenDir.empty())
    cfg.defaultOpenDir = cfg.defaultSaveDir;
}

bool ParseHexColor(string hex, Color &outColor) {
  if (!hex.empty() && hex[0] == '#')
    hex.erase(hex.begin());
  if (hex.size() != 6 && hex.size() != 8)
    return false;
  for (char c : hex) {
    if (!isxdigit((unsigned char)c))
      return false;
  }
  auto byteFromHex = [](const string &s) -> unsigned char {
    return (unsigned char)strtol(s.c_str(), nullptr, 16);
  };
  outColor.r = byteFromHex(hex.substr(0, 2));
  outColor.g = byteFromHex(hex.substr(2, 2));
  outColor.b = byteFromHex(hex.substr(4, 2));
  outColor.a = (hex.size() == 8) ? byteFromHex(hex.substr(6, 2)) : 255;
  return true;
}

bool ParseNamedColor(string name, Color &outColor) {
  name = ToLower(Trim(name));
  static const unordered_map<string, Color> colors = {
      {"lightgray", LIGHTGRAY}, {"gray", GRAY},       {"darkgray", DARKGRAY},
      {"yellow", YELLOW},       {"gold", GOLD},       {"orange", ORANGE},
      {"pink", PINK},           {"red", RED},         {"maroon", MAROON},
      {"green", GREEN},         {"lime", LIME},       {"darkgreen", DARKGREEN},
      {"skyblue", SKYBLUE},     {"blue", BLUE},       {"darkblue", DARKBLUE},
      {"purple", PURPLE},       {"violet", VIOLET},   {"darkpurple", DARKPURPLE},
      {"beige", BEIGE},         {"brown", BROWN},     {"darkbrown", DARKBROWN},
      {"white", WHITE},         {"black", BLACK},     {"blank", BLANK},
      {"magenta", MAGENTA},     {"raywhite", RAYWHITE},
  };
  auto it = colors.find(name);
  if (it == colors.end())
    return false;
  outColor = it->second;
  return true;
}

string ColorToHex(Color c) {
  return TextFormat("#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
}

void SetStatus(Canvas &canvas, const AppConfig &cfg, const string &message,
               double seconds = -1.0) {
  canvas.statusMessage = message;
  if (seconds <= 0.0)
    seconds = cfg.statusDurationSeconds;
  canvas.statusUntil = GetTime() + seconds;
}

void SetTheme(Canvas &canvas, const AppConfig &cfg, bool dark) {
  canvas.darkTheme = dark;
  if (dark) {
    canvas.backgroundColor = cfg.darkBackground;
    canvas.uiTextColor = cfg.darkUiText;
    canvas.textureColorA = cfg.darkTextureA;
    canvas.textureColorB = cfg.darkTextureB;
    canvas.gridColor = cfg.darkGridColor;
    canvas.graphAxisColor = cfg.darkGraphAxis;
    canvas.graphMajorColor = cfg.darkGraphMajor;
    canvas.graphMinorColor = cfg.darkGraphMinor;
    canvas.graphLabelColor = cfg.darkGraphLabel;
    canvas.statusBarBg = cfg.darkStatusBg;
    canvas.statusBarBg.a = 255;
    canvas.statusLabelColor = cfg.darkStatusLabel;
    canvas.statusValueColor = cfg.darkStatusValue;
    canvas.drawColor = RAYWHITE;
  } else {
    canvas.backgroundColor = cfg.lightBackground;
    canvas.uiTextColor = cfg.lightUiText;
    canvas.textureColorA = cfg.lightTextureA;
    canvas.textureColorB = cfg.lightTextureB;
    canvas.gridColor = cfg.lightGridColor;
    canvas.graphAxisColor = cfg.lightGraphAxis;
    canvas.graphMajorColor = cfg.lightGraphMajor;
    canvas.graphMinorColor = cfg.lightGraphMinor;
    canvas.graphLabelColor = cfg.lightGraphLabel;
    canvas.statusBarBg = cfg.lightStatusBg;
    canvas.statusBarBg.a = 255;
    canvas.statusLabelColor = cfg.lightStatusLabel;
    canvas.statusValueColor = cfg.lightStatusValue;
    canvas.drawColor = BLACK;
  }
}

void DrawBackgroundPattern(const Canvas &canvas) {
  float left = canvas.camera.target.x - canvas.camera.offset.x / canvas.camera.zoom;
  float top = canvas.camera.target.y - canvas.camera.offset.y / canvas.camera.zoom;
  float right =
      canvas.camera.target.x + (float)GetScreenWidth() / canvas.camera.zoom;
  float bottom =
      canvas.camera.target.y + (float)GetScreenHeight() / canvas.camera.zoom;

  if (canvas.bgType == BG_BLANK)
    return;

  float spacing = max(6.0f, canvas.gridWidth);

  float startX = floorf(left / spacing) * spacing;
  float startY = floorf(top / spacing) * spacing;
  Color lineColor = canvas.gridColor;

  if (canvas.bgType == BG_GRID) {
    for (float x = startX; x <= right + spacing; x += spacing)
      DrawLineV({x, top - spacing}, {x, bottom + spacing}, lineColor);
    for (float y = startY; y <= bottom + spacing; y += spacing)
      DrawLineV({left - spacing, y}, {right + spacing, y}, lineColor);
  } else if (canvas.bgType == BG_DOTTED) {
    for (float x = startX; x <= right + spacing; x += spacing) {
      for (float y = startY; y <= bottom + spacing; y += spacing) {
        DrawCircleV({x, y}, 1.4f, lineColor);
      }
    }
  } else if (canvas.bgType == BG_GRAPH) {
    float unit = max(0.0001f, canvas.graphUnit);
    float minorSpacing = max(0.0001f, canvas.graphMinorSpacing);
    float majorUnitsRaw = ChooseGraphStepUnits(unit, canvas.camera.zoom,
                                               canvas.graphLabelMinPx,
                                               canvas.graphLabelMaxPx);
    int majorUnits = max(1, (int)roundf(majorUnitsRaw));
    float majorSpacing = max(minorSpacing, (float)majorUnits * unit);

    float minorPx = minorSpacing * canvas.camera.zoom;
    if (minorPx >= 4.0f) {
      float minorStartX = floorf(left / minorSpacing) * minorSpacing;
      float minorStartY = floorf(top / minorSpacing) * minorSpacing;
      for (float x = minorStartX; x <= right + minorSpacing; x += minorSpacing)
        DrawLineV({x, top - minorSpacing}, {x, bottom + minorSpacing},
                  canvas.graphMinorColor);
      for (float y = minorStartY; y <= bottom + minorSpacing; y += minorSpacing)
        DrawLineV({left - minorSpacing, y}, {right + minorSpacing, y},
                  canvas.graphMinorColor);
    }

    float majorStartX = floorf(left / majorSpacing) * majorSpacing;
    float majorStartY = floorf(top / majorSpacing) * majorSpacing;
    for (float x = majorStartX; x <= right + majorSpacing; x += majorSpacing)
      DrawLineV({x, top - majorSpacing}, {x, bottom + majorSpacing},
                canvas.graphMajorColor);
    for (float y = majorStartY; y <= bottom + majorSpacing; y += majorSpacing)
      DrawLineV({left - majorSpacing, y}, {right + majorSpacing, y},
                canvas.graphMajorColor);

    bool axisXVisible = (0.0f >= left && 0.0f <= right);
    bool axisYVisible = (0.0f >= top && 0.0f <= bottom);
    if (axisXVisible) {
      DrawLineEx({0.0f, top - majorSpacing}, {0.0f, bottom + majorSpacing},
                 2.0f, canvas.graphAxisColor);
    }
    if (axisYVisible) {
      DrawLineEx({left - majorSpacing, 0.0f}, {right + majorSpacing, 0.0f},
                 2.0f, canvas.graphAxisColor);
    }

    float labelSize = max(6.0f, canvas.graphLabelSize);
    float labelPad = 6.0f / max(0.0001f, canvas.camera.zoom);
    float labelLineY =
        axisYVisible ? 0.0f : (0.0f < top ? top + labelPad : bottom - labelPad);

    float xLabelY = labelLineY + labelPad;
    if (axisYVisible) {
      float below = 0.0f + labelPad;
      float above = 0.0f - labelPad - labelSize;
      if (below + labelSize <= bottom)
        xLabelY = below;
      else
        xLabelY = above;
    } else {
      xLabelY = (0.0f < top) ? (top + labelPad)
                             : (bottom - labelPad - labelSize);
    }
    float minLabelY = top + labelPad;
    float maxLabelY = bottom - labelPad - labelSize;
    if (maxLabelY < minLabelY)
      maxLabelY = minLabelY;
    xLabelY = min(max(xLabelY, minLabelY), maxLabelY);

    int startXi = (int)floorf(left / majorSpacing);
    int endXi = (int)ceilf(right / majorSpacing);
    for (int i = startXi; i <= endXi; ++i) {
      float x = (float)i * majorSpacing;
      int value = i * majorUnits;
      if (value == 0)
        continue;
      string label = to_string(value);
      Vector2 size = MeasureTextEx(canvas.font, label.c_str(), labelSize, 1.0f);
      DrawTextEx(canvas.font, label.c_str(),
                 {x - size.x * 0.5f, xLabelY}, labelSize, 1.0f,
                 canvas.graphLabelColor);
    }

    int startYi = (int)floorf(top / majorSpacing);
    int endYi = (int)ceilf(bottom / majorSpacing);
    for (int i = startYi; i <= endYi; ++i) {
      float y = (float)i * majorSpacing;
      int value = -i * majorUnits;
      if (value == 0)
        continue;
      string label = to_string(value);
      Vector2 size = MeasureTextEx(canvas.font, label.c_str(), labelSize, 1.0f);
      float yLabelX = 0.0f;
      if (axisXVisible) {
        yLabelX = 0.0f + labelPad;
        if (yLabelX + size.x > right)
          yLabelX = 0.0f - labelPad - size.x;
      } else if (0.0f < left) {
        yLabelX = left + labelPad;
      } else {
        yLabelX = right - labelPad - size.x;
      }
      float minLabelX = left + labelPad;
      float maxLabelX = right - labelPad - size.x;
      if (maxLabelX < minLabelX)
        maxLabelX = minLabelX;
      yLabelX = min(max(yLabelX, minLabelX), maxLabelX);
      DrawTextEx(canvas.font, label.c_str(),
                 {yLabelX, y - size.y * 0.5f}, labelSize, 1.0f,
                 canvas.graphLabelColor);
    }

    Vector2 zeroSize = MeasureTextEx(canvas.font, "0", labelSize, 1.0f);
    float zeroX = 0.0f - labelPad - zeroSize.x;
    float zeroY = 0.0f + labelPad;
    float minZeroX = left + labelPad;
    float maxZeroX = right - labelPad - zeroSize.x;
    float minZeroY = top + labelPad;
    float maxZeroY = bottom - labelPad - labelSize;
    if (maxZeroX < minZeroX)
      maxZeroX = minZeroX;
    if (maxZeroY < minZeroY)
      maxZeroY = minZeroY;
    zeroX = min(max(zeroX, minZeroX), maxZeroX);
    zeroY = min(max(zeroY, minZeroY), maxZeroY);
    DrawTextEx(canvas.font, "0", {zeroX, zeroY}, labelSize, 1.0f,
               canvas.graphLabelColor);
  }
}

void SetMode(Canvas &canvas, const AppConfig &cfg, Mode mode) {
  canvas.mode = mode;
  canvas.isTypingNumber = false;
  if (mode == SELECTION_MODE) {
    canvas.modeText = "SELECTION";
    canvas.modeColor = cfg.modeSelection;
  } else if (mode == MOVE_MODE) {
    canvas.modeText = "MOVE";
    canvas.modeColor = cfg.modeMove;
  } else if (mode == RESIZE_ROTATE_MODE) {
    canvas.modeText = "RESIZE/ROTATE";
    canvas.modeColor = cfg.modeSelection;
  } else if (mode == LINE_MODE) {
    canvas.modeText = "LINE";
    canvas.modeColor = cfg.modeLine;
  } else if (mode == DOTTEDLINE_MODE) {
    canvas.modeText = "DOTTED LINE";
    canvas.modeColor = cfg.modeLine;
  } else if (mode == ARROWLINE_MODE) {
    canvas.modeText = "ARROW LINE";
    canvas.modeColor = cfg.modeLine;
  } else if (mode == CIRCLE_MODE) {
    canvas.modeText = "CIRCLE";
    canvas.modeColor = cfg.modeCircle;
  } else if (mode == DOTTEDCIRCLE_MODE) {
    canvas.modeText = "DOTTED CIRCLE";
    canvas.modeColor = cfg.modeCircle;
  } else if (mode == RECTANGLE_MODE) {
    canvas.modeText = "RECTANGLE";
    canvas.modeColor = cfg.modeRect;
  } else if (mode == DOTTEDRECT_MODE) {
    canvas.modeText = "DOTTED RECT";
    canvas.modeColor = cfg.modeRect;
  } else if (mode == TRIANGLE_MODE) {
    canvas.modeText = "TRIANGLE";
    canvas.modeColor = cfg.modeTriangle;
  } else if (mode == DOTTEDTRIANGLE_MODE) {
    canvas.modeText = "DOTTED TRIANGLE";
    canvas.modeColor = cfg.modeTriangle;
  } else if (mode == TEXT_MODE) {
    canvas.modeText = "TEXT";
    canvas.modeColor = cfg.modeTextColor;
  } else if (mode == ERASER_MODE) {
    canvas.modeText = "ERASER";
    canvas.modeColor = cfg.modeEraser;
  } else if (mode == PEN_MODE) {
    canvas.modeText = "PEN";
    canvas.modeColor = cfg.modePen;
  }
}

void SerializeElement(ofstream &out, const Element &el) {
  out << "ELEMENT " << (int)el.type << " " << el.uniqueID << " "
      << el.strokeWidth << " " << (int)el.color.r << " " << (int)el.color.g
      << " " << (int)el.color.b << " " << (int)el.color.a << " " << el.start.x
      << " " << el.start.y << " " << el.end.x << " " << el.end.y << " "
      << el.rotation << " " << el.textSize << "\n";
  out << "TEXT " << el.text.size() << "\n" << el.text << "\n";
  out << "PATH " << el.path.size() << "\n";
  for (const auto &p : el.path)
    out << p.x << " " << p.y << "\n";
  out << "CHILDREN " << el.children.size() << "\n";
  for (const auto &c : el.children)
    SerializeElement(out, c);
  out << "END\n";
}

bool SaveCanvasToFile(const Canvas &canvas, const string &path) {
  ofstream out(path);
  if (!out.is_open())
    return false;
  out << "TOGGLE_V1\n";
  out << "TEXTSIZE " << canvas.textSize << "\n";
  out << "STROKEWIDTH " << canvas.strokeWidth << "\n";
  out << "DRAWCOLOR " << (int)canvas.drawColor.r << " " << (int)canvas.drawColor.g
      << " " << (int)canvas.drawColor.b << " " << (int)canvas.drawColor.a << "\n";
  out << "GRIDTYPE " << (int)canvas.bgType << "\n";
  out << "GRIDWIDTH " << canvas.gridWidth << "\n";
  out << "ELEMENT_COUNT " << canvas.elements.size() << "\n";
  for (const auto &el : canvas.elements)
    SerializeElement(out, el);
  return true;
}

bool DeserializeElement(istream &in, Element &el) {
  string tag;
  if (!(in >> tag) || tag != "ELEMENT")
    return false;
  string line;
  getline(in, line);
  line = Trim(line);
  stringstream ls(line);
  int type = 0;
  int r = 0, g = 0, b = 0, a = 255;
  el.textSize = 24.0f;
  el.rotation = 0.0f;
  if (!(ls >> type >> el.uniqueID >> el.strokeWidth >> r >> g >> b >> a >>
        el.start.x >> el.start.y >> el.end.x >> el.end.y))
    return false;
  vector<float> tail;
  float v = 0.0f;
  while (ls >> v)
    tail.push_back(v);
  if (tail.size() == 1) {
    el.textSize = tail[0];
  } else if (tail.size() >= 2) {
    el.rotation = tail[0];
    el.textSize = tail[1];
  }
  el.type = (Mode)type;
  el.color = {(unsigned char)r, (unsigned char)g, (unsigned char)b,
              (unsigned char)a};

  size_t textLen = 0;
  if (!(in >> tag >> textLen) || tag != "TEXT")
    return false;
  in.ignore(numeric_limits<streamsize>::max(), '\n');
  string textLine;
  getline(in, textLine);
  el.text = textLine;

  size_t pathCount = 0;
  if (!(in >> tag >> pathCount) || tag != "PATH")
    return false;
  el.path.clear();
  el.path.reserve(pathCount);
  for (size_t i = 0; i < pathCount; i++) {
    Vector2 p{};
    if (!(in >> p.x >> p.y))
      return false;
    el.path.push_back(p);
  }

  size_t childCount = 0;
  if (!(in >> tag >> childCount) || tag != "CHILDREN")
    return false;
  el.children.clear();
  el.children.reserve(childCount);
  for (size_t i = 0; i < childCount; i++) {
    Element child;
    if (!DeserializeElement(in, child))
      return false;
    el.children.push_back(child);
  }

  if (!(in >> tag) || tag != "END")
    return false;
  el.originalIndex = -1;
  return true;
}

bool LoadCanvasFromFile(Canvas &canvas, const string &path) {
  ifstream in(path);
  if (!in.is_open())
    return false;

  string magic;
  getline(in, magic);
  if (Trim(magic) != "TOGGLE_V1")
    return false;

  string tag;
  if (!(in >> tag) || tag != "TEXTSIZE")
    return false;
  in >> canvas.textSize;

  if (!(in >> tag) || tag != "STROKEWIDTH")
    return false;
  in >> canvas.strokeWidth;

  int r, g, b, a;
  if (!(in >> tag) || tag != "DRAWCOLOR")
    return false;
  if (!(in >> r >> g >> b >> a))
    return false;
  canvas.drawColor = {(unsigned char)r, (unsigned char)g, (unsigned char)b,
                      (unsigned char)a};

  int bgType;
  if (!(in >> tag) || tag != "GRIDTYPE")
    return false;
  if (!(in >> bgType))
    return false;
  canvas.bgType = (BackgroundType)bgType;

  if (!(in >> tag) || tag != "GRIDWIDTH")
    return false;
  in >> canvas.gridWidth;

  size_t count = 0;
  if (!(in >> tag) || tag != "ELEMENT_COUNT")
    return false;
  if (!(in >> count))
    return false;

  vector<Element> loaded;
  loaded.reserve(count);
  for (size_t i = 0; i < count; i++) {
    Element el;
    if (!DeserializeElement(in, el))
      return false;
    loaded.push_back(el);
  }

  canvas.elements = loaded;
  canvas.selectedIndices.clear();
  canvas.undoStack.clear();
  canvas.redoStack.clear();
  canvas.isTextEditing = false;
  canvas.commandMode = false;
  return true;
}

string SvgEscape(const string &text) {
  string out;
  out.reserve(text.size());
  for (char c : text) {
    if (c == '&')
      out += "&amp;";
    else if (c == '<')
      out += "&lt;";
    else if (c == '>')
      out += "&gt;";
    else if (c == '"')
      out += "&quot;";
    else if (c == '\'')
      out += "&apos;";
    else
      out.push_back(c);
  }
  return out;
}

void WriteSvgElement(ofstream &out, const Element &el, const string &fontFamily,
                     float textSize, const Camera2D &camera) {
  string stroke = TextFormat("rgb(%d,%d,%d)", el.color.r, el.color.g, el.color.b);
  Vector2 s = el.start;
  Vector2 e = el.end;
  if (el.rotation != 0.0f &&
      (el.type == LINE_MODE || el.type == DOTTEDLINE_MODE ||
       el.type == ARROWLINE_MODE)) {
    Vector2 center = ElementCenterLocal(el);
    s = RotatePoint(s, center, el.rotation);
    e = RotatePoint(e, center, el.rotation);
  }
  s = GetWorldToScreen2D(s, camera);
  e = GetWorldToScreen2D(e, camera);
  float scaledStroke = max(0.5f, el.strokeWidth * camera.zoom);
  float effectiveTextSize = (el.textSize > 0.0f) ? el.textSize : textSize;
  float scaledTextSize = max(6.0f, effectiveTextSize * camera.zoom);
  if (el.type == LINE_MODE || el.type == DOTTEDLINE_MODE || el.type == ARROWLINE_MODE) {
    out << "<line x1=\"" << s.x << "\" y1=\"" << s.y << "\" x2=\""
        << e.x << "\" y2=\"" << e.y << "\" stroke=\"" << stroke
        << "\" stroke-width=\"" << scaledStroke << "\"";
    if (el.type == DOTTEDLINE_MODE)
      out << " stroke-dasharray=\"8,6\"";
    out << " fill=\"none\" stroke-linecap=\"round\" />\n";
    if (el.type == ARROWLINE_MODE) {
      float angle = atan2f(e.y - s.y, e.x - s.x);
      float lineLen = Vector2Distance(s, e);
      float headSize = max(12.0f, scaledStroke * 3.0f);
      if (headSize > lineLen * 0.7f)
        headSize = lineLen * 0.7f;
      Vector2 p1 = {e.x - headSize * cosf(angle - PI / 6),
                    e.y - headSize * sinf(angle - PI / 6)};
      Vector2 p2 = {e.x - headSize * cosf(angle + PI / 6),
                    e.y - headSize * sinf(angle + PI / 6)};
      out << "<line x1=\"" << e.x << "\" y1=\"" << e.y << "\" x2=\"" << p1.x
          << "\" y2=\"" << p1.y << "\" stroke=\"" << stroke
          << "\" stroke-width=\"" << scaledStroke
          << "\" fill=\"none\" stroke-linecap=\"round\" />\n";
      out << "<line x1=\"" << e.x << "\" y1=\"" << e.y << "\" x2=\"" << p2.x
          << "\" y2=\"" << p2.y << "\" stroke=\"" << stroke
          << "\" stroke-width=\"" << scaledStroke
          << "\" fill=\"none\" stroke-linecap=\"round\" />\n";
    }
  } else if (el.type == RECTANGLE_MODE || el.type == DOTTEDRECT_MODE) {
    float x = min(s.x, e.x);
    float y = min(s.y, e.y);
    float w = fabsf(e.x - s.x);
    float h = fabsf(e.y - s.y);
    out << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w
        << "\" height=\"" << h << "\" stroke=\"" << stroke
        << "\" stroke-width=\"" << scaledStroke << "\" fill=\"none\"";
    if (el.rotation != 0.0f) {
      Vector2 center = ElementCenterLocal(el);
      Vector2 cs = GetWorldToScreen2D(center, camera);
      out << " transform=\"rotate(" << (el.rotation * RAD2DEG) << " " << cs.x
          << " " << cs.y << ")\"";
    }
    if (el.type == DOTTEDRECT_MODE)
      out << " stroke-dasharray=\"8,6\"";
    out << " />\n";
  } else if (el.type == CIRCLE_MODE || el.type == DOTTEDCIRCLE_MODE) {
    float r = Vector2Distance(s, e);
    out << "<circle cx=\"" << s.x << "\" cy=\"" << s.y
        << "\" r=\"" << r << "\" stroke=\"" << stroke << "\" stroke-width=\""
        << scaledStroke << "\" fill=\"none\"";
    if (el.type == DOTTEDCIRCLE_MODE)
      out << " stroke-dasharray=\"8,6\"";
    out << " />\n";
  } else if (el.type == TRIANGLE_MODE || el.type == DOTTEDTRIANGLE_MODE) {
    Vector2 apex, left, right;
    GetTriangleVerticesLocal(el, apex, left, right);
    if (el.rotation != 0.0f) {
      Vector2 center = ElementCenterLocal(el);
      apex = RotatePoint(apex, center, el.rotation);
      left = RotatePoint(left, center, el.rotation);
      right = RotatePoint(right, center, el.rotation);
    }
    Vector2 a = GetWorldToScreen2D(apex, camera);
    Vector2 b = GetWorldToScreen2D(left, camera);
    Vector2 c = GetWorldToScreen2D(right, camera);
    out << "<polygon points=\"" << a.x << "," << a.y << " " << b.x << ","
        << b.y << " " << c.x << "," << c.y << "\" stroke=\"" << stroke
        << "\" stroke-width=\"" << scaledStroke
        << "\" fill=\"none\" stroke-linejoin=\"round\"";
    if (el.type == DOTTEDTRIANGLE_MODE)
      out << " stroke-dasharray=\"8,6\"";
    out << " />\n";
  } else if (el.type == PEN_MODE) {
    if (el.path.size() >= 2) {
      out << "<polyline points=\"";
      Vector2 center = ElementCenterLocal(el);
      for (const auto &p : el.path) {
        Vector2 wp = p;
        if (el.rotation != 0.0f)
          wp = RotatePoint(wp, center, el.rotation);
        Vector2 sp = GetWorldToScreen2D(wp, camera);
        out << sp.x << "," << sp.y << " ";
      }
      out << "\" stroke=\"" << stroke << "\" stroke-width=\"" << scaledStroke
          << "\" fill=\"none\" stroke-linecap=\"round\" stroke-linejoin=\"round\" />\n";
    } else if (el.path.size() == 1) {
      Vector2 wp = el.path[0];
      if (el.rotation != 0.0f) {
        Vector2 center = ElementCenterLocal(el);
        wp = RotatePoint(wp, center, el.rotation);
      }
      Vector2 sp = GetWorldToScreen2D(wp, camera);
      out << "<circle cx=\"" << sp.x << "\" cy=\"" << sp.y
          << "\" r=\"" << max(0.5f, el.strokeWidth * 0.5f * camera.zoom)
          << "\" fill=\""
          << stroke << "\" />\n";
    }
  } else if (el.type == TEXT_MODE) {
    out << "<text x=\"" << s.x << "\" y=\"" << (s.y + scaledTextSize)
        << "\" fill=\"" << stroke << "\" font-family=\"" << SvgEscape(fontFamily)
        << "\" font-size=\"" << scaledTextSize << "\"";
    if (el.rotation != 0.0f) {
      Vector2 center = ElementCenterLocal(el);
      Vector2 cs = GetWorldToScreen2D(center, camera);
      out << " transform=\"rotate(" << (el.rotation * RAD2DEG) << " " << cs.x
          << " " << cs.y << ")\"";
    }
    out << ">" << SvgEscape(el.text) << "</text>\n";
  } else if (el.type == GROUP_MODE) {
    for (const auto &child : el.children)
      WriteSvgElement(out, child, fontFamily, textSize, camera);
  }
}

bool ExportCanvasSvg(const Canvas &canvas, const string &filename,
                     const vector<Element> &elements, const Camera2D &camera,
                     int outWidth, int outHeight) {
  ofstream out(filename);
  if (!out.is_open())
    return false;
  int w = outWidth;
  int h = outHeight;
  out << fixed << setprecision(3);
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << w
      << "\" height=\"" << h << "\" viewBox=\"0 0 " << w << " " << h << "\">\n";
  out << "<rect width=\"100%\" height=\"100%\" fill=\"rgb("
      << (int)canvas.backgroundColor.r << "," << (int)canvas.backgroundColor.g << ","
      << (int)canvas.backgroundColor.b << ")\" />\n";
  for (const auto &el : elements)
    WriteSvgElement(out, el, canvas.fontFamilyPath, canvas.textSize, camera);
  out << "</svg>\n";
  return true;
}

bool ExportCanvasRaster(const Canvas &canvas, const string &filename,
                        const vector<Element> &elements, const Camera2D &camera,
                        int outWidth, int outHeight) {
  RenderTexture2D target = LoadRenderTexture(outWidth, outHeight);
  BeginTextureMode(target);
  ClearBackground(canvas.backgroundColor);
  BeginMode2D(camera);
  for (const auto &el : elements)
    DrawElement(el, canvas.font, canvas.textSize);
  EndMode2D();
  EndTextureMode();

  Image img = LoadImageFromTexture(target.texture);
  ImageFlipVertical(&img);
  bool ok = ExportImage(img, filename.c_str());
  UnloadImage(img);
  UnloadRenderTexture(target);
  return ok;
}

bool TryLoadFont(Canvas &canvas, const AppConfig &cfg, const string &nameOrPath) {
  string path = Trim(nameOrPath);
  if (path.empty())
    return false;
  if (ToLower(path) == "default") {
    if (canvas.ownsFont)
      UnloadFont(canvas.font);
    canvas.font = GetFontDefault();
    canvas.ownsFont = false;
    canvas.fontFamilyPath = "default";
    return true;
  }
  if (!FileExists(path.c_str()))
    return false;
  int atlas = max(16, cfg.fontAtlasSize);
  Font newFont = LoadFontEx(path.c_str(), atlas, nullptr, 0);
  if (newFont.texture.id == 0)
    return false;
  SetTextureFilter(newFont.texture, TEXTURE_FILTER_BILINEAR);
  if (canvas.ownsFont)
    UnloadFont(canvas.font);
  canvas.font = newFont;
  canvas.ownsFont = true;
  canvas.fontFamilyPath = path;
  return true;
}

bool BuildExportScene(const Canvas &canvas, ExportScope scope, float rasterScale,
                      vector<Element> &elementsOut, Camera2D &cameraOut,
                      int &widthOut, int &heightOut, string &errorOut) {
  elementsOut.clear();
  if (scope == EXPORT_SELECTED) {
    vector<int> ids = GetSelectedIDs(canvas);
    for (int id : ids) {
      int idx = FindElementIndexByID(canvas, id);
      if (idx >= 0 && idx < (int)canvas.elements.size())
        elementsOut.push_back(canvas.elements[idx]);
    }
    if (elementsOut.empty()) {
      errorOut = "No selected elements to export";
      return false;
    }
  } else {
    elementsOut = canvas.elements;
  }

  if (scope == EXPORT_FRAME) {
    float scale = max(1.0f, rasterScale);
    cameraOut = canvas.camera;
    cameraOut.zoom *= scale;
    cameraOut.offset = {canvas.camera.offset.x * scale,
                        canvas.camera.offset.y * scale};
    widthOut = max(1, (int)roundf((float)GetScreenWidth() * scale));
    heightOut = max(1, (int)roundf((float)GetScreenHeight() * scale));
    return true;
  }

  Rectangle bounds{};
  if (!UnionBounds(elementsOut, bounds)) {
    errorOut = "Nothing to export";
    return false;
  }

  float scale = max(1.0f, rasterScale);
  float pad = max(12.0f, 24.0f * scale);
  Rectangle padded = ExpandRect(bounds, pad / scale);
  widthOut = max(1, (int)ceilf(padded.width * scale));
  heightOut = max(1, (int)ceilf(padded.height * scale));

  cameraOut = {};
  cameraOut.rotation = 0.0f;
  cameraOut.zoom = scale;
  cameraOut.target = {padded.x, padded.y};
  cameraOut.offset = {0.0f, 0.0f};
  return true;
}

void ExecuteCommand(Canvas &canvas, AppConfig &cfg, string command) {
  command = Trim(command);
  if (command.empty())
    return;
  if (command[0] == ':')
    command = command.substr(1);
  command = Trim(command);
  vector<string> tokens = TokenizeQuotedArgs(command);
  if (tokens.empty())
    return;
  string op = tokens[0];
  vector<string> args;
  for (size_t i = 1; i < tokens.size(); i++)
    args.push_back(StripQuotes(tokens[i]));
  string opLower = ToLower(op);

  if (opLower == "q") {
    canvas.shouldQuit = true;
    return;
  }
  if (opLower == "w" || opLower == "wq") {
    string targetPath;
    if (args.empty()) {
      targetPath = canvas.savePath;
      if (targetPath.empty()) {
        string dir = ResolveDefaultDir(cfg.defaultSaveDir, DefaultDownloadsDir());
        targetPath = JoinPath(dir, "untitled.toggle");
      }
    } else if (args.size() == 1) {
      filesystem::path p = ExpandUserPath(args[0]);
      string filename = p.filename().string();
      if (filename.empty())
        filename = "untitled.toggle";
      filename = EnsureExt(filename, "toggle");
      if (p.has_parent_path()) {
        targetPath = (p.parent_path() / filename).string();
      } else {
        string dir = ResolveDefaultDir(cfg.defaultSaveDir, DefaultDownloadsDir());
        targetPath = JoinPath(dir, filename);
      }
    } else {
      string filename = EnsureExt(args[0], "toggle");
      string dir = ResolveDefaultDir(args[1], ResolveDefaultDir(cfg.defaultSaveDir,
                                                                 DefaultDownloadsDir()));
      targetPath = JoinPath(dir, filename);
    }

    filesystem::path target = ExpandUserPath(targetPath);
    if (!EnsureDirectory(target.parent_path().string())) {
      SetStatus(canvas, cfg, "Save failed: could not create directory");
      return;
    }
    targetPath = target.string();
    if (SaveCanvasToFile(canvas, targetPath)) {
      canvas.savePath = targetPath;
      SetStatus(canvas, cfg, "Saved to " + targetPath);
      if (opLower == "wq")
        canvas.shouldQuit = true;
    } else {
      SetStatus(canvas, cfg, "Save failed: " + targetPath);
    }
    return;
  }
  if (opLower == "o" || opLower == "open") {
    string sourcePath;
    if (args.empty()) {
      if (canvas.savePath.empty()) {
        SetStatus(canvas, cfg, "Usage: :open 'filename' ['path']");
        return;
      }
      sourcePath = canvas.savePath;
    } else if (args.size() == 1) {
      filesystem::path p = ExpandUserPath(args[0]);
      if (p.extension().string().empty())
        p.replace_extension(".toggle");
      if (p.has_parent_path()) {
        sourcePath = p.string();
      } else {
        string dir =
            ResolveDefaultDir(cfg.defaultOpenDir, ResolveDefaultDir(cfg.defaultSaveDir,
                                                                    DefaultDownloadsDir()));
        sourcePath = JoinPath(dir, p.string());
      }
    } else {
      string filename = args[0];
      if (filesystem::path(filename).extension().string().empty())
        filename = EnsureExt(filename, "toggle");
      sourcePath = JoinPath(ResolveDefaultDir(args[1], cfg.defaultOpenDir), filename);
    }

    if (LoadCanvasFromFile(canvas, sourcePath)) {
      canvas.savePath = sourcePath;
      NormalizeCanvasIDs(canvas);
      SetStatus(canvas, cfg, "Opened " + sourcePath);
    } else {
      SetStatus(canvas, cfg, "Open failed: " + sourcePath);
    }
    return;
  }
  if (opLower == "theme") {
    string v = args.empty() ? "" : ToLower(args[0]);
    if (v == "dark") {
      SetTheme(canvas, cfg, true);
      SetStatus(canvas, cfg, "Theme set to dark");
    } else if (v == "light") {
      SetTheme(canvas, cfg, false);
      SetStatus(canvas, cfg, "Theme set to light");
    } else {
      SetStatus(canvas, cfg, "Usage: :theme dark|light");
    }
    return;
  }
  if (opLower == "font") {
    float size = 0.0f;
    if (args.empty() || !ParsePositiveFloat(args[0], size) ||
        size < cfg.minTextSize ||
        size > cfg.maxTextSize) {
      SetStatus(canvas, cfg,
                "Usage: :font [" + to_string((int)cfg.minTextSize) + "-" +
                    to_string((int)cfg.maxTextSize) + "]");
      return;
    }
    vector<int> selectedIDs = GetSelectedIDs(canvas);
    if (!selectedIDs.empty()) {
      SaveBackup(canvas);
      for (int id : selectedIDs) {
        int idx = FindElementIndexByID(canvas, id);
        if (idx == -1)
          continue;
        ApplyTextSizeRecursive(canvas.elements[idx], size, canvas.font,
                               canvas.textSize);
      }
      SetStatus(canvas, cfg,
                "Font size applied to selected text: " + to_string((int)size));
    } else {
      canvas.textSize = size;
      SetStatus(canvas, cfg, "Default font size set to " + to_string((int)size));
    }
    return;
  }
  if (opLower == "font-family") {
    if (!args.empty() && TryLoadFont(canvas, cfg, args[0])) {
      for (auto &el : canvas.elements)
        RecomputeTextBoundsRecursive(el, canvas.font, canvas.textSize);
      SetStatus(canvas, cfg, "Font family set to " + args[0]);
    } else
      SetStatus(canvas, cfg, "Font load failed");
    return;
  }
  if (opLower == "color") {
    Color c{};
    if (args.empty() ||
        (!ParseHexColor(args[0], c) && !ParseNamedColor(args[0], c))) {
      SetStatus(canvas, cfg,
                "Usage: :color #RRGGBB/#RRGGBBAA or color name");
      return;
    }
    vector<int> selectedIDs = GetSelectedIDs(canvas);
    if (!selectedIDs.empty()) {
      SaveBackup(canvas);
      for (int id : selectedIDs) {
        int idx = FindElementIndexByID(canvas, id);
        if (idx == -1)
          continue;
        ApplyColorRecursive(canvas.elements[idx], c);
      }
      SetStatus(canvas, cfg, "Color applied to selection: " + ColorToHex(c));
    } else {
      canvas.drawColor = c;
      SetStatus(canvas, cfg, "Default draw color set to " + ColorToHex(c));
    }
    return;
  }
  if (opLower == "strokew") {
    float w = 0.0f;
    if (args.empty() || !ParsePositiveFloat(args[0], w) ||
        w < cfg.minStrokeWidth ||
        w > cfg.maxStrokeWidth) {
      SetStatus(canvas, cfg,
                "Usage: :strokew [" + to_string((int)cfg.minStrokeWidth) + "-" +
                    to_string((int)cfg.maxStrokeWidth) + "]");
      return;
    }
    vector<int> selectedIDs = GetSelectedIDs(canvas);
    if (!selectedIDs.empty()) {
      SaveBackup(canvas);
      for (int id : selectedIDs) {
        int idx = FindElementIndexByID(canvas, id);
        if (idx == -1)
          continue;
        ApplyStrokeRecursive(canvas.elements[idx], w);
      }
      SetStatus(canvas, cfg, "Stroke width applied to selection");
    } else {
      canvas.strokeWidth = w;
      SetStatus(canvas, cfg, "Default stroke width set");
    }
    return;
  }
  if (opLower == "gridw") {
    float gw = 0.0f;
    if (args.empty() || !ParsePositiveFloat(args[0], gw) || gw < 6.0f ||
        gw > 200.0f) {
      SetStatus(canvas, cfg, "Usage: :gridw [6-200]");
      return;
    }
    canvas.gridWidth = gw;
    SetStatus(canvas, cfg, "Grid spacing set");
    return;
  }
  if (opLower == "graph") {
    string v = args.empty() ? "toggle" : ToLower(args[0]);
    if (v == "on" || v == "graph") {
      canvas.bgType = BG_GRAPH;
      canvas.camera.target = {0.0f, 0.0f};
      SetStatus(canvas, cfg, "Graph mode on");
    } else if (v == "off" || v == "blank") {
      canvas.bgType = BG_BLANK;
      SetStatus(canvas, cfg, "Graph mode off");
    } else if (v == "toggle") {
      if (canvas.bgType == BG_GRAPH) {
        canvas.bgType = BG_BLANK;
        SetStatus(canvas, cfg, "Graph mode off");
      } else {
        canvas.bgType = BG_GRAPH;
        canvas.camera.target = {0.0f, 0.0f};
        SetStatus(canvas, cfg, "Graph mode on");
      }
    } else {
      SetStatus(canvas, cfg, "Usage: :graph on|off|toggle");
    }
    return;
  }
  if (opLower == "graphunit") {
    float v = 0.0f;
    if (args.empty() || !ParsePositiveFloat(args[0], v) || v < 0.1f) {
      SetStatus(canvas, cfg, "Usage: :graphunit [>0]");
      return;
    }
    canvas.graphUnit = v;
    SetStatus(canvas, cfg, "Graph unit set");
    return;
  }
  if (opLower == "graphminor") {
    float v = 0.0f;
    if (args.empty() || !ParsePositiveFloat(args[0], v) || v < 0.1f) {
      SetStatus(canvas, cfg, "Usage: :graphminor [>0]");
      return;
    }
    canvas.graphMinorSpacing = v;
    SetStatus(canvas, cfg, "Graph minor spacing set");
    return;
  }
  if (opLower == "graphlabel") {
    float v = 0.0f;
    if (args.empty() || !ParsePositiveFloat(args[0], v) || v < 6.0f) {
      SetStatus(canvas, cfg, "Usage: :graphlabel [>=6]");
      return;
    }
    canvas.graphLabelSize = v;
    SetStatus(canvas, cfg, "Graph label size set");
    return;
  }
  if (opLower == "graphspacing") {
    float minPx = 0.0f;
    float maxPx = 0.0f;
    if (args.empty() || !ParsePositiveFloat(args[0], minPx) || minPx < 10.0f) {
      SetStatus(canvas, cfg, "Usage: :graphspacing min_px max_px");
      return;
    }
    if (args.size() >= 2 && ParsePositiveFloat(args[1], maxPx)) {
      maxPx = max(10.0f, maxPx);
    } else {
      maxPx = minPx;
    }
    if (maxPx < minPx)
      maxPx = minPx;
    canvas.graphLabelMinPx = minPx;
    canvas.graphLabelMaxPx = maxPx;
    SetStatus(canvas, cfg, "Graph label spacing set");
    return;
  }
  if (opLower == "type") {
    string v = args.empty() ? "" : ToLower(args[0]);
    if (v == "blank") {
      canvas.bgType = BG_BLANK;
      SetStatus(canvas, cfg, "Canvas type: blank");
    } else if (v == "grid") {
      canvas.bgType = BG_GRID;
      SetStatus(canvas, cfg, "Canvas type: grid");
    } else if (v == "dotted") {
      canvas.bgType = BG_DOTTED;
      SetStatus(canvas, cfg, "Canvas type: dotted");
    } else if (v == "graph") {
      canvas.bgType = BG_GRAPH;
      canvas.camera.target = {0.0f, 0.0f};
      SetStatus(canvas, cfg, "Canvas type: graph");
    } else {
      SetStatus(canvas, cfg, "Usage: :type blank|grid|dotted|graph");
    }
    return;
  }
  if (opLower == "resizet" || opLower == "resizeb" || opLower == "resizel" ||
      opLower == "resizer") {
    int delta = 0;
    if (args.empty() || !ParseIntValue(args[0], delta)) {
      SetStatus(canvas, cfg, "Usage: :" + opLower + " [number]");
      return;
    }
    int minW = cfg.minWindowWidth;
    int minH = cfg.minWindowHeight;
    int x = GetWindowPosition().x;
    int y = GetWindowPosition().y;
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    if (opLower == "resizet") {
      y -= delta;
      h += delta;
    } else if (opLower == "resizeb") {
      h += delta;
    } else if (opLower == "resizel") {
      x -= delta;
      w += delta;
    } else if (opLower == "resizer") {
      w += delta;
    }
    w = max(minW, w);
    h = max(minH, h);
    SetWindowSize(w, h);
    SetWindowPosition(x, y);
    SetStatus(canvas, cfg, "Window resized");
    return;
  }
  if (opLower == "export") {
    vector<string> normalized;
    normalized.reserve(args.size());
    for (const auto &a : args)
      normalized.push_back(StripQuotes(a));

    int typeIdx = -1;
    string type = "png";
    ExportScope scope = EXPORT_ALL;
    bool scopeSet = false;
    for (int i = 0; i < (int)normalized.size(); i++) {
      if (IsExportType(normalized[i])) {
        typeIdx = i;
        type = NormalizeExportType(normalized[i]);
        continue;
      }
      if (IsExportScopeToken(normalized[i])) {
        scope = ParseExportScope(normalized[i]);
        scopeSet = true;
      }
    }

    vector<string> rest;
    for (int i = 0; i < (int)normalized.size(); i++) {
      bool isType = (i == typeIdx) && IsExportType(normalized[i]);
      bool isScope = IsExportScopeToken(normalized[i]);
      if (!isType && !isScope)
        rest.push_back(normalized[i]);
    }

    filesystem::path basePath =
        canvas.savePath.empty() ? filesystem::path("untitled")
                                : filesystem::path(canvas.savePath).stem();
    string filename = basePath.filename().string();
    if (filename.empty())
      filename = "untitled";
    string outDir =
        ResolveDefaultDir(cfg.defaultExportDir, ResolveDefaultDir(cfg.defaultSaveDir,
                                                                  DefaultDownloadsDir()));

    if (rest.size() == 1) {
      if (LooksLikeDirPath(rest[0]))
        outDir = ExpandUserPath(rest[0]);
      else
        filename = filesystem::path(rest[0]).stem().string();
    } else if (rest.size() >= 2) {
      bool firstLooksDir = LooksLikeDirPath(rest[0]) || HasDirectoryPart(rest[0]);
      bool secondLooksDir = LooksLikeDirPath(rest[1]) || HasDirectoryPart(rest[1]);
      if (firstLooksDir && !secondLooksDir) {
        outDir = ExpandUserPath(rest[0]);
        filename = filesystem::path(rest[1]).stem().string();
      } else {
        filename = filesystem::path(rest[0]).stem().string();
        outDir = ExpandUserPath(rest[1]);
      }
    }

    if (filename.empty())
      filename = "untitled";
    string outName = EnsureExt(filename, type);
    if (!EnsureDirectory(outDir)) {
      SetStatus(canvas, cfg, "Export failed: could not create directory");
      return;
    }
    string fullPath = JoinPath(outDir, outName);

    vector<Element> exportElements;
    Camera2D exportCamera{};
    int exportW = 0;
    int exportH = 0;
    string exportErr;
    float sceneScale = (type == "svg") ? 1.0f : cfg.exportRasterScale;
    if (!BuildExportScene(canvas, scope, sceneScale, exportElements,
                          exportCamera, exportW, exportH, exportErr)) {
      SetStatus(canvas, cfg, "Export failed: " + exportErr);
      return;
    }

    bool ok = false;
    if (type == "svg")
      ok = ExportCanvasSvg(canvas, fullPath, exportElements, exportCamera, exportW,
                           exportH);
    else
      ok = ExportCanvasRaster(canvas, fullPath, exportElements, exportCamera,
                              exportW, exportH);

    if (ok)
      SetStatus(canvas, cfg, "Exported " + fullPath);
    else
      SetStatus(canvas, cfg, "Export failed");
    return;
  }

  if (opLower == "reloadconfig") {
    SetDefaultKeymap(cfg);
    LoadConfig(cfg);
    SetTheme(canvas, cfg, canvas.darkTheme);
    SetStatus(canvas, cfg, "Config reloaded");
    return;
  }

  if (opLower == "writeconfig") {
    WriteDefaultConfig(cfg);
    SetStatus(canvas, cfg, "Config written to " + cfg.configPath);
    return;
  }

  SetStatus(canvas, cfg, "Unknown command: " + op);
}

int main() {
  AppConfig cfg;
  SetDefaultKeymap(cfg);
  LoadConfig(cfg);

  int screenWidth = cfg.windowWidth;
  int screenHeight = cfg.windowHeight;
  Canvas canvas;
  unsigned int windowFlags = FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE;
  if (cfg.startMaximized)
    windowFlags |= FLAG_WINDOW_MAXIMIZED;
  SetConfigFlags(windowFlags);
  InitWindow(screenWidth, screenHeight, cfg.windowTitle.c_str());
  if (cfg.startMaximized)
    MaximizeWindow();
  SetWindowMinSize(cfg.minWindowWidth, cfg.minWindowHeight);
  SetExitKey(KEY_NULL);
  SetTargetFPS(cfg.targetFps);
  canvas.camera.offset = {0.0f, 0.0f};
  canvas.camera.target = {0.0f, 0.0f};
  canvas.camera.rotation = 0.0f;
  canvas.camera.zoom = 1.0f;
  canvas.font =
      LoadFontEx(cfg.defaultFontPath.c_str(), max(16, cfg.fontAtlasSize), nullptr, 0);
  if (canvas.font.texture.id == 0) {
    canvas.font = GetFontDefault();
    canvas.ownsFont = false;
    canvas.fontFamilyPath = "default";
  } else {
    canvas.ownsFont = true;
    canvas.fontFamilyPath = cfg.defaultFontPath;
  }
  SetTextureFilter(canvas.font.texture, TEXTURE_FILTER_BILINEAR);
  canvas.strokeWidth = cfg.defaultStrokeWidth;
  canvas.textSize = cfg.defaultTextSize;
  canvas.gridWidth = cfg.defaultGridWidth;
  canvas.graphUnit = cfg.defaultGraphUnit;
  canvas.graphMinorSpacing = cfg.defaultGraphMinorSpacing;
  canvas.graphLabelSize = cfg.defaultGraphLabelSize;
  canvas.graphLabelMinPx = cfg.defaultGraphLabelMinPx;
  canvas.graphLabelMaxPx = cfg.defaultGraphLabelMaxPx;
  canvas.drawColor = cfg.defaultDrawColor;
  canvas.showTags = cfg.defaultShowTags;
  canvas.bgType = cfg.defaultBgType;
  SetTheme(canvas, cfg, cfg.defaultDarkTheme);
  SetMode(canvas, cfg, PEN_MODE);
  canvas.lastMouseScreen = GetMousePosition();

  while (!WindowShouldClose()) {
    bool escPressed = IsKeyPressed(KEY_ESCAPE);
    int key = 0;
    int polledKey = 0;
    while ((polledKey = GetKeyPressed()) != 0) {
      key = polledKey;
      if (polledKey == KEY_ESCAPE)
        escPressed = true;
    }
    bool shiftDown = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool ctrlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool altDown = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    if (canvas.bgType == BG_GRAPH) {
      canvas.camera.offset = {(float)GetScreenWidth() * 0.5f,
                              (float)GetScreenHeight() * 0.5f};
    }
    if (!canvas.isTextEditing && !canvas.commandMode &&
        IsKeyPressed(KEY_O)) {
      canvas.antiMouseMode = !canvas.antiMouseMode;
      if (canvas.antiMouseMode) {
        canvas.antiMousePos = GetMousePosition();
        canvas.antiMouseVel = {0.0f, 0.0f};
        HideCursor();
      } else {
        ShowCursor();
      }
      canvas.lastMouseScreen =
          canvas.antiMouseMode ? canvas.antiMousePos : GetMousePosition();
    }
    if (canvas.antiMouseMode && !canvas.isTextEditing && !canvas.commandMode &&
        IsKeyPressed(KEY_M)) {
      SetMode(canvas, cfg, MOVE_MODE);
    }
    if (canvas.antiMouseMode && !canvas.isTextEditing && !canvas.commandMode) {
      const float maxSpeed = 900.0f;
      const float accel = 4200.0f;
      const float tapStep = 2.0f;
      float dt = GetFrameTime();
      Vector2 dir = {0.0f, 0.0f};
      Vector2 pressDir = {0.0f, 0.0f};
      bool pressedMove = false;
      if (ctrlDown) {
        if (IsKeyDown(KEY_W))
          dir.y -= 1.0f;
        if (IsKeyDown(KEY_S))
          dir.y += 1.0f;
        if (IsKeyDown(KEY_A))
          dir.x -= 1.0f;
        if (IsKeyDown(KEY_D))
          dir.x += 1.0f;
        if (IsKeyPressed(KEY_W)) {
          pressDir.y -= 1.0f;
          pressedMove = true;
        }
        if (IsKeyPressed(KEY_S)) {
          pressDir.y += 1.0f;
          pressedMove = true;
        }
        if (IsKeyPressed(KEY_A)) {
          pressDir.x -= 1.0f;
          pressedMove = true;
        }
        if (IsKeyPressed(KEY_D)) {
          pressDir.x += 1.0f;
          pressedMove = true;
        }
      }
      if (pressedMove && (pressDir.x != 0.0f || pressDir.y != 0.0f)) {
        pressDir = Vector2Normalize(pressDir);
        canvas.antiMousePos.x += pressDir.x * tapStep;
        canvas.antiMousePos.y += pressDir.y * tapStep;
      }
      if (dir.x != 0.0f || dir.y != 0.0f) {
        dir = Vector2Normalize(dir);
        canvas.antiMouseVel.x += dir.x * accel * dt;
        canvas.antiMouseVel.y += dir.y * accel * dt;
        float speed = Vector2Length(canvas.antiMouseVel);
        if (speed > maxSpeed) {
          canvas.antiMouseVel =
              Vector2Scale(Vector2Normalize(canvas.antiMouseVel), maxSpeed);
        }
      } else {
        canvas.antiMouseVel = {0.0f, 0.0f};
      }
      canvas.antiMousePos.x += canvas.antiMouseVel.x * dt;
      canvas.antiMousePos.y += canvas.antiMouseVel.y * dt;
      canvas.antiMousePos.x =
          Clamp(canvas.antiMousePos.x, 0.0f, (float)GetScreenWidth());
      canvas.antiMousePos.y =
          Clamp(canvas.antiMousePos.y, 0.0f, (float)GetScreenHeight());
    }
    Vector2 mouseScreen =
        canvas.antiMouseMode ? canvas.antiMousePos : GetMousePosition();
    Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, canvas.camera);
    Vector2 mouseDelta = Vector2Subtract(mouseScreen, canvas.lastMouseScreen);
    bool mouseLeftPressed = canvas.antiMouseMode
                                ? IsKeyPressed(KEY_COMMA)
                                : IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool mouseLeftDown = canvas.antiMouseMode
                             ? IsKeyDown(KEY_COMMA)
                             : IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    bool mouseLeftReleased = canvas.antiMouseMode
                                 ? IsKeyReleased(KEY_COMMA)
                                 : IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

    if (!canvas.isTextEditing && !canvas.commandMode && shiftDown &&
        !canvas.selectedIndices.empty()) {
      const float maxSpeed = 900.0f;
      const float accel = 4200.0f;
      const float tapStep = 2.0f;
      float dt = GetFrameTime();
      Vector2 dir = {0.0f, 0.0f};
      Vector2 pressDir = {0.0f, 0.0f};
      bool pressedMove = false;
      if (IsKeyDown(KEY_W))
        dir.y -= 1.0f;
      if (IsKeyDown(KEY_S))
        dir.y += 1.0f;
      if (IsKeyDown(KEY_A))
        dir.x -= 1.0f;
      if (IsKeyDown(KEY_D))
        dir.x += 1.0f;
      if (IsKeyPressed(KEY_W)) {
        pressDir.y -= 1.0f;
        pressedMove = true;
      }
      if (IsKeyPressed(KEY_S)) {
        pressDir.y += 1.0f;
        pressedMove = true;
      }
      if (IsKeyPressed(KEY_A)) {
        pressDir.x -= 1.0f;
        pressedMove = true;
      }
      if (IsKeyPressed(KEY_D)) {
        pressDir.x += 1.0f;
        pressedMove = true;
      }
      if (pressedMove && (pressDir.x != 0.0f || pressDir.y != 0.0f)) {
        pressDir = Vector2Normalize(pressDir);
        Vector2 tap = {pressDir.x * tapStep / canvas.camera.zoom,
                       pressDir.y * tapStep / canvas.camera.zoom};
        if (!canvas.keyMoveActive) {
          SaveBackup(canvas);
          canvas.keyMoveActive = true;
        }
        for (int idx : canvas.selectedIndices) {
          if (idx >= 0 && idx < (int)canvas.elements.size())
            MoveElement(canvas.elements[idx], tap);
        }
      }
      if (dir.x != 0.0f || dir.y != 0.0f) {
        dir = Vector2Normalize(dir);
        canvas.keyMoveVel.x += dir.x * accel * dt;
        canvas.keyMoveVel.y += dir.y * accel * dt;
        float speed = Vector2Length(canvas.keyMoveVel);
        if (speed > maxSpeed) {
          canvas.keyMoveVel =
              Vector2Scale(Vector2Normalize(canvas.keyMoveVel), maxSpeed);
        }
        Vector2 delta = {canvas.keyMoveVel.x * dt / canvas.camera.zoom,
                         canvas.keyMoveVel.y * dt / canvas.camera.zoom};
        if (!canvas.keyMoveActive) {
          SaveBackup(canvas);
          canvas.keyMoveActive = true;
        }
        for (int idx : canvas.selectedIndices) {
          if (idx >= 0 && idx < (int)canvas.elements.size())
            MoveElement(canvas.elements[idx], delta);
        }
      } else {
        canvas.keyMoveVel = {0.0f, 0.0f};
        canvas.keyMoveActive = false;
      }
    } else {
      canvas.keyMoveVel = {0.0f, 0.0f};
      canvas.keyMoveActive = false;
    }
    const int statusH = canvas.showStatusBar ? 32 : 0;
    const int statusY = GetScreenHeight() - statusH;
    const bool mouseOnStatusBar =
        canvas.showStatusBar && mouseScreen.y >= statusY;

    if (!canvas.isTextEditing && !canvas.commandMode &&
        IsActionPressed(cfg, "open_command_mode", shiftDown, ctrlDown, altDown)) {
      canvas.commandMode = true;
      canvas.commandBuffer.clear();
    }

    if (canvas.commandMode) {
      if (escPressed) {
        canvas.commandMode = false;
        canvas.commandBuffer.clear();
      } else {
        int c = GetCharPressed();
        while (c > 0) {
          if (c >= 32 && c < 127 &&
              !(canvas.commandBuffer.empty() && c == ':'))
            canvas.commandBuffer.push_back((char)c);
          c = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !canvas.commandBuffer.empty()) {
          canvas.commandBuffer.pop_back();
        }
        if (IsKeyPressed(KEY_ENTER)) {
          ExecuteCommand(canvas, cfg, ":" + canvas.commandBuffer);
          canvas.commandMode = false;
          canvas.commandBuffer.clear();
        }
      }
    }

    if (canvas.shouldQuit)
      break;

    if (!canvas.commandMode) {
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
      float oldZoom = canvas.camera.zoom;
      float nextZoom = oldZoom + wheel * cfg.zoomStep * oldZoom;
      if (nextZoom < cfg.minZoom)
        nextZoom = cfg.minZoom;
      if (nextZoom > cfg.maxZoom)
        nextZoom = cfg.maxZoom;
      if (nextZoom != oldZoom) {
        Vector2 before = GetScreenToWorld2D(mouseScreen, canvas.camera);
        canvas.camera.zoom = nextZoom;
        Vector2 after = GetScreenToWorld2D(mouseScreen, canvas.camera);
        canvas.camera.target =
            Vector2Subtract(canvas.camera.target, Vector2Subtract(before, after));
      }
    }

    if (IsActionPressed(cfg, "zoom_in", shiftDown, ctrlDown, altDown)) {
      float oldZoom = canvas.camera.zoom;
      float nextZoom = oldZoom * cfg.zoomKeyScale;
      if (nextZoom > cfg.maxZoom)
        nextZoom = cfg.maxZoom;
      if (nextZoom != oldZoom) {
        Vector2 before = GetScreenToWorld2D(mouseScreen, canvas.camera);
        canvas.camera.zoom = nextZoom;
        Vector2 after = GetScreenToWorld2D(mouseScreen, canvas.camera);
        canvas.camera.target =
            Vector2Subtract(canvas.camera.target, Vector2Subtract(before, after));
      }
    }
    if (IsActionPressed(cfg, "zoom_out", shiftDown, ctrlDown, altDown)) {
      float oldZoom = canvas.camera.zoom;
      float nextZoom = oldZoom / cfg.zoomKeyScale;
      if (nextZoom < cfg.minZoom)
        nextZoom = cfg.minZoom;
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
      canvas.editingTextSize = canvas.textSize;
      canvas.textEditBackedUp = false;
    }
    if (escPressed && !canvas.isTextEditing) {
      canvas.isTypingNumber = false;
      canvas.inputNumber = 0;
      canvas.lastKey = 0;
    }
    NormalizeCanvasIDs(canvas);
    if (canvas.isTextEditing)
      key = 0;

    if (!canvas.isTextEditing && IsKeyPressed(KEY_TAB)) {
      canvas.showStatusBar = !canvas.showStatusBar;
    }
    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "stroke_inc", shiftDown, ctrlDown, altDown)) {
      canvas.strokeWidth = min(cfg.maxStrokeWidth, canvas.strokeWidth + 1.0f);
    }
    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "stroke_dec", shiftDown, ctrlDown, altDown)) {
      canvas.strokeWidth = max(cfg.minStrokeWidth, canvas.strokeWidth - 1.0f);
    }

    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "copy", shiftDown, ctrlDown, altDown)) {
      if (!canvas.selectedIndices.empty()) {
        canvas.clipboard.clear();
        for (int idx : canvas.selectedIndices) {
          if (idx >= 0 && idx < (int)canvas.elements.size())
            canvas.clipboard.push_back(canvas.elements[idx]);
        }
        canvas.pasteOffsetIndex = 0;
      }
    }

    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "paste", shiftDown, ctrlDown, altDown)) {
      if (!canvas.clipboard.empty()) {
        SaveBackup(canvas);
        RestoreZOrder(canvas);
        canvas.selectedIndices.clear();

        float step = cfg.pasteOffsetStep * (canvas.pasteOffsetIndex + 1);
        Vector2 pasteOffset = {step, step};
        for (const auto &item : canvas.clipboard) {
          Element cloned = item;

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
    } else if (!canvas.isTextEditing &&
               IsActionPressed(cfg, "mode_pen", shiftDown, ctrlDown, altDown)) {
      SetMode(canvas, cfg, PEN_MODE);
    }

    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "mode_selection", shiftDown, ctrlDown, altDown)) {
      SetMode(canvas, cfg, SELECTION_MODE);
    }
    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "mode_move", shiftDown, ctrlDown, altDown)) {
      SetMode(canvas, cfg, MOVE_MODE);
    }
    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "mode_resize_rotate", shiftDown, ctrlDown, altDown)) {
      SetMode(canvas, cfg, RESIZE_ROTATE_MODE);
    }
    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "mode_line_base", shiftDown, ctrlDown, altDown)) {
      int dottedPrefixKey = PrimaryKeyForAction(cfg, "prefix_dotted");
      int arrowPrefixKey = PrimaryKeyForAction(cfg, "prefix_arrow");
      if (canvas.lastKey == dottedPrefixKey) {
        SetMode(canvas, cfg, DOTTEDLINE_MODE);
      } else if (canvas.lastKey == arrowPrefixKey) {
        SetMode(canvas, cfg, ARROWLINE_MODE);
      } else {
        SetMode(canvas, cfg, LINE_MODE);
      }
    }
    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "mode_circle_base", shiftDown, ctrlDown, altDown)) {
      int dottedPrefixKey = PrimaryKeyForAction(cfg, "prefix_dotted");
      if (canvas.lastKey == dottedPrefixKey) {
        SetMode(canvas, cfg, DOTTEDCIRCLE_MODE);
      } else {
        SetMode(canvas, cfg, CIRCLE_MODE);
      }
    }
    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "mode_rect_base", shiftDown, ctrlDown, altDown)) {
      int dottedPrefixKey = PrimaryKeyForAction(cfg, "prefix_dotted");
      if (canvas.lastKey == dottedPrefixKey) {
        SetMode(canvas, cfg, DOTTEDRECT_MODE);
      } else {
        SetMode(canvas, cfg, RECTANGLE_MODE);
      }
    }
    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "mode_triangle", shiftDown, ctrlDown, altDown)) {
      int dottedPrefixKey = PrimaryKeyForAction(cfg, "prefix_dotted");
      if (canvas.lastKey == dottedPrefixKey) {
        SetMode(canvas, cfg, DOTTEDTRIANGLE_MODE);
      } else {
        SetMode(canvas, cfg, TRIANGLE_MODE);
      }
    }
    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "mode_eraser", shiftDown, ctrlDown, altDown)) {
      SetMode(canvas, cfg, ERASER_MODE);
    }
    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "mode_text", shiftDown, ctrlDown, altDown)) {
      SetMode(canvas, cfg, TEXT_MODE);
      if (!canvas.isTextEditing) {
        canvas.textBuffer.clear();
        canvas.editingIndex = -1;
      }
    }

    bool graphTogglePressed =
        !canvas.isTextEditing &&
        IsActionPressed(cfg, "graph_toggle", shiftDown, ctrlDown, altDown);
    bool groupTogglePressed =
        IsActionPressed(cfg, "group_toggle", shiftDown, ctrlDown, altDown) ||
        (shiftDown &&
         IsActionPressed(cfg, "group_toggle", false, ctrlDown, altDown));
    bool groupHandled = false;
    if (!canvas.isTextEditing && groupTogglePressed) {
      if (shiftDown) {
        if (!canvas.selectedIndices.empty()) {
          SaveBackup(canvas);
          vector<int> sorted = canvas.selectedIndices;
          sort(sorted.begin(), sorted.end(), greater<int>());
          for (int idx : sorted) {
            if (idx >= 0 && idx < (int)canvas.elements.size() &&
                canvas.elements[idx].type == GROUP_MODE) {
              Element g = canvas.elements[idx];
              canvas.elements.erase(canvas.elements.begin() + idx);
              for (auto &child : g.children) {
                canvas.elements.push_back(child);
              }
              groupHandled = true;
            }
          }
          canvas.selectedIndices.clear();
        }
      } else if (canvas.selectedIndices.size() > 1) {
        SaveBackup(canvas);
        Element group;
        group.type = GROUP_MODE;
        group.strokeWidth = canvas.strokeWidth;
        group.color = canvas.drawColor;
        group.uniqueID =
            canvas.nextElementId++;

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
        for (auto &c : group.children)
          EnsureUniqueIDRecursive(c, canvas);

        canvas.elements.push_back(group);
        canvas.selectedIndices = {(int)canvas.elements.size() - 1};
        groupHandled = true;
      }
    }
    if (!canvas.isTextEditing && graphTogglePressed && !groupHandled) {
      if (canvas.bgType == BG_GRAPH) {
        canvas.bgType = BG_BLANK;
        SetStatus(canvas, cfg, "Graph mode off");
      } else {
        canvas.bgType = BG_GRAPH;
        canvas.camera.target = {0.0f, 0.0f};
        SetStatus(canvas, cfg, "Graph mode on");
      }
    }

    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "toggle_tags", shiftDown, ctrlDown, altDown))
      canvas.showTags = !canvas.showTags;
    if (key != 0)
      canvas.lastKey = key;

    if (!canvas.isTextEditing) {
      bool redoPressed =
          IsActionPressed(cfg, "redo", shiftDown, ctrlDown, altDown);
      bool undoPressed =
          !redoPressed &&
          IsActionPressed(cfg, "undo", shiftDown, ctrlDown, altDown);

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
    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "delete_selection", shiftDown, ctrlDown, altDown)) {
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
    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "select_all", shiftDown, ctrlDown, altDown)) {
      RestoreZOrder(canvas);
      canvas.selectedIndices.clear();
      for (int i = 0; i < (int)canvas.elements.size(); ++i)
        canvas.selectedIndices.push_back(i);
    }
    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "z_backward", shiftDown, ctrlDown, altDown)) {
      MoveSelectionZOrder(canvas, false);
    }
    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "z_forward", shiftDown, ctrlDown, altDown)) {
      MoveSelectionZOrder(canvas, true);
    }

    if (!canvas.isTextEditing && escPressed && !canvas.selectedIndices.empty()) {
      RestoreZOrder(canvas);
      canvas.selectedIndices.clear();
    }

    if (canvas.mode == MOVE_MODE) {
      if (mouseLeftDown && !mouseOnStatusBar) {
        Vector2 delta = mouseDelta;
        canvas.camera.target.x -= delta.x / canvas.camera.zoom;
        canvas.camera.target.y -= delta.y / canvas.camera.zoom;
      }
    } else if (canvas.mode == SELECTION_MODE) {
      float hitTol = cfg.defaultHitTolerance / canvas.camera.zoom;
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

      bool jPressed =
          IsActionPressed(cfg, "select_next_tag", shiftDown, ctrlDown, altDown);
      bool kPressed =
          IsActionPressed(cfg, "select_prev_tag", shiftDown, ctrlDown, altDown);
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
        if (targetIdx != -1) {
          SaveBackup(canvas);
          RestoreZOrder(canvas);
          targetIdx = FindElementIndexByID(canvas, targetID);
          if (targetIdx != -1) {
            Element selected = canvas.elements[targetIdx];
            selected.originalIndex = targetIdx;
            canvas.elements.erase(canvas.elements.begin() + targetIdx);
            canvas.elements.push_back(selected);
            canvas.selectedIndices = {(int)canvas.elements.size() - 1};
          }
        }
        canvas.isTypingNumber = false;
      }

      if (mouseLeftPressed && !mouseOnStatusBar) {
        canvas.startPoint = mouseWorld;
        canvas.currentMouse = mouseWorld;
        canvas.isDragging = true;
        bool hit = false;
        int hitIndex = -1;
        bool hitSelectedBounds =
            !canvas.selectedIndices.empty() &&
            IsPointOnSelectedBounds(canvas, canvas.startPoint);
        if (hitSelectedBounds) {
          for (int i = (int)canvas.selectedIndices.size() - 1; i >= 0; --i) {
            int idx = canvas.selectedIndices[i];
            if (idx >= 0 && idx < (int)canvas.elements.size()) {
              if (IsPointInSelectionVisual(canvas.elements[idx],
                                           canvas.startPoint)) {
                hitIndex = idx;
                hit = true;
                break;
              }
            }
          }
        } else {
          for (int i = (int)canvas.elements.size() - 1; i >= 0; i--) {
            Rectangle tagHit = {canvas.elements[i].start.x,
                                canvas.elements[i].start.y - 20, 20, 20};
            if (IsPointOnElement(canvas.elements[i], canvas.startPoint, hitTol) ||
                CheckCollisionPointRec(canvas.startPoint, tagHit)) {
              hitIndex = i;
              hit = true;
              break;
            }
          }
        }
        if (hit) {
          bool alreadySelected = false;
          for (int idx : canvas.selectedIndices)
            if (idx == hitIndex)
              alreadySelected = true;
          if (hitSelectedBounds) {
            SaveBackup(canvas);
          } else {
            RestoreZOrder(canvas);
            SaveBackup(canvas);
            Element selected = canvas.elements[hitIndex];
            selected.originalIndex = hitIndex;
            canvas.elements.erase(canvas.elements.begin() + hitIndex);
            canvas.elements.push_back(selected);
            canvas.selectedIndices = {(int)canvas.elements.size() - 1};
          }
          canvas.isBoxSelecting = false;
          canvas.boxSelectActive = false;
        } else {
          RestoreZOrder(canvas);
          canvas.selectedIndices.clear();
          canvas.isBoxSelecting = true;
          canvas.boxSelectActive = false;
        }
        canvas.hasMoved = false;
      }

      if (mouseLeftDown && canvas.isDragging) {
        Vector2 prevMouse = canvas.currentMouse;
        canvas.currentMouse = mouseWorld;
        Vector2 delta = Vector2Subtract(canvas.currentMouse, prevMouse);
        if (canvas.isBoxSelecting) {
          float activationDist = cfg.selectionBoxActivationPx / canvas.camera.zoom;
          if (!canvas.boxSelectActive &&
              (Vector2Distance(canvas.startPoint, canvas.currentMouse) >=
                   activationDist ||
               Vector2Length(mouseDelta) > 0.0f)) {
            canvas.boxSelectActive = true;
          }
          if (canvas.boxSelectActive) {
            canvas.selectedIndices.clear();
            Rectangle selectionBox = {
                min(canvas.startPoint.x, canvas.currentMouse.x),
                min(canvas.startPoint.y, canvas.currentMouse.y),
                abs(canvas.currentMouse.x - canvas.startPoint.x),
                abs(canvas.currentMouse.y - canvas.startPoint.y)};
            for (int i = 0; i < (int)canvas.elements.size(); i++) {
              if (ElementIntersectsRect(canvas.elements[i], selectionBox,
                                        hitTol)) {
                canvas.selectedIndices.push_back(i);
                if (canvas.elements[i].originalIndex == -1)
                  canvas.elements[i].originalIndex = i;
              }
            }
          }
        } else {
          Vector2 dragDelta = {mouseDelta.x / canvas.camera.zoom,
                               mouseDelta.y / canvas.camera.zoom};
          if (!canvas.selectedIndices.empty() &&
              (dragDelta.x != 0 || dragDelta.y != 0)) {
            canvas.hasMoved = true;
            for (int idx : canvas.selectedIndices) {
              if (idx >= 0 && idx < (int)canvas.elements.size())
                MoveElement(canvas.elements[idx], dragDelta);
            }
          }
        }
      }
      if (mouseLeftReleased) {
        if (!canvas.isBoxSelecting && !canvas.hasMoved &&
            !canvas.undoStack.empty())
          canvas.undoStack.pop_back();
        canvas.isDragging = false;
        canvas.isBoxSelecting = false;
        canvas.boxSelectActive = false;
      }
    } else if (canvas.mode == RESIZE_ROTATE_MODE) {
      float hitTol = cfg.defaultHitTolerance / canvas.camera.zoom;
      float handleRadius = 7.0f / canvas.camera.zoom;
      float rotateOffset = 26.0f / canvas.camera.zoom;

      auto pickTopElement = [&]() -> int {
        for (int i = (int)canvas.elements.size() - 1; i >= 0; --i) {
          if (IsPointOnElement(canvas.elements[i], mouseWorld, hitTol))
            return i;
        }
        return -1;
      };

      if (mouseLeftPressed && !mouseOnStatusBar) {
        canvas.transformActive = false;
        canvas.transformHandle = 0;
        canvas.transformIndex = -1;

        int activeIdx = -1;
        if (!canvas.selectedIndices.empty())
          activeIdx = canvas.selectedIndices.back();

        if (activeIdx >= 0 && activeIdx < (int)canvas.elements.size()) {
          Element &el = canvas.elements[activeIdx];
          Vector2 center = ElementCenterLocal(el);

          bool handleHit = false;
          int handle = 0;

          if (el.type == LINE_MODE || el.type == DOTTEDLINE_MODE ||
              el.type == ARROWLINE_MODE) {
            Vector2 s = el.start;
            Vector2 e = el.end;
            if (el.rotation != 0.0f) {
              s = RotatePoint(s, center, el.rotation);
              e = RotatePoint(e, center, el.rotation);
            }
            if (Vector2Distance(mouseWorld, s) <= handleRadius) {
              handleHit = true;
              handle = 7;
            } else if (Vector2Distance(mouseWorld, e) <= handleRadius) {
              handleHit = true;
              handle = 8;
            } else {
              Vector2 mid = {(s.x + e.x) * 0.5f, (s.y + e.y) * 0.5f};
              Vector2 dir = Vector2Subtract(e, s);
              if (Vector2Length(dir) > 0.001f) {
                dir = Vector2Normalize(dir);
                Vector2 normal = {-dir.y, dir.x};
                Vector2 rotHandle =
                    Vector2Add(mid, Vector2Scale(normal, rotateOffset));
                if (Vector2Distance(mouseWorld, rotHandle) <= handleRadius) {
                  handleHit = true;
                  handle = 2;
                }
              }
            }
          } else {
            Rectangle b = el.GetLocalBounds();
            Vector2 tl = {b.x, b.y};
            Vector2 tr = {b.x + b.width, b.y};
            Vector2 br = {b.x + b.width, b.y + b.height};
            Vector2 bl = {b.x, b.y + b.height};
            Vector2 rtc = {b.x + b.width * 0.5f, b.y - rotateOffset};

            if (el.rotation != 0.0f) {
              tl = RotatePoint(tl, center, el.rotation);
              tr = RotatePoint(tr, center, el.rotation);
              br = RotatePoint(br, center, el.rotation);
              bl = RotatePoint(bl, center, el.rotation);
              rtc = RotatePoint(rtc, center, el.rotation);
            }

            if (Vector2Distance(mouseWorld, rtc) <= handleRadius) {
              handleHit = true;
              handle = 2;
            } else if (Vector2Distance(mouseWorld, tl) <= handleRadius) {
              handleHit = true;
              handle = 3;
            } else if (Vector2Distance(mouseWorld, tr) <= handleRadius) {
              handleHit = true;
              handle = 4;
            } else if (Vector2Distance(mouseWorld, br) <= handleRadius) {
              handleHit = true;
              handle = 5;
            } else if (Vector2Distance(mouseWorld, bl) <= handleRadius) {
              handleHit = true;
              handle = 6;
            }
          }

          if (handleHit) {
            SaveBackup(canvas);
            canvas.transformActive = true;
            canvas.transformHandle = handle;
            canvas.transformIndex = activeIdx;
            canvas.transformStart = el;
            canvas.transformCenter = center;
            canvas.transformStartMouse = mouseWorld;
            canvas.transformStartAngle =
                atan2f(mouseWorld.y - center.y, mouseWorld.x - center.x);
          }
        }

        if (!canvas.transformActive && activeIdx >= 0 &&
            activeIdx < (int)canvas.elements.size()) {
          Element &el = canvas.elements[activeIdx];
          if (IsPointInSelectionVisual(el, mouseWorld)) {
            SaveBackup(canvas);
            canvas.transformActive = true;
            canvas.transformHandle = 1;
            canvas.transformIndex = activeIdx;
            canvas.transformStart = el;
            canvas.transformCenter = ElementCenterLocal(el);
            canvas.transformStartMouse = mouseWorld;
            canvas.transformStartAngle = atan2f(
                mouseWorld.y - canvas.transformCenter.y,
                mouseWorld.x - canvas.transformCenter.x);
          }
        }

        if (!canvas.transformActive) {
          int hitIndex = pickTopElement();
          if (hitIndex != -1) {
            RestoreZOrder(canvas);
            SaveBackup(canvas);
            Element selected = canvas.elements[hitIndex];
            selected.originalIndex = hitIndex;
            canvas.elements.erase(canvas.elements.begin() + hitIndex);
            canvas.elements.push_back(selected);
            canvas.selectedIndices = {(int)canvas.elements.size() - 1};
            canvas.transformActive = true;
            canvas.transformHandle = 1;
            canvas.transformIndex = (int)canvas.elements.size() - 1;
            canvas.transformStart = canvas.elements[canvas.transformIndex];
            canvas.transformCenter =
                ElementCenterLocal(canvas.transformStart);
            canvas.transformStartMouse = mouseWorld;
            canvas.transformStartAngle = atan2f(
                mouseWorld.y - canvas.transformCenter.y,
                mouseWorld.x - canvas.transformCenter.x);
          } else {
            RestoreZOrder(canvas);
            canvas.selectedIndices.clear();
          }
        }
      }

      if (mouseLeftDown && canvas.transformActive) {
        int idx = canvas.transformIndex;
        if (idx >= 0 && idx < (int)canvas.elements.size()) {
          Element base = canvas.transformStart;
          Vector2 center = canvas.transformCenter;
          Element &el = canvas.elements[idx];

          if (canvas.transformHandle == 1) {
            Vector2 delta = Vector2Subtract(mouseWorld, canvas.transformStartMouse);
            el = base;
            MoveElement(el, delta);
          } else if (canvas.transformHandle == 2) {
            float angle =
                atan2f(mouseWorld.y - center.y, mouseWorld.x - center.x);
            float delta = angle - canvas.transformStartAngle;
            el = base;
            el.rotation = base.rotation + delta;
          } else if (canvas.transformHandle == 7 ||
                     canvas.transformHandle == 8) {
            Vector2 localMouse = mouseWorld;
            if (base.rotation != 0.0f)
              localMouse = RotatePoint(mouseWorld, center, -base.rotation);
            el = base;
            if (canvas.transformHandle == 7)
              el.start = localMouse;
            else
              el.end = localMouse;
          } else if (canvas.transformHandle >= 3 &&
                     canvas.transformHandle <= 6) {
            Vector2 localMouse = mouseWorld;
            if (base.rotation != 0.0f)
              localMouse = RotatePoint(mouseWorld, center, -base.rotation);
            el = base;
            Rectangle b = base.GetLocalBounds();
            float minSize = 1.0f;
            float x0 = b.x;
            float y0 = b.y;
            float x1 = b.x + b.width;
            float y1 = b.y + b.height;

            if (canvas.transformHandle == 3 || canvas.transformHandle == 6) {
              x0 = min(localMouse.x, x1 - minSize);
            } else {
              x1 = max(localMouse.x, x0 + minSize);
            }

            if (canvas.transformHandle == 3 || canvas.transformHandle == 4) {
              y0 = min(localMouse.y, y1 - minSize);
            } else {
              y1 = max(localMouse.y, y0 + minSize);
            }

            el.start = {min(x0, x1), min(y0, y1)};
            el.end = {max(x0, x1), max(y0, y1)};
          }
        }
      }

      if (mouseLeftReleased) {
        canvas.transformActive = false;
        canvas.transformHandle = 0;
        canvas.transformIndex = -1;
      }
    } else if (canvas.mode == ERASER_MODE) {
      if (mouseLeftDown && !mouseOnStatusBar) {
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
      if (mouseLeftPressed && !mouseOnStatusBar) {
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
          canvas.editingTextSize =
              (canvas.elements[hitIndex].textSize > 0.0f)
                  ? canvas.elements[hitIndex].textSize
                  : canvas.textSize;
          canvas.textEditBackedUp = false;
        } else {
          SaveBackup(canvas);
          Element newEl;
          newEl.type = TEXT_MODE;
          newEl.start = m;
          newEl.end = {m.x + 10.0f, m.y + canvas.textSize};
          newEl.strokeWidth = canvas.strokeWidth;
          newEl.color = canvas.drawColor;
          newEl.originalIndex = -1;
          newEl.uniqueID = canvas.nextElementId++;
          newEl.text = "";
          newEl.textSize = canvas.textSize;
          canvas.elements.push_back(newEl);

          canvas.textPos = m;
          canvas.isTextEditing = true;
          canvas.editingIndex = (int)canvas.elements.size() - 1;
          canvas.editingOriginalText.clear();
          canvas.textBuffer.clear();
          canvas.editingColor = canvas.drawColor;
          canvas.editingTextSize = canvas.textSize;
          canvas.textEditBackedUp = true;
        }
      }

      if (canvas.isTextEditing) {
        bool changed = false;
        int ch = GetCharPressed();
        while (ch > 0) {
          if (ch >= 32 && ch < 127) {
            bool suppressClickComma =
                canvas.antiMouseMode && ch == ',' && mouseLeftDown;
            if (!suppressClickComma) {
              canvas.textBuffer.push_back((char)ch);
              changed = true;
            }
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
                              canvas.editingTextSize, 2);
            Element &el = canvas.elements[canvas.editingIndex];
            el.text = canvas.textBuffer;
            el.textSize = canvas.editingTextSize;
            el.start = canvas.textPos;
            el.end = {canvas.textPos.x + max(10.0f, size.x),
                      canvas.textPos.y + max(canvas.editingTextSize, size.y)};
          }
        }
      }
    } else {
      if (mouseLeftPressed && !mouseOnStatusBar) {
        canvas.startPoint = mouseWorld;
        canvas.currentMouse = canvas.startPoint;
        canvas.isDragging = true;
        if (canvas.mode == PEN_MODE) {
          canvas.currentPath.clear();
          canvas.currentPath.push_back(canvas.startPoint);
        }
      }
      if (mouseLeftDown && canvas.isDragging) {
        canvas.currentMouse = mouseWorld;
        if (canvas.mode == TRIANGLE_MODE || canvas.mode == DOTTEDTRIANGLE_MODE)
          canvas.currentMouse =
              ConstrainTriangleEnd(cfg, canvas.startPoint, canvas.currentMouse);
        if (canvas.mode == PEN_MODE &&
            Vector2Distance(canvas.currentPath.back(), canvas.currentMouse) >
                cfg.penSampleDistance)
          canvas.currentPath.push_back(canvas.currentMouse);
      }
      if (mouseLeftReleased && canvas.isDragging) {
        canvas.isDragging = false;
        if (canvas.mode == PEN_MODE ||
            Vector2Distance(canvas.startPoint, canvas.currentMouse) > 1.0f) {
          SaveBackup(canvas);
          Element newEl;
          newEl.type = canvas.mode;
          newEl.start = canvas.startPoint;
          Vector2 endPoint = canvas.currentMouse;
          if (canvas.mode == TRIANGLE_MODE || canvas.mode == DOTTEDTRIANGLE_MODE)
            endPoint = ConstrainTriangleEnd(cfg, canvas.startPoint, endPoint);
          newEl.end = endPoint;
          newEl.strokeWidth = canvas.strokeWidth;
          newEl.color = canvas.drawColor;
          newEl.originalIndex = -1;

          newEl.uniqueID = canvas.nextElementId++;

          if (canvas.mode == PEN_MODE)
            newEl.path = canvas.currentPath;
          canvas.elements.push_back(newEl);
        }
      }
    }

    }

    BeginDrawing();
    ClearBackground(canvas.backgroundColor);
    BeginMode2D(canvas.camera);
    DrawBackgroundPattern(canvas);

    for (size_t i = 0; i < canvas.elements.size(); i++) {
      if (canvas.mode == TEXT_MODE && canvas.isTextEditing &&
          (int)i == canvas.editingIndex)
        continue;
      DrawElement(canvas.elements[i], canvas.font, canvas.textSize);
      bool isSelected = false;
      for (int idx : canvas.selectedIndices)
        if (idx == (int)i)
          isSelected = true;
      if ((canvas.mode == SELECTION_MODE || canvas.mode == RESIZE_ROTATE_MODE) &&
          isSelected) {
        const Element &el = canvas.elements[i];
        Color selColor = {70, 140, 160, 255};
        if (el.type == LINE_MODE || el.type == DOTTEDLINE_MODE ||
            el.type == ARROWLINE_MODE) {
          float pad = 6.0f;
          Vector2 s = el.start;
          Vector2 e = el.end;
          if (el.rotation != 0.0f) {
            Vector2 center = ElementCenterLocal(el);
            s = RotatePoint(s, center, el.rotation);
            e = RotatePoint(e, center, el.rotation);
          }
          float length = Vector2Distance(s, e);
          if (length < 0.01f) {
            Rectangle b = el.GetBounds();
            DrawRectangleLinesEx({b.x - 5, b.y - 5, b.width + 10, b.height + 10},
                                 2, selColor);
          } else {
            float angle = atan2f(e.y - s.y, e.x - s.x) * RAD2DEG;
            float width = length + pad * 2.0f;
            float height = el.strokeWidth + pad * 2.0f;
            Vector2 center = {(s.x + e.x) * 0.5f, (s.y + e.y) * 0.5f};
            Rectangle rect = {center.x, center.y, width, height};
            Vector2 origin = {width * 0.5f, height * 0.5f};
            DrawRectanglePro(rect, origin, angle, Fade(selColor, 0.18f));
            float rad = angle * DEG2RAD;
            Vector2 hx = {cosf(rad) * (width * 0.5f), sinf(rad) * (width * 0.5f)};
            Vector2 hy = {-sinf(rad) * (height * 0.5f),
                          cosf(rad) * (height * 0.5f)};
            Vector2 c1 = Vector2Subtract(Vector2Subtract(center, hx), hy);
            Vector2 c2 = Vector2Add(Vector2Subtract(center, hx), hy);
            Vector2 c3 = Vector2Add(Vector2Add(center, hx), hy);
            Vector2 c4 = Vector2Subtract(Vector2Add(center, hx), hy);
            DrawLineV(c1, c2, selColor);
            DrawLineV(c2, c3, selColor);
            DrawLineV(c3, c4, selColor);
            DrawLineV(c4, c1, selColor);
          }
        } else if (el.rotation != 0.0f) {
          Rectangle b = el.GetLocalBounds();
          Vector2 center = ElementCenterLocal(el);
          Vector2 tl = RotatePoint({b.x, b.y}, center, el.rotation);
          Vector2 tr =
              RotatePoint({b.x + b.width, b.y}, center, el.rotation);
          Vector2 br = RotatePoint({b.x + b.width, b.y + b.height}, center,
                                   el.rotation);
          Vector2 bl =
              RotatePoint({b.x, b.y + b.height}, center, el.rotation);
          DrawLineV(tl, tr, selColor);
          DrawLineV(tr, br, selColor);
          DrawLineV(br, bl, selColor);
          DrawLineV(bl, tl, selColor);
        } else {
          Rectangle b = el.GetBounds();
          DrawRectangleLinesEx({b.x - 5, b.y - 5, b.width + 10, b.height + 10},
                               2, selColor);
        }
      }
      if (canvas.showTags) {
        int displayId = canvas.elements[i].uniqueID;
        if (displayId < 0) {
          Element &mut = const_cast<Element &>(canvas.elements[i]);
          EnsureUniqueIDRecursive(mut, canvas);
          displayId = mut.uniqueID;
        }

        float tx = canvas.elements[i].start.x;
        float ty = canvas.elements[i].start.y - 22.0f;
        DrawRectangle((int)tx, (int)ty, 24, 22, YELLOW);
        DrawRectangleLines((int)tx, (int)ty, 24, 22, BLACK);
        string tag = TextFormat("%d", displayId);
        float fx = tx + 6.0f;
        float fy = ty + 4.0f;
        DrawTextEx(canvas.font, tag.c_str(), {fx, fy}, 12, 1, BLACK);
        DrawTextEx(canvas.font, tag.c_str(), {fx + 0.6f, fy}, 12, 1, BLACK);
      }
    }

    if (canvas.mode == RESIZE_ROTATE_MODE && !canvas.selectedIndices.empty()) {
      int idx = canvas.selectedIndices.back();
      if (idx >= 0 && idx < (int)canvas.elements.size()) {
        const Element &el = canvas.elements[idx];
        Color handleColor = {70, 140, 160, 255};
        float handleRadius = 6.0f / canvas.camera.zoom;
        float rotateOffset = 26.0f / canvas.camera.zoom;
        Vector2 center = ElementCenterLocal(el);

        if (el.type == LINE_MODE || el.type == DOTTEDLINE_MODE ||
            el.type == ARROWLINE_MODE) {
          Vector2 s = el.start;
          Vector2 e = el.end;
          if (el.rotation != 0.0f) {
            s = RotatePoint(s, center, el.rotation);
            e = RotatePoint(e, center, el.rotation);
          }
          DrawCircleV(s, handleRadius, handleColor);
          DrawCircleV(e, handleRadius, handleColor);
          Vector2 mid = {(s.x + e.x) * 0.5f, (s.y + e.y) * 0.5f};
          Vector2 dir = Vector2Subtract(e, s);
          if (Vector2Length(dir) > 0.001f) {
            dir = Vector2Normalize(dir);
            Vector2 normal = {-dir.y, dir.x};
            Vector2 rotHandle = Vector2Add(mid, Vector2Scale(normal, rotateOffset));
            DrawLineV(mid, rotHandle, handleColor);
            DrawCircleV(rotHandle, handleRadius, handleColor);
          }
        } else {
          Rectangle b = el.GetLocalBounds();
          Vector2 tl = {b.x, b.y};
          Vector2 tr = {b.x + b.width, b.y};
          Vector2 br = {b.x + b.width, b.y + b.height};
          Vector2 bl = {b.x, b.y + b.height};
          Vector2 tc = {b.x + b.width * 0.5f, b.y};
          Vector2 rotHandleLocal = {tc.x, tc.y - rotateOffset};

          if (el.rotation != 0.0f) {
            tl = RotatePoint(tl, center, el.rotation);
            tr = RotatePoint(tr, center, el.rotation);
            br = RotatePoint(br, center, el.rotation);
            bl = RotatePoint(bl, center, el.rotation);
            tc = RotatePoint(tc, center, el.rotation);
            rotHandleLocal = RotatePoint(rotHandleLocal, center, el.rotation);
          }

          DrawCircleV(tl, handleRadius, handleColor);
          DrawCircleV(tr, handleRadius, handleColor);
          DrawCircleV(br, handleRadius, handleColor);
          DrawCircleV(bl, handleRadius, handleColor);
          DrawLineV(tc, rotHandleLocal, handleColor);
          DrawCircleV(rotHandleLocal, handleRadius, handleColor);
        }
      }
    }

    if (canvas.isDragging) {
      if (canvas.mode == SELECTION_MODE && canvas.isBoxSelecting) {
        Rectangle box = {min(canvas.startPoint.x, canvas.currentMouse.x),
                         min(canvas.startPoint.y, canvas.currentMouse.y),
                         abs(canvas.currentMouse.x - canvas.startPoint.x),
                         abs(canvas.currentMouse.y - canvas.startPoint.y)};
        Color grubYellow = {255, 221, 51, 255};
        DrawRectangleRec(box, Fade(grubYellow, 0.2f));
        DrawRectangleLinesEx(box, 1, grubYellow);
      } else if (canvas.mode != SELECTION_MODE && canvas.mode != ERASER_MODE) {
        Element preview;
        preview.type = canvas.mode;
        preview.start = canvas.startPoint;
        preview.end = mouseWorld;
        if (canvas.mode == TRIANGLE_MODE || canvas.mode == DOTTEDTRIANGLE_MODE)
          preview.end = ConstrainTriangleEnd(cfg, canvas.startPoint, preview.end);
        preview.strokeWidth = canvas.strokeWidth;
        preview.color = Fade(canvas.drawColor, 0.5f);
        if (canvas.mode == PEN_MODE)
          preview.path = canvas.currentPath;
        DrawElement(preview, canvas.font, canvas.textSize);
      }
    }
    if (canvas.mode == ERASER_MODE)
      DrawCircleLinesV(mouseWorld, 10, ORANGE);
    if (canvas.mode == TEXT_MODE && canvas.isTextEditing) {
      DrawTextEx(canvas.font, canvas.textBuffer.c_str(), canvas.textPos,
                 canvas.editingTextSize, 2, Fade(canvas.editingColor, 0.7f));
    }
    EndMode2D();

    if (canvas.showStatusBar) {
      DrawRectangle(0, statusY, GetScreenWidth(), statusH, canvas.statusBarBg);
      DrawRectangleLines(0, statusY, GetScreenWidth(), statusH,
                         canvas.statusLabelColor);
    }

    string saveDisplay =
        canvas.savePath.empty() ? DefaultSaveTargetPath(cfg) : canvas.savePath;
    float leftMaxW = GetScreenWidth() * 0.58f;

    float leftX = 10.0f;
    float leftY = (float)statusY + 7.0f;
    leftX = DrawLabelValue(canvas.font, leftX, leftY, 16, 1.5f, "MODE: ",
                           canvas.modeText, canvas.statusLabelColor,
                           canvas.statusValueColor);
    if (canvas.showStatusBar) {
      DrawTextEx(canvas.font, "  |  FILE: ", {leftX, leftY}, 16, 1.5f,
                 canvas.statusLabelColor);
      leftX += MeasureTextEx(canvas.font, "  |  FILE: ", 16, 1.5f).x;
      string filePart =
          EllipsizeTail(canvas.font, saveDisplay, 16, 1.5f,
                        max(10.0f, leftMaxW - (leftX - 10.0f)));
      DrawTextEx(canvas.font, filePart.c_str(), {leftX, leftY}, 16, 1.5f,
                 canvas.statusValueColor);
    }

    string sw = TextFormat("%.1f", canvas.strokeWidth);
    string col = ColorToHex(canvas.drawColor);
    string zm = TextFormat("%.2fx", canvas.camera.zoom);
    string sel = TextFormat("%d", (int)canvas.selectedIndices.size());
    string els = TextFormat("%d", (int)canvas.elements.size());
    vector<pair<string, string>> rightPairs = {{"SW: ", sw},
                                               {"  COL: ", col},
                                               {"  Z: ", zm},
                                               {"  SEL: ", sel},
                                               {"  ELS: ", els}};
    float rightW = 0.0f;
    for (const auto &kv : rightPairs) {
      rightW += MeasureTextEx(canvas.font, kv.first.c_str(), 16, 1.5f).x;
      rightW += MeasureTextEx(canvas.font, kv.second.c_str(), 16, 1.5f).x;
    }
    float rx = GetScreenWidth() - rightW - 12.0f;
    if (canvas.showStatusBar) {
      for (const auto &kv : rightPairs) {
        rx = DrawLabelValue(canvas.font, rx, leftY, 16, 1.5f, kv.first, kv.second,
                            canvas.statusLabelColor, canvas.statusValueColor);
      }
    }

    if (canvas.showStatusBar && !canvas.statusMessage.empty() &&
        GetTime() <= canvas.statusUntil) {
      float mw = MeasureTextEx(canvas.font, canvas.statusMessage.c_str(), 14, 1.0f).x;
      float mx = max(12.0f, (GetScreenWidth() - mw) * 0.5f);
      DrawTextEx(canvas.font, canvas.statusMessage.c_str(), {mx, (float)statusY - 20},
                 14, 1.0f, canvas.statusValueColor);
    }

    if (canvas.commandMode) {
      int h = 32;
      int y = statusY - h;
      DrawRectangle(0, y, GetScreenWidth(), h, canvas.statusBarBg);
      DrawRectangleLines(0, y, GetScreenWidth(), h, canvas.statusLabelColor);
      string line = ":" + canvas.commandBuffer + "_";
      DrawTextEx(canvas.font, line.c_str(), {10, (float)y + 6.0f}, 18, 1.5f,
                 canvas.statusValueColor);
    }

    if (canvas.antiMouseMode) {
      float size = 8.0f;
      float thick = 2.0f;
      Color cursorColor = {220, 70, 70, 220};
      DrawLineEx({mouseScreen.x - size, mouseScreen.y},
                 {mouseScreen.x + size, mouseScreen.y}, thick, cursorColor);
      DrawLineEx({mouseScreen.x, mouseScreen.y - size},
                 {mouseScreen.x, mouseScreen.y + size}, thick, cursorColor);
    }
    EndDrawing();
    canvas.lastMouseScreen = mouseScreen;
  }
  if (canvas.ownsFont)
    UnloadFont(canvas.font);
  CloseWindow();
  return 0;
}
