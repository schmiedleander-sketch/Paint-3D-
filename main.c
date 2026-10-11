#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Custom memory allocation wrappers using HeapAlloc/HeapFree (no C CRT dependency)
static void* AppAlloc(size_t size) {
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
}

static void* AppRealloc(void* ptr, size_t size) {
    if (!ptr) return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
    return HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ptr, size);
}

static void AppFree(void* ptr) {
    if (ptr) HeapFree(GetProcessHeap(), 0, ptr);
}

static size_t AppStrLen(const char *str) {
    size_t len = 0;
    while (str && str[len]) len++;
    return len;
}

static void AppStrCpy(char *dest, const char *src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Tool types
typedef enum {
    TOOL_BRUSH,
    TOOL_MARKER,
    TOOL_CALLIGRAPHY,
    TOOL_AIRBRUSH,
    TOOL_OIL_BRUSH,
    TOOL_WATERCOLOR,
    TOOL_PIXEL_PEN,
    TOOL_PENCIL,
    TOOL_ERASER,
    TOOL_FILL,
    TOOL_2D_LINE,
    TOOL_2D_RECT,
    TOOL_2D_CIRCLE,
    TOOL_2D_STAR,
    TOOL_3D_CUBE,
    TOOL_3D_SPHERE,
    TOOL_3D_CYLINDER,
    TOOL_3D_CONE,
    TOOL_3D_TORUS,
    TOOL_STICKER_SMILEY,
    TOOL_STICKER_HEART,
    TOOL_TEXT
} ToolType;

typedef enum {
    TAB_BRUSHES,
    TAB_2D_SHAPES,
    TAB_3D_SHAPES,
    TAB_STICKERS,
    TAB_TEXT,
    TAB_EFFECTS,
    TAB_CANVAS
} TabCategory;

typedef struct {
    double x, y, z;
} Vec3;

typedef struct {
    int v1, v2, v3;
    COLORREF color;
} Triangle3D;

typedef struct {
    Vec3 *vertices;
    int num_vertices;
    Triangle3D *triangles;
    int num_triangles;
    Vec3 pos;
    Vec3 rot;
    Vec3 scale;
    COLORREF color;
    char name[32];
    BOOL selected;
} Object3D;

typedef struct {
    HWND hwndMain;
    HWND hwndToolbar;
    HWND hwndSidebar;
    HWND hwndCanvas;
    HWND hwndStatus;

    TabCategory currentTab;
    ToolType currentTool;

    COLORREF primaryColor;
    int brushThickness;

    int canvasWidth;
    int canvasHeight;
    COLORREF canvasBgColor;

    HBITMAP hbmCanvas;
    HDC hdcCanvas;
    void *pixelBuffer;

    HBITMAP hbmUndoStack[10];
    int undoCount;

    Object3D *objects3D;
    int numObjects3D;
    int selectedObjectIndex;

    Vec3 lightDir;
    int activeEffect;

    BOOL isDrawing;
    POINT lastMousePos;
    POINT startMousePos;

    char textBuffer[256];
    int fontSize;

    COLORREF palette[20];
    int paletteCount;
    COLORREF customColors[16];
} AppState;

static AppState g_App;

#define ID_TAB_BRUSHES    101
#define ID_TAB_2D         102
#define ID_TAB_3D         103
#define ID_TAB_STICKERS   104
#define ID_TAB_TEXT       105
#define ID_TAB_EFFECTS    106
#define ID_TAB_CANVAS     107

#define ID_TOOL_START     200
#define ID_COLOR_START    300
#define ID_BTN_ADD_COLOR  399
#define ID_SLIDER_THICK   401
#define ID_BTN_UNDO       501
#define ID_BTN_CLEAR      502
#define ID_BTN_SAVE_2D    503
#define ID_BTN_EXPORT_3D  504
#define ID_EFFECT_START   600
#define ID_EDIT_TEXT      701

void InitCanvasBitmap(int w, int h);
void PushUndoState();
void PerformUndo();
void DrawBrushStroke(HDC hdcTarget, int x1, int y1, int x2, int y2);
void PaintFloodFill(int startX, int startY, COLORREF fillColor);
void Draw2DShape(HDC hdcTarget, int x1, int y1, int x2, int y2);
void Add3DPrimitive(ToolType tool);
void Render3DObjects(HDC hdc);
void ExportToOBJ(const char *filename);
void ExportToPNG(const char *filename);
void BuildSidebarControls();

static Vec3 Vec3_Add(Vec3 a, Vec3 b) { return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vec3 Vec3_Sub(Vec3 a, Vec3 b) { return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z}; }
static double Vec3_Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vec3 Vec3_Cross(Vec3 a, Vec3 b) {
    return (Vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}
static Vec3 Vec3_Normalize(Vec3 v) {
    double len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1e-6) return (Vec3){0, 0, 0};
    return (Vec3){v.x / len, v.y / len, v.z / len};
}

static Vec3 RotateVec3(Vec3 v, Vec3 rot) {
    double radX = rot.x * M_PI / 180.0;
    double radY = rot.y * M_PI / 180.0;
    double radZ = rot.z * M_PI / 180.0;

    double y1 = v.y * cos(radX) - v.z * sin(radX);
    double z1 = v.y * sin(radX) + v.z * cos(radX);
    double x1 = v.x;

    double x2 = x1 * cos(radY) + z1 * sin(radY);
    double z2 = -x1 * sin(radY) + z1 * cos(radY);
    double y2 = y1;

    double x3 = x2 * cos(radZ) - y2 * sin(radZ);
    double y3 = x2 * sin(radZ) + y2 * cos(radZ);
    double z3 = z2;

    return (Vec3){x3, y3, z3};
}

void InitCanvasBitmap(int w, int h) {
    if (g_App.hdcCanvas) DeleteDC(g_App.hdcCanvas);
    if (g_App.hbmCanvas) DeleteObject(g_App.hbmCanvas);

    HDC hdcScreen = GetDC(NULL);
    g_App.hdcCanvas = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    g_App.hbmCanvas = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &g_App.pixelBuffer, NULL, 0);
    SelectObject(g_App.hdcCanvas, g_App.hbmCanvas);
    ReleaseDC(NULL, hdcScreen);

    HBRUSH hbr = CreateSolidBrush(g_App.canvasBgColor);
    RECT rect = {0, 0, w, h};
    FillRect(g_App.hdcCanvas, &rect, hbr);
    DeleteObject(hbr);

    g_App.canvasWidth = w;
    g_App.canvasHeight = h;
}

