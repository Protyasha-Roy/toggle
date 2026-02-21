#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <filesystem>
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

enum BackgroundType { BG_BLANK, BG_GRID, BG_DOTTED };

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
  int targetFps = 60;
  int minWindowWidth = 320;
  int minWindowHeight = 240;
  string windowTitle = "Toggle : no more toggling";
  string defaultFontPath = "IosevkaNerdFontMono-Regular.ttf";
  int fontAtlasSize = 96;
  string defaultSaveDir;
  string defaultExportDir;
  string defaultOpenDir;
  bool defaultDarkTheme = false;
  bool defaultShowTags = false;
  float defaultStrokeWidth = 2.0f;
  float minStrokeWidth = 1.0f;
  float maxStrokeWidth = 50.0f;
  float defaultTextSize = 24.0f;
  float minTextSize = 6.0f;
  float maxTextSize = 200.0f;
  float defaultGridWidth = 24.0f;
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
  Color lightBackground = WHITE;
  Color darkBackground = {24, 26, 31, 255};
  Color lightUiText = DARKGRAY;
  Color darkUiText = {220, 222, 228, 255};
  Color modeSelection = MAROON;
  Color modeMove = DARKBROWN;
  Color modeLine = BLUE;
  Color modeCircle = DARKGREEN;
  Color modeRect = RED;
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
  vector<Vector2> path;
  int originalIndex = -1;
  int uniqueID = -1;
  vector<Element> children;
  string text;
  float textSize = 24.0f;

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
  bool darkTheme = false;
  Color backgroundColor = WHITE;
  Color uiTextColor = DARKGRAY;
  Color drawColor = BLACK;
  BackgroundType bgType = BG_BLANK;
  float gridWidth = 24.0f;
  string savePath;
  string fontFamilyPath = "IosevkaNerdFontMono-Regular.ttf";
  bool ownsFont = true;
};

void RestoreZOrder(Canvas &canvas);
bool ParseHexColor(string hex, Color &outColor);
string ColorToHex(Color c);

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

