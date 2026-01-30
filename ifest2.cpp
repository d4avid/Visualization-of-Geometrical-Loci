#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>
#include <algorithm> // Required for std::swap

// --- Fragment Shader Source ---
const char* shaderSource =
"#version 330\n"
"in vec2 fragTexCoord;\n"
"out vec4 finalColor;\n"
"uniform vec2 u_points[20];\n"
"uniform int u_count;\n"
"uniform int u_k;\n"
"uniform int u_mode;\n"
"uniform vec2 u_res;\n"
"uniform vec2 u_offset;\n"
"uniform float u_zoom;\n"
"\n"
"void main() {\n"
"    vec2 screenPos = vec2(gl_FragCoord.x, u_res.y - gl_FragCoord.y);\n"
"    vec2 worldPos = (screenPos - u_offset) / u_zoom;\n"
"    \n"
"    float d[20];\n"
"    for (int i = 0; i < u_count; i++) d[i] = length(worldPos - u_points[i]);\n"
"    \n"
"    float diff = -1.0;\n"
"    if (u_mode == 0) {\n"
"        float dists[20];\n"
"        for(int i=0; i<u_count; i++) dists[i] = d[i];\n"
"        for (int i = 0; i < u_count - 1; i++) {\n"
"            for (int j = 0; j < u_count - i - 1; j++) {\n"
"                if (dists[j] > dists[j+1]) {\n"
"                    float t = dists[j]; dists[j] = dists[j+1]; dists[j+1] = t;\n"
"                }\n"
"            }\n"
"        }\n"
"        float sumSmall = 0.0;\n"
"        for(int i = 0; i < u_k - 1; i++) sumSmall += dists[u_count - u_k + i];\n"
"        diff = dists[u_count - 1] - sumSmall;\n"
"    } else {\n"
"        float sumOthers = 0.0;\n"
"        for(int i = 0; i < u_k - 1; i++) sumOthers += d[i];\n"
"        diff = d[u_count - 1] - sumOthers;\n"
"    }\n"
"\n"
"    vec3 color = vec3(0.0);\n"
"    if (diff > 0.0) {\n"
"        color = vec3(0.2, 0.4, 0.9) * (1.0 - exp(-diff * 0.02));\n"
"        float edge = smoothstep(2.0, 0.0, diff * u_zoom);\n"
"        color += vec3(0.8, 0.9, 1.0) * edge;\n"
"    }\n"
"    finalColor = vec4(color, 1.0);\n"
"}\n";

struct PointData { Vector2 pos; char name; };