void PushUndoState() {
    if (g_App.undoCount >= 10) {
        DeleteObject(g_App.hbmUndoStack[0]);
        for (int i = 0; i < 9; i++) {
            g_App.hbmUndoStack[i] = g_App.hbmUndoStack[i + 1];
        }
        g_App.undoCount = 9;
    }
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmCopy = CreateCompatibleBitmap(hdcScreen, g_App.canvasWidth, g_App.canvasHeight);
    SelectObject(hdcMem, hbmCopy);
    BitBlt(hdcMem, 0, 0, g_App.canvasWidth, g_App.canvasHeight, g_App.hdcCanvas, 0, 0, SRCCOPY);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    g_App.hbmUndoStack[g_App.undoCount++] = hbmCopy;
}

void PerformUndo() {
    if (g_App.undoCount > 0) {
        g_App.undoCount--;
        HBITMAP hbmLast = g_App.hbmUndoStack[g_App.undoCount];
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        SelectObject(hdcMem, hbmLast);
        BitBlt(g_App.hdcCanvas, 0, 0, g_App.canvasWidth, g_App.canvasHeight, hdcMem, 0, 0, SRCCOPY);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        DeleteObject(hbmLast);
        InvalidateRect(g_App.hwndCanvas, NULL, FALSE);
    }
}

void DrawBrushStroke(HDC hdcTarget, int x1, int y1, int x2, int y2) {
    COLORREF col = g_App.primaryColor;
    if (g_App.currentTool == TOOL_ERASER) {
        col = g_App.canvasBgColor;
    }

    int radius = g_App.brushThickness / 2;
    if (radius < 1) radius = 1;

    HPEN hPen = CreatePen(PS_SOLID, g_App.brushThickness, col);
    HBRUSH hBrush = CreateSolidBrush(col);
    HPEN hOldPen = (HPEN)SelectObject(hdcTarget, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcTarget, hBrush);

    MoveToEx(hdcTarget, x1, y1, NULL);
    LineTo(hdcTarget, x2, y2);
    Ellipse(hdcTarget, x2 - radius, y2 - radius, x2 + radius, y2 + radius);

    SelectObject(hdcTarget, hOldPen);
    SelectObject(hdcTarget, hOldBrush);
    DeleteObject(hPen);
    DeleteObject(hBrush);
}

void PaintFloodFill(int startX, int startY, COLORREF fillColor) {
    if (startX < 0 || startX >= g_App.canvasWidth || startY < 0 || startY >= g_App.canvasHeight)
        return;

    COLORREF targetColor = GetPixel(g_App.hdcCanvas, startX, startY);
    if (targetColor == fillColor) return;

    HBRUSH hBrush = CreateSolidBrush(fillColor);
    HBRUSH hOld = (HBRUSH)SelectObject(g_App.hdcCanvas, hBrush);
    ExtFloodFill(g_App.hdcCanvas, startX, startY, targetColor, FLOODFILLSURFACE);
    SelectObject(g_App.hdcCanvas, hOld);
    DeleteObject(hBrush);
}