bool IsPointOnElement(const Element &el, Vector2 p, float tolerance) {
  float tol = max(0.5f, tolerance);
  if (el.type == LINE_MODE || el.type == DOTTEDLINE_MODE ||
      el.type == ARROWLINE_MODE) {
    if (Vector2Distance(el.start, el.end) < 0.001f) {
      return Vector2Distance(p, el.start) <= (el.strokeWidth * 0.5f + tol);
    }
    return CheckCollisionPointLine(p, el.start, el.end,
                                   el.strokeWidth * 0.5f + tol);
  }
  if (el.type == CIRCLE_MODE || el.type == DOTTEDCIRCLE_MODE) {
    float r = Vector2Distance(el.start, el.end);
    float d = Vector2Distance(p, el.start);
    return fabsf(d - r) <= (el.strokeWidth * 0.5f + tol);
  }
  if (el.type == RECTANGLE_MODE || el.type == DOTTEDRECT_MODE) {
    float x0 = min(el.start.x, el.end.x);
    float y0 = min(el.start.y, el.end.y);
    float x1 = max(el.start.x, el.end.x);
    float y1 = max(el.start.y, el.end.y);
    Vector2 a = {x0, y0};
    Vector2 b = {x1, y0};
    Vector2 c = {x1, y1};
    Vector2 d = {x0, y1};
    float t = el.strokeWidth * 0.5f + tol;
    return CheckCollisionPointLine(p, a, b, t) ||
           CheckCollisionPointLine(p, b, c, t) ||
           CheckCollisionPointLine(p, c, d, t) ||
           CheckCollisionPointLine(p, d, a, t);
  }
  if (el.type == PEN_MODE) {
    if (el.path.empty())
      return false;
    if (el.path.size() == 1)
      return Vector2Distance(p, el.path[0]) <= (el.strokeWidth * 0.5f + tol);
    float t = el.strokeWidth * 0.5f + tol;
    for (size_t i = 1; i < el.path.size(); i++) {
      if (CheckCollisionPointLine(p, el.path[i - 1], el.path[i], t))
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
    return CheckCollisionPointRec(p, el.GetBounds());
  }
  return false;
}

bool IsPointOnSelectedBounds(const Canvas &canvas, Vector2 p) {
  for (int i = (int)canvas.selectedIndices.size() - 1; i >= 0; --i) {
    int idx = canvas.selectedIndices[i];
    if (idx >= 0 && idx < (int)canvas.elements.size()) {
      Rectangle b = canvas.elements[idx].GetBounds();
      Rectangle expanded = {b.x - 5.0f, b.y - 5.0f, b.width + 10.0f,
                            b.height + 10.0f};
      if (CheckCollisionPointRec(p, expanded))
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
    float overlap = el.strokeWidth * 0.5f;
    DrawDashedLine({r.x - overlap, r.y}, {r.x + r.width + overlap, r.y},
                   el.strokeWidth, el.color);
    DrawDashedLine({r.x + r.width, r.y - overlap},
                   {r.x + r.width, r.y + r.height + overlap}, el.strokeWidth,
                   el.color);
    DrawDashedLine({r.x + r.width + overlap, r.y + r.height},
                   {r.x - overlap, r.y + r.height}, el.strokeWidth, el.color);
    DrawDashedLine({r.x, r.y + r.height + overlap}, {r.x, r.y - overlap},
                   el.strokeWidth, el.color);
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
    float size = (el.textSize > 0.0f) ? el.textSize : textSize;
    DrawTextEx(font, el.text.c_str(), el.start, size, 2, el.color);
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

string BackgroundTypeToString(BackgroundType t) {
  if (t == BG_GRID)
    return "grid";
  if (t == BG_DOTTED)
    return "dotted";
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
  AddDefaultBinding(cfg, "mode_rect_base", "R");
  AddDefaultBinding(cfg, "prefix_dotted", "D");
  AddDefaultBinding(cfg, "prefix_arrow", "A");
  AddDefaultBinding(cfg, "mode_eraser", "E");
  AddDefaultBinding(cfg, "mode_text", "T");
  AddDefaultBinding(cfg, "group_toggle", "G");
  AddDefaultBinding(cfg, "toggle_tags", "F");
  AddDefaultBinding(cfg, "undo", "U|Ctrl+Z");
  AddDefaultBinding(cfg, "redo", "Shift+U|Ctrl+Y|Ctrl+Shift+Z");
  AddDefaultBinding(cfg, "delete_selection", "X");
  AddDefaultBinding(cfg, "z_backward", "Left_Bracket");
  AddDefaultBinding(cfg, "z_forward", "Right_Bracket");
  AddDefaultBinding(cfg, "select_next_tag", "J");
  AddDefaultBinding(cfg, "select_prev_tag", "K");
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
  out << "app.target_fps=" << cfg.targetFps << "\n";
  out << "app.status_seconds=" << cfg.statusDurationSeconds << "\n";
  out << "font.default_path=" << cfg.defaultFontPath << "\n";
  out << "font.atlas_size=" << cfg.fontAtlasSize << "\n";
  out << "path.default_save_dir=" << cfg.defaultSaveDir << "\n";
  out << "path.default_export_dir=" << cfg.defaultExportDir << "\n";
  out << "path.default_open_dir=" << cfg.defaultOpenDir << "\n";
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
  out << "canvas.draw_color=" << ColorToHex(cfg.defaultDrawColor) << "\n";
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
  out << "mode_color.selection=" << ColorToHex(cfg.modeSelection) << "\n";
  out << "mode_color.move=" << ColorToHex(cfg.modeMove) << "\n";
  out << "mode_color.line=" << ColorToHex(cfg.modeLine) << "\n";
  out << "mode_color.circle=" << ColorToHex(cfg.modeCircle) << "\n";
  out << "mode_color.rect=" << ColorToHex(cfg.modeRect) << "\n";
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
    else if (key == "canvas.draw_color" && ParseHexColor(value, cv))
      cfg.defaultDrawColor = cv;
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
  } else {
    canvas.backgroundColor = cfg.lightBackground;
    canvas.uiTextColor = cfg.lightUiText;
  }
}

void DrawBackgroundPattern(const Canvas &canvas) {
  if (canvas.bgType == BG_BLANK)
    return;

  float spacing = max(6.0f, canvas.gridWidth);
  float left = canvas.camera.target.x - canvas.camera.offset.x / canvas.camera.zoom;
  float top = canvas.camera.target.y - canvas.camera.offset.y / canvas.camera.zoom;
  float right =
      canvas.camera.target.x + (float)GetScreenWidth() / canvas.camera.zoom;
  float bottom =
      canvas.camera.target.y + (float)GetScreenHeight() / canvas.camera.zoom;

  float startX = floorf(left / spacing) * spacing;
  float startY = floorf(top / spacing) * spacing;
  Color lineColor = canvas.darkTheme ? Fade(RAYWHITE, 0.12f) : Fade(BLACK, 0.12f);

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
      << el.textSize << "\n";
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
  if (!(ls >> type >> el.uniqueID >> el.strokeWidth >> r >> g >> b >> a >>
        el.start.x >> el.start.y >> el.end.x >> el.end.y))
    return false;
  // Backward compatibility for files saved before textSize was serialized.
  if (!(ls >> el.textSize))
    el.textSize = 24.0f;
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
  Vector2 s = GetWorldToScreen2D(el.start, camera);
  Vector2 e = GetWorldToScreen2D(el.end, camera);
  float scaledStroke = max(0.5f, el.strokeWidth * camera.zoom);
  float effectiveTextSize = (el.textSize > 0.0f) ? el.textSize : textSize;
  float scaledTextSize = max(6.0f, effectiveTextSize * camera.zoom);
  if (el.type == LINE_MODE || el.type == DOTTEDLINE_MODE || el.type == ARROWLINE_MODE) {
    out << "<line x1=\"" << s.x << "\" y1=\"" << s.y << "\" x2=\""
        << e.x << "\" y2=\"" << e.y << "\" stroke=\"" << stroke
        << "\" stroke-width=\"" << scaledStroke << "\"";
    if (el.type == DOTTEDLINE_MODE)
      out << " stroke-dasharray=\"8,6\"";
    out << " fill=\"none\" />\n";
  } else if (el.type == RECTANGLE_MODE || el.type == DOTTEDRECT_MODE) {
    float x = min(s.x, e.x);
    float y = min(s.y, e.y);
    float w = fabsf(e.x - s.x);
    float h = fabsf(e.y - s.y);
    out << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w
        << "\" height=\"" << h << "\" stroke=\"" << stroke
        << "\" stroke-width=\"" << scaledStroke << "\" fill=\"none\"";
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
  } else if (el.type == PEN_MODE) {
    if (el.path.size() >= 2) {
      out << "<polyline points=\"";
      for (const auto &p : el.path) {
        Vector2 sp = GetWorldToScreen2D(p, camera);
        out << sp.x << "," << sp.y << " ";
      }
      out << "\" stroke=\"" << stroke << "\" stroke-width=\"" << scaledStroke
          << "\" fill=\"none\" stroke-linecap=\"round\" stroke-linejoin=\"round\" />\n";
    } else if (el.path.size() == 1) {
      Vector2 sp = GetWorldToScreen2D(el.path[0], camera);
      out << "<circle cx=\"" << sp.x << "\" cy=\"" << sp.y
          << "\" r=\"" << max(0.5f, el.strokeWidth * 0.5f * camera.zoom)
          << "\" fill=\""
          << stroke << "\" />\n";
    }
  } else if (el.type == TEXT_MODE) {
    out << "<text x=\"" << s.x << "\" y=\"" << (s.y + scaledTextSize)
        << "\" fill=\"" << stroke << "\" font-family=\"" << SvgEscape(fontFamily)
        << "\" font-size=\"" << scaledTextSize << "\">" << SvgEscape(el.text)
        << "</text>\n";
  } else if (el.type == GROUP_MODE) {
    for (const auto &child : el.children)
      WriteSvgElement(out, child, fontFamily, textSize, camera);
  }
}

bool ExportCanvasSvg(const Canvas &canvas, const string &filename) {
  ofstream out(filename);
  if (!out.is_open())
    return false;
  int w = GetScreenWidth();
  int h = GetScreenHeight();
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << w
      << "\" height=\"" << h << "\" viewBox=\"0 0 " << w << " " << h << "\">\n";
  out << "<rect width=\"100%\" height=\"100%\" fill=\"white\" />\n";
  for (const auto &el : canvas.elements)
    WriteSvgElement(out, el, canvas.fontFamilyPath, canvas.textSize, canvas.camera);
  out << "</svg>\n";
  return true;
}

bool ExportCanvasRaster(const Canvas &canvas, const string &filename) {
  RenderTexture2D target = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
  BeginTextureMode(target);
  ClearBackground(canvas.backgroundColor);
  BeginMode2D(canvas.camera);
  DrawBackgroundPattern(canvas);
  for (const auto &el : canvas.elements)
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
    if (args.empty() || !ParseHexColor(args[0], c)) {
      SetStatus(canvas, cfg, "Usage: :color #RRGGBB or #RRGGBBAA");
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
    } else {
      SetStatus(canvas, cfg, "Usage: :type blank|grid|dotted");
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
    for (int i = 0; i < (int)normalized.size(); i++) {
      if (IsExportType(normalized[i])) {
        typeIdx = i;
        type = NormalizeExportType(normalized[i]);
        break;
      }
    }

    vector<string> rest;
    for (int i = 0; i < (int)normalized.size(); i++) {
      if (i != typeIdx)
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
      if (typeIdx == 2 && normalized.size() >= 3) {
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

    bool ok = false;
    if (type == "svg")
      ok = ExportCanvasSvg(canvas, fullPath);
    else
      ok = ExportCanvasRaster(canvas, fullPath);

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
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(screenWidth, screenHeight, cfg.windowTitle.c_str());
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
  canvas.drawColor = cfg.defaultDrawColor;
  canvas.showTags = cfg.defaultShowTags;
  canvas.bgType = cfg.defaultBgType;
  SetTheme(canvas, cfg, cfg.defaultDarkTheme);
  SetMode(canvas, cfg, SELECTION_MODE);

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
    Vector2 mouseScreen = GetMousePosition();
    Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, canvas.camera);

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
    } else if (!canvas.isTextEditing &&
               IsActionPressed(cfg, "mode_pen", shiftDown, ctrlDown, altDown)) {
      SetMode(canvas, cfg, PEN_MODE);
    }

    // Other mode keys
    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "mode_selection", shiftDown, ctrlDown, altDown)) {
      SetMode(canvas, cfg, SELECTION_MODE);
    }
    if (!canvas.isTextEditing &&
        IsActionPressed(cfg, "mode_move", shiftDown, ctrlDown, altDown)) {
      SetMode(canvas, cfg, MOVE_MODE);
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

    bool groupTogglePressed =
        IsActionPressed(cfg, "group_toggle", shiftDown, ctrlDown, altDown) ||
        (shiftDown &&
         IsActionPressed(cfg, "group_toggle", false, ctrlDown, altDown));
    if (!canvas.isTextEditing && groupTogglePressed) {
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
        group.color = canvas.drawColor;
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

      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        canvas.startPoint = mouseWorld;
        canvas.currentMouse = mouseWorld;
        canvas.isDragging = true;
        bool hit = false;
        int hitIndex = -1;
        bool hitSelectedBounds =
            !canvas.selectedIndices.empty() &&
            IsPointOnSelectedBounds(canvas, canvas.startPoint);
        float hitTol = cfg.defaultHitTolerance / canvas.camera.zoom;
        if (hitSelectedBounds) {
          for (int i = (int)canvas.selectedIndices.size() - 1; i >= 0; --i) {
            int idx = canvas.selectedIndices[i];
            if (idx >= 0 && idx < (int)canvas.elements.size()) {
              Rectangle b = canvas.elements[idx].GetBounds();
              Rectangle expanded = {b.x - 5.0f, b.y - 5.0f, b.width + 10.0f,
                                    b.height + 10.0f};
              if (CheckCollisionPointRec(canvas.startPoint, expanded)) {
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
          canvas.boxSelectActive = false;
        } else {
          RestoreZOrder(canvas);
          canvas.selectedIndices.clear();
          canvas.isBoxSelecting = true;
          canvas.boxSelectActive = false;
        }
        canvas.hasMoved = false;
      }

      if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && canvas.isDragging) {
        Vector2 prevMouse = canvas.currentMouse;
        canvas.currentMouse = GetScreenToWorld2D(GetMousePosition(), canvas.camera);
        Vector2 delta = Vector2Subtract(canvas.currentMouse, prevMouse);
        if (canvas.isBoxSelecting) {
          float activationDist = cfg.selectionBoxActivationPx / canvas.camera.zoom;
          if (!canvas.boxSelectActive &&
              Vector2Distance(canvas.startPoint, canvas.currentMouse) >=
                  activationDist) {
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
              if (CheckCollisionRecs(selectionBox,
                                     canvas.elements[i].GetBounds())) {
                canvas.selectedIndices.push_back(i);
                if (canvas.elements[i].originalIndex == -1)
                  canvas.elements[i].originalIndex = i;
              }
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
        canvas.boxSelectActive = false;
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
                cfg.penSampleDistance)
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
          newEl.color = canvas.drawColor;
          newEl.originalIndex = -1;

          // Assign the permanent ID and increment the counter
          newEl.uniqueID = canvas.nextElementId++;

          if (canvas.mode == PEN_MODE)
            newEl.path = canvas.currentPath;
          canvas.elements.push_back(newEl);
        }
      }
    }

    } // end non-command input handling

    // --- Drawing
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
        preview.color = Fade(canvas.drawColor, 0.5f);
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
                 canvas.editingTextSize, 2, Fade(canvas.editingColor, 0.7f));
    }
    EndMode2D();
    Color modeDisplayColor =
        (canvas.darkTheme && canvas.modeColor.r < 32 && canvas.modeColor.g < 32 &&
         canvas.modeColor.b < 32)
            ? RAYWHITE
            : canvas.modeColor;
    DrawTextEx(canvas.font, "Current Mode:", {10, 10}, 24, 2, canvas.uiTextColor);
    DrawTextEx(canvas.font, canvas.modeText, {180, 10}, 24, 2,
               modeDisplayColor);
    DrawTextEx(canvas.font, TextFormat("Stroke: %.1f  Color: %s", canvas.strokeWidth,
                                       ColorToHex(canvas.drawColor).c_str()),
               {10, 42}, 18, 2, canvas.uiTextColor);

    if (!canvas.statusMessage.empty() && GetTime() <= canvas.statusUntil) {
      DrawRectangle(10, 70, 520, 28, Fade(canvas.darkTheme ? BLACK : WHITE, 0.85f));
      DrawRectangleLines(10, 70, 520, 28, Fade(canvas.uiTextColor, 0.7f));
      DrawTextEx(canvas.font, canvas.statusMessage.c_str(), {18, 75}, 16, 1.5f,
                 canvas.uiTextColor);
    }

    if (canvas.commandMode) {
      int h = 34;
      int y = GetScreenHeight() - h;
      DrawRectangle(0, y, GetScreenWidth(), h, canvas.darkTheme ? BLACK : LIGHTGRAY);
      DrawRectangleLines(0, y, GetScreenWidth(), h, Fade(canvas.uiTextColor, 0.6f));
      string line = ":" + canvas.commandBuffer + "_";
      DrawTextEx(canvas.font, line.c_str(), {10, (float)y + 8.0f}, 20, 2,
                 canvas.uiTextColor);
    }
    EndDrawing();
  }
  if (canvas.ownsFont)
    UnloadFont(canvas.font);
  CloseWindow();
  return 0;
}