std::vector<PointData> GeneratePolygon(int n, float radius) {
    std::vector<PointData> p;
    float sectorAngle = 360.0f / (float)n;
    float offsetAngle = 90.0f + (sectorAngle / 2.0f);
    for (int i = 0; i < n; i++) {
        float angle = (float)i * sectorAngle + offsetAngle;
        Vector2 pos = { cosf(angle * DEG2RAD) * radius, sinf(angle * DEG2RAD) * radius };
        p.push_back({ pos, (char)('A' + i) });
    }
    return p;
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Locus Visualizer: Interactive Mode");

    Camera2D camera = { 0 };
    camera.offset = { GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
    camera.zoom = 1.0f;

    auto ResetPoints = [&]() -> std::vector<PointData> {
        return { {{-200, -100}, 'A'}, {{200, -100}, 'B'}, {{0, 150}, 'C'} };
        };

    std::vector<PointData> points = ResetPoints();
    int draggedIndex = -1, editingIndex = -1;
    bool isPanning = false;
    int kValue = 3, nVertices = 3, modeValue = 0;
    std::string inputBuffer = "";

    Shader shader = LoadShaderFromMemory(0, shaderSource);
    int locPoints = GetShaderLocation(shader, "u_points"), locCount = GetShaderLocation(shader, "u_count"), locK = GetShaderLocation(shader, "u_k"), locMode = GetShaderLocation(shader, "u_mode"), locRes = GetShaderLocation(shader, "u_res"), locOffset = GetShaderLocation(shader, "u_offset"), locZoom = GetShaderLocation(shader, "u_zoom");

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        Vector2 mousePos = GetMousePosition();
        Vector2 worldMouse = GetScreenToWorld2D(mousePos, camera);

        Rectangle btnRestart = { (float)GetScreenWidth() - 140, 20, 120, 40 };
        Rectangle sliderK = { 20, (float)GetScreenHeight() - 40, 200, 20 };
        Rectangle sliderN = { (float)GetScreenWidth() - 220, (float)GetScreenHeight() - 40, 200, 20 };
        Rectangle btnMode = { 240, (float)GetScreenHeight() - 40, 160, 25 };

        bool overUI = CheckCollisionPointRec(mousePos, btnRestart) || CheckCollisionPointRec(mousePos, sliderK) || CheckCollisionPointRec(mousePos, sliderN) || CheckCollisionPointRec(mousePos, btnMode);

        // Interaction Logic
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, btnRestart)) points = ResetPoints();
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, btnMode)) modeValue = !modeValue;
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, sliderK)) kValue = Clamp(2 + (int)(((mousePos.x - sliderK.x) / sliderK.width) * (points.size() - 1)), 2, (int)points.size());
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, sliderN)) {
            int newN = Clamp(3 + (int)(((mousePos.x - sliderN.x) / sliderN.width) * 17), 3, 20);
            if (newN != nVertices) { nVertices = newN; points = GeneratePolygon(nVertices, 200.0f); kValue = Clamp(kValue, 2, (int)points.size()); }
        }

        if (editingIndex != -1) {
            int key = GetCharPressed();
            while (key > 0) { if ((key >= '0' && key <= '9') || key == ',' || key == '-') inputBuffer += (char)key; key = GetCharPressed(); }
            if (IsKeyPressed(KEY_BACKSPACE) && !inputBuffer.empty()) inputBuffer.pop_back();
            if (IsKeyPressed(KEY_ENTER)) {
                size_t comma = inputBuffer.find(',');
                if (comma != std::string::npos) { try { points[editingIndex].pos = { std::stof(inputBuffer.substr(0, comma)), std::stof(inputBuffer.substr(comma + 1)) }; } catch (...) {} }
                editingIndex = -1; inputBuffer = "";
            }
        }
        else if (!overUI) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                    for (int i = 0; i < (int)points.size(); i++) {
                        if (CheckCollisionPointCircle(mousePos, GetWorldToScreen2D(points[i].pos, camera), 20)) {
                            std::swap(points[i], points.back());
                            modeValue = 1; break;
                        }
                    }
                }
                else if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                    for (size_t i = 0; i < points.size(); i++) {
                        if (CheckCollisionPointCircle(mousePos, GetWorldToScreen2D(points[i].pos, camera), 20)) {
                            points.erase(points.begin() + i);
                            for (size_t j = 0; j < points.size(); j++) points[j].name = (char)('A' + j);
                            kValue = Clamp(kValue, 2, (int)points.size()); break;
                        }
                    }
                }
                else {
                    draggedIndex = -1;
                    for (int i = 0; i < (int)points.size(); i++) {
                        Vector2 s = GetWorldToScreen2D(points[i].pos, camera);
                        if (CheckCollisionPointCircle(mousePos, s, 20)) { draggedIndex = i; break; }
                        if (CheckCollisionPointRec(mousePos, { s.x + 5, s.y - 30, 150, 25 })) { editingIndex = i; inputBuffer = ""; break; }
                    }
                    if (draggedIndex == -1 && editingIndex == -1) isPanning = true;
                }
            }
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && draggedIndex != -1) points[draggedIndex].pos = worldMouse;
            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && points.size() < 20) {
                points.push_back({ worldMouse, (char)('A' + (int)points.size()) });
                kValue = Clamp(kValue, 2, (int)points.size());
            }
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) { draggedIndex = -1; isPanning = false; }
        if (isPanning) camera.target = Vector2Subtract(camera.target, Vector2Scale(GetMouseDelta(), 1.0f / camera.zoom));

        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            Vector2 m1 = GetScreenToWorld2D(mousePos, camera);
            camera.zoom = Clamp(camera.zoom + wheel * 0.1f, 0.05f, 20.0f);
            Vector2 m2 = GetScreenToWorld2D(mousePos, camera);
            camera.target = Vector2Add(camera.target, Vector2Subtract(m1, m2));
        }

        // Shader Uniforms
        Vector2 rawPoints[20];
        for (size_t i = 0; i < points.size(); i++) rawPoints[i] = points[i].pos;
        float res[2] = { (float)GetScreenWidth(), (float)GetScreenHeight() };
        float off[2] = { camera.offset.x - camera.target.x * camera.zoom, camera.offset.y - camera.target.y * camera.zoom };
        int count = (int)points.size();

        SetShaderValue(shader, locRes, res, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, locCount, &count, SHADER_UNIFORM_INT);
        SetShaderValue(shader, locK, &kValue, SHADER_UNIFORM_INT);
        SetShaderValue(shader, locMode, &modeValue, SHADER_UNIFORM_INT);
        SetShaderValueV(shader, locPoints, rawPoints, SHADER_UNIFORM_VEC2, count);
        SetShaderValue(shader, locOffset, off, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, locZoom, &camera.zoom, SHADER_UNIFORM_FLOAT);

        BeginDrawing();
        ClearBackground(BLACK);

        // 1. Draw Locus Shader
        BeginShaderMode(shader);
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), WHITE);
        EndShaderMode();

        // 2. Draw World Elements (Grid, Points)
        BeginMode2D(camera);
        // Draw Grid
        float gridSize = 50.0f;
        for (int i = -100; i <= 100; i++) {
            DrawLineV({ (float)i * gridSize, -5000 }, { (float)i * gridSize, 5000 }, Fade(GRAY, 0.2f));
            DrawLineV({ -5000, (float)i * gridSize }, { 5000, (float)i * gridSize }, Fade(GRAY, 0.2f));
        }
        // Draw Main Axes
        DrawLineEx({ -10000, 0 }, { 10000, 0 }, 2.0f / camera.zoom, Fade(RAYWHITE, 0.5f));
        DrawLineEx({ 0, -10000 }, { 0, 10000 }, 2.0f / camera.zoom, Fade(RAYWHITE, 0.5f));

        for (int i = 0; i < (int)points.size(); i++) {
            bool isMaster = (modeValue == 1 && i == (int)points.size() - 1);
            DrawCircleV(points[i].pos, 5.0f / camera.zoom, isMaster ? GOLD : WHITE);
            if (isMaster) DrawCircleLinesV(points[i].pos, 12.0f / camera.zoom, GOLD);
        }
        EndMode2D();

        // 3. Draw Labels and UI
        for (int i = 0; i < (int)points.size(); i++) {
            Vector2 s = GetWorldToScreen2D(points[i].pos, camera);
            Color labelColor = (modeValue == 1 && i == (int)points.size() - 1) ? GOLD : WHITE;
            if (editingIndex == i) DrawText(TextFormat("%c: %s_", points[i].name, inputBuffer.c_str()), (int)s.x + 12, (int)s.y - 28, 20, YELLOW);
            else DrawText(TextFormat("%c: (%.0f, %.0f)", points[i].name, points[i].pos.x, points[i].pos.y), (int)s.x + 10, (int)s.y - 25, 18, labelColor);
        }

        DrawRectangle(10, 10, 430, 100, Fade(BLACK, 0.6f));
        DrawText("Left-Drag: Move/Pan | Right-Click: Add | Scroll: Zoom", 20, 20, 15, GREEN);
        DrawText("Ctrl + Left-Click: Set Master Point", 20, 40, 15, GOLD);
        DrawText("Shift + Left-Click: Remove Point", 20, 60, 15, RED);
        DrawText("Click Label: Edit Coords | Sliders: Reset/Adjust", 20, 80, 15, SKYBLUE);

        DrawRectangleRec(btnRestart, DARKGRAY); DrawText("RESTART", (int)btnRestart.x + 20, (int)btnRestart.y + 10, 20, WHITE);
        DrawRectangleRec(sliderK, DARKGRAY); DrawRectangle(sliderK.x + ((float)(kValue - 2) / (points.size() > 1 ? points.size() - 1 : 1)) * sliderK.width - 5, sliderK.y - 5, 10, 30, SKYBLUE);
        DrawText(TextFormat("K-Points: %d", kValue), (int)sliderK.x, (int)sliderK.y - 25, 20, SKYBLUE);
        DrawRectangleRec(btnMode, modeValue == 0 ? DARKBLUE : MAROON); DrawText(modeValue == 0 ? "MODE: ALL" : "MODE: SINGLE", (int)btnMode.x + 5, (int)btnMode.y + 5, 12, WHITE);
        DrawRectangleRec(sliderN, DARKGRAY); DrawRectangle(sliderN.x + ((float)(nVertices - 3) / 17.0f) * sliderN.width - 5, sliderN.y - 5, 10, 30, ORANGE);
        DrawText(TextFormat("Polygon (N): %d", nVertices), (int)sliderN.x, (int)sliderN.y - 25, 20, ORANGE);

        EndDrawing();
    }
    UnloadShader(shader);
    CloseWindow();
    return 0;
}