void Draw2DShape(HDC hdcTarget, int x1, int y1, int x2, int y2) {
    HPEN hPen = CreatePen(PS_SOLID, g_App.brushThickness, g_App.primaryColor);
    HBRUSH hBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HPEN hOldPen = (HPEN)SelectObject(hdcTarget, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcTarget, hBrush);

    switch (g_App.currentTool) {
        case TOOL_2D_LINE:
            MoveToEx(hdcTarget, x1, y1, NULL);
            LineTo(hdcTarget, x2, y2);
            break;
        case TOOL_2D_RECT:
            Rectangle(hdcTarget, x1, y1, x2, y2);
            break;
        case TOOL_2D_CIRCLE:
            Ellipse(hdcTarget, x1, y1, x2, y2);
            break;
        case TOOL_2D_STAR: {
            int cx = (x1 + x2) / 2;
            int cy = (y1 + y2) / 2;
            int rx = abs(x2 - x1) / 2;
            int ry = abs(y2 - y1) / 2;
            POINT pts[10];
            for (int i = 0; i < 10; i++) {
                double a = i * M_PI / 5.0 - M_PI / 2.0;
                double r = (i % 2 == 0) ? 1.0 : 0.4;
                pts[i].x = cx + (int)(rx * r * cos(a));
                pts[i].y = cy + (int)(ry * r * sin(a));
            }
            Polygon(hdcTarget, pts, 10);
            break;
        }
        case TOOL_STICKER_SMILEY: {
            int cx = (x1 + x2) / 2;
            int cy = (y1 + y2) / 2;
            int r = (abs(x2 - x1) + abs(y2 - y1)) / 4;
            if (r < 5) r = 5;
            Ellipse(hdcTarget, cx - r, cy - r, cx + r, cy + r);
            Ellipse(hdcTarget, cx - r/2 - r/6, cy - r/3 - r/6, cx - r/2 + r/6, cy - r/3 + r/6);
            Ellipse(hdcTarget, cx + r/2 - r/6, cy - r/3 - r/6, cx + r/2 + r/6, cy - r/3 + r/6);
            Arc(hdcTarget, cx - r/2, cy - r/4, cx + r/2, cy + r/2, cx - r/2, cy + r/8, cx + r/2, cy + r/8);
            break;
        }
        case TOOL_STICKER_HEART: {
            int cx = (x1 + x2) / 2;
            int cy = (y1 + y2) / 2;
            int w = abs(x2 - x1);
            int h = abs(y2 - y1);
            POINT pts[7] = {
                {cx, cy + h/2},
                {cx - w/2, cy},
                {cx - w/2, cy - h/3},
                {cx, cy - h/6},
                {cx + w/2, cy - h/3},
                {cx + w/2, cy},
                {cx, cy + h/2}
            };
            Polyline(hdcTarget, pts, 7);
            break;
        }
        case TOOL_TEXT: {
            HFONT hFont = CreateFontA(g_App.fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            HFONT hOldFont = (HFONT)SelectObject(hdcTarget, hFont);
            SetTextColor(hdcTarget, g_App.primaryColor);
            SetBkMode(hdcTarget, TRANSPARENT);
            TextOutA(hdcTarget, x1, y1, g_App.textBuffer[0] ? g_App.textBuffer : "Paint 3D Text",
                     g_App.textBuffer[0] ? (int)AppStrLen(g_App.textBuffer) : 13);
            SelectObject(hdcTarget, hOldFont);
            DeleteObject(hFont);
            break;
        }
        default:
            break;
    }

    SelectObject(hdcTarget, hOldPen);
    SelectObject(hdcTarget, hOldBrush);
    DeleteObject(hPen);
}

void Add3DPrimitive(ToolType tool) {
    Object3D obj = {0};
    obj.pos = (Vec3){0.0, 0.0, 0.0};
    obj.rot = (Vec3){15.0, 30.0, 0.0};
    obj.scale = (Vec3){100.0, 100.0, 100.0};
    obj.color = g_App.primaryColor;
    obj.selected = TRUE;

    if (tool == TOOL_3D_CUBE) {
        AppStrCpy(obj.name, "3D Cube");
        obj.num_vertices = 8;
        obj.vertices = (Vec3*)AppAlloc(sizeof(Vec3) * 8);
        Vec3 v[8] = {
            {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
            {-1, -1,  1}, {1, -1,  1}, {1, 1,  1}, {-1, 1,  1}
        };
        for (int i = 0; i < 8; i++) obj.vertices[i] = v[i];

        obj.num_triangles = 12;
        obj.triangles = (Triangle3D*)AppAlloc(sizeof(Triangle3D) * 12);
        Triangle3D t[12] = {
            {0, 1, 2, obj.color}, {0, 2, 3, obj.color},
            {5, 4, 7, obj.color}, {5, 7, 6, obj.color},
            {3, 2, 6, obj.color}, {3, 6, 7, obj.color},
            {4, 5, 1, obj.color}, {4, 1, 0, obj.color},
            {4, 0, 3, obj.color}, {4, 3, 7, obj.color},
            {1, 5, 6, obj.color}, {1, 6, 2, obj.color}
        };
        for (int i = 0; i < 12; i++) obj.triangles[i] = t[i];
    } else if (tool == TOOL_3D_SPHERE) {
        AppStrCpy(obj.name, "3D Sphere");
        int latBands = 12;
        int longBands = 12;
        obj.num_vertices = (latBands + 1) * (longBands + 1);
        obj.vertices = (Vec3*)AppAlloc(sizeof(Vec3) * obj.num_vertices);

        int vIdx = 0;
        for (int lat = 0; lat <= latBands; lat++) {
            double theta = lat * M_PI / latBands;
            double sinTheta = sin(theta);
            double cosTheta = cos(theta);

            for (int lon = 0; lon <= longBands; lon++) {
                double phi = lon * 2 * M_PI / longBands;
                obj.vertices[vIdx].x = cos(phi) * sinTheta;
                obj.vertices[vIdx].y = cosTheta;
                obj.vertices[vIdx].z = sin(phi) * sinTheta;
                vIdx++;
            }
        }

        obj.num_triangles = latBands * longBands * 2;
        obj.triangles = (Triangle3D*)AppAlloc(sizeof(Triangle3D) * obj.num_triangles);
        int tIdx = 0;
        for (int lat = 0; lat < latBands; lat++) {
            for (int lon = 0; lon < longBands; lon++) {
                int first = (lat * (longBands + 1)) + lon;
                int second = first + longBands + 1;
                obj.triangles[tIdx++] = (Triangle3D){first, second, first + 1, obj.color};
                obj.triangles[tIdx++] = (Triangle3D){second, second + 1, first + 1, obj.color};
            }
        }
    } else if (tool == TOOL_3D_CYLINDER) {
        AppStrCpy(obj.name, "3D Cylinder");
        int segments = 16;
        obj.num_vertices = segments * 2 + 2;
        obj.vertices = (Vec3*)AppAlloc(sizeof(Vec3) * obj.num_vertices);

        int topCenter = segments * 2;
        int bottomCenter = segments * 2 + 1;
        obj.vertices[topCenter] = (Vec3){0, 1, 0};
        obj.vertices[bottomCenter] = (Vec3){0, -1, 0};

        for (int i = 0; i < segments; i++) {
            double angle = i * 2 * M_PI / segments;
            obj.vertices[i] = (Vec3){cos(angle), 1, sin(angle)};
            obj.vertices[i + segments] = (Vec3){cos(angle), -1, sin(angle)};
        }

        obj.num_triangles = segments * 4;
        obj.triangles = (Triangle3D*)AppAlloc(sizeof(Triangle3D) * obj.num_triangles);
        int tIdx = 0;
        for (int i = 0; i < segments; i++) {
            int next = (i + 1) % segments;
            obj.triangles[tIdx++] = (Triangle3D){i, next, i + segments, obj.color};
            obj.triangles[tIdx++] = (Triangle3D){next, next + segments, i + segments, obj.color};
            obj.triangles[tIdx++] = (Triangle3D){topCenter, i, next, obj.color};
            obj.triangles[tIdx++] = (Triangle3D){bottomCenter, next + segments, i + segments, obj.color};
        }
    } else if (tool == TOOL_3D_CONE) {
        AppStrCpy(obj.name, "3D Cone");
        int segments = 16;
        obj.num_vertices = segments + 2;
        obj.vertices = (Vec3*)AppAlloc(sizeof(Vec3) * obj.num_vertices);

        int apex = segments;
        int bottomCenter = segments + 1;
        obj.vertices[apex] = (Vec3){0, 1, 0};
        obj.vertices[bottomCenter] = (Vec3){0, -1, 0};

        for (int i = 0; i < segments; i++) {
            double angle = i * 2 * M_PI / segments;
            obj.vertices[i] = (Vec3){cos(angle), -1, sin(angle)};
        }

        obj.num_triangles = segments * 2;
        obj.triangles = (Triangle3D*)AppAlloc(sizeof(Triangle3D) * obj.num_triangles);
        int tIdx = 0;
        for (int i = 0; i < segments; i++) {
            int next = (i + 1) % segments;
            obj.triangles[tIdx++] = (Triangle3D){apex, next, i, obj.color};
            obj.triangles[tIdx++] = (Triangle3D){bottomCenter, i, next, obj.color};
        }
    } else if (tool == TOOL_3D_TORUS) {
        AppStrCpy(obj.name, "3D Torus");
        int R_segments = 12;
        int r_segments = 8;
        double R = 0.8;
        double r = 0.3;

        obj.num_vertices = R_segments * r_segments;
        obj.vertices = (Vec3*)AppAlloc(sizeof(Vec3) * obj.num_vertices);

        int vIdx = 0;
        for (int i = 0; i < R_segments; i++) {
            double u = i * 2 * M_PI / R_segments;
            for (int j = 0; j < r_segments; j++) {
                double v = j * 2 * M_PI / r_segments;
                obj.vertices[vIdx].x = (R + r * cos(v)) * cos(u);
                obj.vertices[vIdx].y = r * sin(v);
                obj.vertices[vIdx].z = (R + r * cos(v)) * sin(u);
                vIdx++;
            }
        }

        obj.num_triangles = R_segments * r_segments * 2;
        obj.triangles = (Triangle3D*)AppAlloc(sizeof(Triangle3D) * obj.num_triangles);
        int tIdx = 0;
        for (int i = 0; i < R_segments; i++) {
            int next_i = (i + 1) % R_segments;
            for (int j = 0; j < r_segments; j++) {
                int next_j = (j + 1) % r_segments;
                int idx00 = i * r_segments + j;
                int idx10 = next_i * r_segments + j;
                int idx01 = i * r_segments + next_j;
                int idx11 = next_i * r_segments + next_j;

                obj.triangles[tIdx++] = (Triangle3D){idx00, idx10, idx01, obj.color};
                obj.triangles[tIdx++] = (Triangle3D){idx10, idx11, idx01, obj.color};
            }
        }
    }

    g_App.objects3D = (Object3D*)AppRealloc(g_App.objects3D, sizeof(Object3D) * (g_App.numObjects3D + 1));
    for (int i = 0; i < g_App.numObjects3D; i++) g_App.objects3D[i].selected = FALSE;

    g_App.objects3D[g_App.numObjects3D] = obj;
    g_App.selectedObjectIndex = g_App.numObjects3D;
    g_App.numObjects3D++;

    InvalidateRect(g_App.hwndCanvas, NULL, FALSE);
}

void Render3DObjects(HDC hdc) {
    if (g_App.numObjects3D == 0) return;

    int cx = g_App.canvasWidth / 2;
    int cy = g_App.canvasHeight / 2;
    double fov = 400.0;

    Vec3 light = Vec3_Normalize(g_App.lightDir);

    for (int o = 0; o < g_App.numObjects3D; o++) {
        Object3D *obj = &g_App.objects3D[o];

        Vec3 *transVerts = (Vec3*)AppAlloc(sizeof(Vec3) * obj->num_vertices);
        POINT *projVerts = (POINT*)AppAlloc(sizeof(POINT) * obj->num_vertices);

        for (int v = 0; v < obj->num_vertices; v++) {
            Vec3 vert = {
                obj->vertices[v].x * obj->scale.x,
                obj->vertices[v].y * obj->scale.y,
                obj->vertices[v].z * obj->scale.z
            };
            vert = RotateVec3(vert, obj->rot);
            vert = Vec3_Add(vert, obj->pos);

            transVerts[v] = vert;

            double z_dist = vert.z + 500.0;
            if (z_dist < 10.0) z_dist = 10.0;
            projVerts[v].x = cx + (int)(vert.x * fov / z_dist);
            projVerts[v].y = cy - (int)(vert.y * fov / z_dist);
        }

        for (int t = 0; t < obj->num_triangles; t++) {
            Triangle3D *tri = &obj->triangles[t];
            Vec3 v1 = transVerts[tri->v1];
            Vec3 v2 = transVerts[tri->v2];
            Vec3 v3 = transVerts[tri->v3];

            Vec3 edge1 = Vec3_Sub(v2, v1);
            Vec3 edge2 = Vec3_Sub(v3, v1);
            Vec3 normal = Vec3_Normalize(Vec3_Cross(edge1, edge2));

            if (normal.z < 0.0) {
                double diffuse = Vec3_Dot(normal, light);
                if (diffuse < 0.2) diffuse = 0.2;

                BYTE r = (BYTE)(GetRValue(obj->color) * diffuse);
                BYTE g = (BYTE)(GetGValue(obj->color) * diffuse);
                BYTE b = (BYTE)(GetBValue(obj->color) * diffuse);
                COLORREF shadedColor = RGB(r, g, b);

                HPEN hPen = CreatePen(PS_SOLID, 1, obj->selected ? RGB(0, 162, 232) : shadedColor);
                HBRUSH hBrush = CreateSolidBrush(shadedColor);
                HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

                POINT poly[3] = {projVerts[tri->v1], projVerts[tri->v2], projVerts[tri->v3]};
                Polygon(hdc, poly, 3);

                SelectObject(hdc, hOldPen);
                SelectObject(hdc, hOldBrush);
                DeleteObject(hPen);
                DeleteObject(hBrush);
            }
        }

        if (obj->selected) {
            HPEN hGizmoPen = CreatePen(PS_DOT, 1, RGB(0, 162, 232));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hGizmoPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

            int minX = 10000, minY = 10000, maxX = -10000, maxY = -10000;
            for (int v = 0; v < obj->num_vertices; v++) {
                if (projVerts[v].x < minX) minX = projVerts[v].x;
                if (projVerts[v].x > maxX) maxX = projVerts[v].x;
                if (projVerts[v].y < minY) minY = projVerts[v].y;
                if (projVerts[v].y > maxY) maxY = projVerts[v].y;
            }

            Rectangle(hdc, minX - 10, minY - 10, maxX + 10, maxY + 10);

            int gizmoX = (minX + maxX) / 2;
            int gizmoY = minY - 25;
            Ellipse(hdc, gizmoX - 8, gizmoY - 8, gizmoX + 8, gizmoY + 8);
            MoveToEx(hdc, gizmoX, gizmoY + 8, NULL);
            LineTo(hdc, gizmoX, minY - 10);

            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hGizmoPen);
        }

        AppFree(transVerts);
        AppFree(projVerts);
    }
}

void ExportToOBJ(const char *filename) {
    HANDLE hFile = CreateFileA(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD written;
    const char *header = "# Paint 3D Exported Wavefront OBJ\r\n";
    WriteFile(hFile, header, AppStrLen(header), &written, NULL);

    int vertOffset = 1;

    for (int o = 0; o < g_App.numObjects3D; o++) {
        Object3D *obj = &g_App.objects3D[o];

        for (int v = 0; v < obj->num_vertices; v++) {
            Vec3 vert = {
                obj->vertices[v].x * obj->scale.x,
                obj->vertices[v].y * obj->scale.y,
                obj->vertices[v].z * obj->scale.z
            };
            vert = RotateVec3(vert, obj->rot);
            vert = Vec3_Add(vert, obj->pos);

            char buf[128];
            wsprintfA(buf, "v %d %d %d\r\n", (int)vert.x, (int)vert.y, (int)vert.z);
            WriteFile(hFile, buf, AppStrLen(buf), &written, NULL);
        }

        for (int t = 0; t < obj->num_triangles; t++) {
            Triangle3D *tri = &obj->triangles[t];
            char buf[128];
            wsprintfA(buf, "f %d %d %d\r\n",
                      tri->v1 + vertOffset,
                      tri->v2 + vertOffset,
                      tri->v3 + vertOffset);
            WriteFile(hFile, buf, AppStrLen(buf), &written, NULL);
        }

        vertOffset += obj->num_vertices;
    }

    CloseHandle(hFile);
    MessageBoxA(g_App.hwndMain, "Successfully exported 3D model to Wavefront .OBJ!", "Paint 3D Export", MB_OK | MB_ICONINFORMATION);
}

void ExportToPNG(const char *filename) {
    BITMAPFILEHEADER bfh = {0};
    BITMAPINFOHEADER bih = {0};

    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = g_App.canvasWidth;
    bih.biHeight = g_App.canvasHeight;
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = g_App.canvasWidth * g_App.canvasHeight * 4;

    bfh.bfType = 0x4D42;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + bih.biSizeImage;

    HANDLE hFile = CreateFileA(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hFile, &bfh, sizeof(bfh), &written, NULL);
        WriteFile(hFile, &bih, sizeof(bih), &written, NULL);

        BYTE *pixels = (BYTE*)g_App.pixelBuffer;
        for (int y = g_App.canvasHeight - 1; y >= 0; y--) {
            WriteFile(hFile, pixels + (y * g_App.canvasWidth * 4), g_App.canvasWidth * 4, &written, NULL);
        }
        CloseHandle(hFile);
        MessageBoxA(g_App.hwndMain, "Successfully saved 2D Image Canvas!", "Paint 3D Save", MB_OK | MB_ICONINFORMATION);
    }
}

LRESULT CALLBACK CanvasWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hbmMem = CreateCompatibleBitmap(hdc, g_App.canvasWidth, g_App.canvasHeight);
            HBITMAP hOldBm = (HBITMAP)SelectObject(hdcMem, hbmMem);

            BitBlt(hdcMem, 0, 0, g_App.canvasWidth, g_App.canvasHeight, g_App.hdcCanvas, 0, 0, SRCCOPY);

            if (g_App.isDrawing && (g_App.currentTab == TAB_2D_SHAPES || g_App.currentTab == TAB_STICKERS)) {
                Draw2DShape(hdcMem, g_App.startMousePos.x, g_App.startMousePos.y, g_App.lastMousePos.x, g_App.lastMousePos.y);
            }

            Render3DObjects(hdcMem);

            BitBlt(hdc, 0, 0, g_App.canvasWidth, g_App.canvasHeight, hdcMem, 0, 0, SRCCOPY);

            SelectObject(hdcMem, hOldBm);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            SetCapture(hwnd);
            g_App.isDrawing = TRUE;
            g_App.startMousePos.x = LOWORD(lParam);
            g_App.startMousePos.y = HIWORD(lParam);
            g_App.lastMousePos = g_App.startMousePos;

            PushUndoState();

            if (g_App.currentTab == TAB_BRUSHES) {
                if (g_App.currentTool == TOOL_FILL) {
                    PaintFloodFill(g_App.startMousePos.x, g_App.startMousePos.y, g_App.primaryColor);
                    InvalidateRect(hwnd, NULL, FALSE);
                } else {
                    DrawBrushStroke(g_App.hdcCanvas, g_App.startMousePos.x, g_App.startMousePos.y, g_App.startMousePos.x, g_App.startMousePos.y);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            } else if (g_App.currentTab == TAB_TEXT) {
                Draw2DShape(g_App.hdcCanvas, g_App.startMousePos.x, g_App.startMousePos.y, 0, 0);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (g_App.isDrawing) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);

                if (g_App.currentTab == TAB_BRUSHES && g_App.currentTool != TOOL_FILL) {
                    DrawBrushStroke(g_App.hdcCanvas, g_App.lastMousePos.x, g_App.lastMousePos.y, x, y);
                    InvalidateRect(hwnd, NULL, FALSE);
                } else if (g_App.currentTab == TAB_3D_SHAPES && g_App.numObjects3D > 0 && g_App.selectedObjectIndex >= 0) {
                    int dx = x - g_App.lastMousePos.x;
                    int dy = y - g_App.lastMousePos.y;
                    Object3D *obj = &g_App.objects3D[g_App.selectedObjectIndex];
                    if (wParam & MK_CONTROL) {
                        obj->rot.z += dx;
                    } else {
                        obj->rot.y += dx;
                        obj->rot.x += dy;
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                } else if (g_App.currentTab == TAB_2D_SHAPES || g_App.currentTab == TAB_STICKERS) {
                    InvalidateRect(hwnd, NULL, FALSE);
                }

                g_App.lastMousePos.x = x;
                g_App.lastMousePos.y = y;
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (g_App.isDrawing) {
                ReleaseCapture();
                g_App.isDrawing = FALSE;
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);

                if (g_App.currentTab == TAB_2D_SHAPES || g_App.currentTab == TAB_STICKERS) {
                    Draw2DShape(g_App.hdcCanvas, g_App.startMousePos.x, g_App.startMousePos.y, x, y);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void BuildSidebarControls() {
    HWND hChild = GetWindow(g_App.hwndSidebar, GW_CHILD);
    while (hChild) {
        HWND hNext = GetWindow(hChild, GW_HWNDNEXT);
        DestroyWindow(hChild);
        hChild = hNext;
    }

    int y = 20;

    if (g_App.currentTab == TAB_BRUSHES) {
        CreateWindowA("STATIC", "BRUSH TOOLS", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, y, 220, 20, g_App.hwndSidebar, NULL, NULL, NULL);
        y += 30;

        const char *tools[] = {"Marker", "Calligraphy", "Airbrush", "Oil Brush", "Watercolor", "Pixel Pen", "Pencil", "Eraser", "Fill"};
        ToolType toolTypes[] = {TOOL_MARKER, TOOL_CALLIGRAPHY, TOOL_AIRBRUSH, TOOL_OIL_BRUSH, TOOL_WATERCOLOR, TOOL_PIXEL_PEN, TOOL_PENCIL, TOOL_ERASER, TOOL_FILL};

        for (int i = 0; i < 9; i++) {
            CreateWindowA("BUTTON", tools[i], WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          15 + (i % 2) * 110, y + (i / 2) * 35, 100, 30,
                          g_App.hwndSidebar, (HMENU)(UINT_PTR)(ID_TOOL_START + toolTypes[i]), NULL, NULL);
        }
        y += 5 * 35 + 20;

        CreateWindowA("STATIC", "Thickness:", WS_CHILD | WS_VISIBLE, 15, y, 100, 20, g_App.hwndSidebar, NULL, NULL, NULL);
        HWND hSlideThick = CreateWindowA(TRACKBAR_CLASS, "", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                         15, y + 20, 210, 30, g_App.hwndSidebar, (HMENU)ID_SLIDER_THICK, NULL, NULL);
        SendMessage(hSlideThick, TBM_SETRANGE, TRUE, MAKELONG(1, 100));
        SendMessage(hSlideThick, TBM_SETPOS, TRUE, g_App.brushThickness);
        y += 60;

    } else if (g_App.currentTab == TAB_2D_SHAPES) {
        CreateWindowA("STATIC", "2D SHAPES", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, y, 220, 20, g_App.hwndSidebar, NULL, NULL, NULL);
        y += 30;

        const char *shapes[] = {"Line", "Rectangle", "Circle", "5-Pt Star"};
        ToolType shapeTypes[] = {TOOL_2D_LINE, TOOL_2D_RECT, TOOL_2D_CIRCLE, TOOL_2D_STAR};

        for (int i = 0; i < 4; i++) {
            CreateWindowA("BUTTON", shapes[i], WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          15 + (i % 2) * 110, y + (i / 2) * 40, 100, 35,
                          g_App.hwndSidebar, (HMENU)(UINT_PTR)(ID_TOOL_START + shapeTypes[i]), NULL, NULL);
        }
        y += 2 * 40 + 20;

    } else if (g_App.currentTab == TAB_3D_SHAPES) {
        CreateWindowA("STATIC", "3D OBJECTS", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, y, 220, 20, g_App.hwndSidebar, NULL, NULL, NULL);
        y += 30;

        const char *shapes3D[] = {"3D Cube", "3D Sphere", "3D Cylinder", "3D Cone", "3D Torus"};
        ToolType types3D[] = {TOOL_3D_CUBE, TOOL_3D_SPHERE, TOOL_3D_CYLINDER, TOOL_3D_CONE, TOOL_3D_TORUS};

        for (int i = 0; i < 5; i++) {
            CreateWindowA("BUTTON", shapes3D[i], WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          15 + (i % 2) * 110, y + (i / 2) * 40, 100, 35,
                          g_App.hwndSidebar, (HMENU)(UINT_PTR)(ID_TOOL_START + types3D[i]), NULL, NULL);
        }
        y += 3 * 40 + 20;

        CreateWindowA("STATIC", "Tip: Drag mouse on canvas to rotate selected 3D object (Ctrl+Drag for Z-rotation)",
                      WS_CHILD | WS_VISIBLE | SS_LEFT, 15, y, 210, 60, g_App.hwndSidebar, NULL, NULL, NULL);
        y += 70;

    } else if (g_App.currentTab == TAB_STICKERS) {
        CreateWindowA("STATIC", "STICKERS", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, y, 220, 20, g_App.hwndSidebar, NULL, NULL, NULL);
        y += 30;

        CreateWindowA("BUTTON", "Smiley Face", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 15, y, 210, 35, g_App.hwndSidebar, (HMENU)(UINT_PTR)(ID_TOOL_START + TOOL_STICKER_SMILEY), NULL, NULL);
        y += 45;
        CreateWindowA("BUTTON", "Heart Sticker", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 15, y, 210, 35, g_App.hwndSidebar, (HMENU)(UINT_PTR)(ID_TOOL_START + TOOL_STICKER_HEART), NULL, NULL);
        y += 55;

    } else if (g_App.currentTab == TAB_TEXT) {
        CreateWindowA("STATIC", "TEXT TOOL", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, y, 220, 20, g_App.hwndSidebar, NULL, NULL, NULL);
        y += 30;

        CreateWindowA("STATIC", "Text Content:", WS_CHILD | WS_VISIBLE, 15, y, 100, 20, g_App.hwndSidebar, NULL, NULL, NULL);
        CreateWindowA("EDIT", g_App.textBuffer, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 15, y + 20, 210, 25, g_App.hwndSidebar, (HMENU)ID_EDIT_TEXT, NULL, NULL);
        y += 55;

        g_App.currentTool = TOOL_TEXT;

    } else if (g_App.currentTab == TAB_EFFECTS) {
        CreateWindowA("STATIC", "LIGHTING & EFFECTS", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, y, 220, 20, g_App.hwndSidebar, NULL, NULL, NULL);
        y += 30;

        const char *effects[] = {"Default White", "Warm Sunset", "Cool Neon", "Cyberpunk", "Dark Studio"};
        for (int i = 0; i < 5; i++) {
            CreateWindowA("BUTTON", effects[i], WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 15, y, 210, 35, g_App.hwndSidebar, (HMENU)(UINT_PTR)(ID_EFFECT_START + i), NULL, NULL);
            y += 40;
        }

    } else if (g_App.currentTab == TAB_CANVAS) {
        CreateWindowA("STATIC", "CANVAS CONTROLS", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, y, 220, 20, g_App.hwndSidebar, NULL, NULL, NULL);
        y += 30;

        CreateWindowA("BUTTON", "Clear Canvas", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 15, y, 210, 35, g_App.hwndSidebar, (HMENU)ID_BTN_CLEAR, NULL, NULL);
        y += 45;
        CreateWindowA("BUTTON", "Save 2D Image (.BMP)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 15, y, 210, 35, g_App.hwndSidebar, (HMENU)ID_BTN_SAVE_2D, NULL, NULL);
        y += 45;
        CreateWindowA("BUTTON", "Export 3D Model (.OBJ)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 15, y, 210, 35, g_App.hwndSidebar, (HMENU)ID_BTN_EXPORT_3D, NULL, NULL);
        y += 55;
    }

    y = 350;
    CreateWindowA("STATIC", "COLOR PALETTE", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, y, 220, 20, g_App.hwndSidebar, NULL, NULL, NULL);
    y += 25;

    for (int i = 0; i < g_App.paletteCount; i++) {
        CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                      15 + (i % 4) * 52, y + (i / 4) * 32, 45, 28,
                      g_App.hwndSidebar, (HMENU)(UINT_PTR)(ID_COLOR_START + i), NULL, NULL);
    }

    y += ((g_App.paletteCount + 3) / 4) * 32 + 10;
    CreateWindowA("BUTTON", "+ Add Custom Color", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  15, y, 210, 30, g_App.hwndSidebar, (HMENU)ID_BTN_ADD_COLOR, NULL, NULL);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_App.hwndToolbar = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE, 0, 0, 1000, 45, hwnd, NULL, NULL, NULL);

            CreateWindowA("BUTTON", "Brushes", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 8, 80, 30, hwnd, (HMENU)ID_TAB_BRUSHES, NULL, NULL);
            CreateWindowA("BUTTON", "2D Shapes", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 95, 8, 85, 30, hwnd, (HMENU)ID_TAB_2D, NULL, NULL);
            CreateWindowA("BUTTON", "3D Shapes", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 185, 8, 85, 30, hwnd, (HMENU)ID_TAB_3D, NULL, NULL);
            CreateWindowA("BUTTON", "Stickers", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 275, 8, 80, 30, hwnd, (HMENU)ID_TAB_STICKERS, NULL, NULL);
            CreateWindowA("BUTTON", "Text", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 360, 8, 70, 30, hwnd, (HMENU)ID_TAB_TEXT, NULL, NULL);
            CreateWindowA("BUTTON", "Effects", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 435, 8, 75, 30, hwnd, (HMENU)ID_TAB_EFFECTS, NULL, NULL);
            CreateWindowA("BUTTON", "Canvas", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 515, 8, 75, 30, hwnd, (HMENU)ID_TAB_CANVAS, NULL, NULL);

            CreateWindowA("BUTTON", "Undo", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 620, 8, 65, 30, hwnd, (HMENU)ID_BTN_UNDO, NULL, NULL);

            g_App.hwndCanvas = CreateWindowA("Paint3DCanvasClass", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                            10, 55, 750, 580, hwnd, NULL, NULL, NULL);

            g_App.hwndSidebar = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE | SS_NOTIFY,
                                             770, 55, 240, 580, hwnd, NULL, NULL, NULL);

            g_App.hwndStatus = CreateWindowA(STATUSCLASSNAME, "Paint 3D Ready", WS_CHILD | WS_VISIBLE,
                                            0, 0, 0, 0, hwnd, NULL, NULL, NULL);

            InitCanvasBitmap(750, 580);
            BuildSidebarControls();
            return 0;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT*)lParam;
            if (dis->CtlID >= ID_COLOR_START && dis->CtlID < ID_COLOR_START + g_App.paletteCount) {
                int idx = dis->CtlID - ID_COLOR_START;
                HBRUSH hbr = CreateSolidBrush(g_App.palette[idx]);
                FillRect(dis->hDC, &dis->rcItem, hbr);
                DeleteObject(hbr);

                if (g_App.primaryColor == g_App.palette[idx]) {
                    HPEN hPen = CreatePen(PS_SOLID, 3, RGB(0, 162, 232));
                    HPEN hOld = (HPEN)SelectObject(dis->hDC, hPen);
                    HBRUSH hOldBr = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
                    Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom);
                    SelectObject(dis->hDC, hOld);
                    SelectObject(dis->hDC, hOldBr);
                    DeleteObject(hPen);
                }
                return TRUE;
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);

            if (HIWORD(wParam) == EN_CHANGE && wmId == ID_EDIT_TEXT) {
                GetWindowTextA((HWND)lParam, g_App.textBuffer, sizeof(g_App.textBuffer));
                return 0;
            }

            if (wmId >= ID_TAB_BRUSHES && wmId <= ID_TAB_CANVAS) {
                g_App.currentTab = (TabCategory)(wmId - ID_TAB_BRUSHES);
                BuildSidebarControls();
                InvalidateRect(g_App.hwndSidebar, NULL, TRUE);
                return 0;
            }

            if (wmId >= ID_TOOL_START && wmId < ID_TOOL_START + 50) {
                ToolType tool = (ToolType)(wmId - ID_TOOL_START);
                g_App.currentTool = tool;

                if (tool >= TOOL_3D_CUBE && tool <= TOOL_3D_TORUS) {
                    Add3DPrimitive(tool);
                }
                return 0;
            }

            if (wmId >= ID_COLOR_START && wmId < ID_COLOR_START + g_App.paletteCount) {
                int idx = wmId - ID_COLOR_START;
                g_App.primaryColor = g_App.palette[idx];
                InvalidateRect(g_App.hwndSidebar, NULL, FALSE);
                return 0;
            }

            if (wmId == ID_BTN_ADD_COLOR) {
                CHOOSECOLORA cc = {0};
                cc.lStructSize = sizeof(cc);
                cc.hwndOwner = hwnd;
                cc.lpCustColors = g_App.customColors;
                cc.rgbResult = g_App.primaryColor;
                cc.Flags = CC_FULLOPEN | CC_RGBINIT;

                if (ChooseColorA(&cc)) {
                    g_App.primaryColor = cc.rgbResult;
                    if (g_App.paletteCount < 20) {
                        g_App.palette[g_App.paletteCount++] = cc.rgbResult;
                    } else {
                        g_App.palette[g_App.paletteCount - 1] = cc.rgbResult;
                    }
                    BuildSidebarControls();
                    InvalidateRect(g_App.hwndSidebar, NULL, TRUE);
                }
                return 0;
            }

            if (wmId >= ID_EFFECT_START && wmId < ID_EFFECT_START + 5) {
                int eff = wmId - ID_EFFECT_START;
                g_App.activeEffect = eff;
                if (eff == 0) g_App.lightDir = (Vec3){0.5, 1.0, -1.0};
                else if (eff == 1) g_App.lightDir = (Vec3){1.0, 0.3, -0.5};
                else if (eff == 2) g_App.lightDir = (Vec3){-1.0, 1.0, -1.0};
                else if (eff == 3) g_App.lightDir = (Vec3){0.0, 1.0, 0.2};
                else if (eff == 4) g_App.lightDir = (Vec3){0.2, 0.2, -1.0};
                InvalidateRect(g_App.hwndCanvas, NULL, FALSE);
                return 0;
            }

            switch (wmId) {
                case ID_BTN_UNDO:
                    PerformUndo();
                    break;
                case ID_BTN_CLEAR:
                    PushUndoState();
                    InitCanvasBitmap(g_App.canvasWidth, g_App.canvasHeight);
                    g_App.numObjects3D = 0;
                    InvalidateRect(g_App.hwndCanvas, NULL, FALSE);
                    break;
                case ID_BTN_SAVE_2D: {
                    OPENFILENAMEA ofn = {0};
                    char szFile[260] = "Paint3D_Drawing.bmp";
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = "Bitmap Image (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
                    if (GetSaveFileNameA(&ofn)) {
                        ExportToPNG(szFile);
                    }
                    break;
                }
                case ID_BTN_EXPORT_3D: {
                    OPENFILENAMEA ofn = {0};
                    char szFile[260] = "Paint3D_Model.obj";
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = "Wavefront 3D Object (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
                    if (GetSaveFileNameA(&ofn)) {
                        ExportToOBJ(szFile);
                    }
                    break;
                }
            }
            return 0;
        }

        case WM_HSCROLL: {
            HWND hSlider = (HWND)lParam;
            if (hSlider && GetDlgCtrlID(hSlider) == ID_SLIDER_THICK) {
                g_App.brushThickness = (int)SendMessage(hSlider, TBM_GETPOS, 0, 0);
            }
            return 0;
        }

        case WM_SIZE: {
            SendMessage(g_App.hwndStatus, WM_SIZE, 0, 0);
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Clean Win32 Application Entry Point (no C CRT overhead or template signatures)
void Paint3DEntry(void) {
    HINSTANCE hInstance = GetModuleHandle(NULL);

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    g_App.currentTab = TAB_BRUSHES;
    g_App.currentTool = TOOL_MARKER;
    g_App.primaryColor = RGB(0, 162, 232);
    g_App.brushThickness = 8;
    g_App.fontSize = 28;
    g_App.canvasBgColor = RGB(255, 255, 255);
    g_App.lightDir = (Vec3){0.5, 1.0, -1.0};
    g_App.numObjects3D = 0;
    g_App.objects3D = NULL;

    COLORREF defaultPalette[] = {
        RGB(0, 0, 0),        RGB(128, 128, 128),  RGB(255, 255, 255),  RGB(237, 28, 36),
        RGB(255, 127, 39),   RGB(255, 201, 14),   RGB(34, 177, 76),    RGB(0, 162, 232),
        RGB(63, 72, 204),    RGB(163, 73, 164),   RGB(255, 174, 201),  RGB(185, 122, 87)
    };
    g_App.paletteCount = 12;
    for (int i = 0; i < 12; i++) g_App.palette[i] = defaultPalette[i];

    WNDCLASSEXA wcxCanvas = {0};
    wcxCanvas.cbSize = sizeof(WNDCLASSEXA);
    wcxCanvas.style = CS_HREDRAW | CS_VREDRAW;
    wcxCanvas.lpfnWndProc = CanvasWndProc;
    wcxCanvas.hInstance = hInstance;
    wcxCanvas.hCursor = LoadCursor(NULL, IDC_CROSS);
    wcxCanvas.lpszClassName = "Paint3DCanvasClass";
    RegisterClassExA(&wcxCanvas);

    WNDCLASSEXA wcxMain = {0};
    wcxMain.cbSize = sizeof(WNDCLASSEXA);
    wcxMain.style = CS_HREDRAW | CS_VREDRAW;
    wcxMain.lpfnWndProc = MainWndProc;
    wcxMain.hInstance = hInstance;
    wcxMain.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    wcxMain.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcxMain.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcxMain.lpszClassName = "Paint3DMainClass";
    RegisterClassExA(&wcxMain);

    HWND hwndMain = CreateWindowExA(0, "Paint3DMainClass", "Paint 3D",
                                    WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                                    CW_USEDEFAULT, CW_USEDEFAULT, 1030, 680,
                                    NULL, NULL, hInstance, NULL);

    g_App.hwndMain = hwndMain;

    if (!hwndMain) ExitProcess(1);

    ShowWindow(hwndMain, SW_SHOW);
    UpdateWindow(hwndMain);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    ExitProcess((UINT)msg.wParam);
}
