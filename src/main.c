#include <math.h>

#include "raylib.h"
#include "rlgl.h"

#define WINDOW_SIZE 800
#define CARD_HALF 1.1f
#define MAX_YAW_DEGREES 42.0f
#define MAX_PITCH_DEGREES 24.0f
#define TILT_RESPONSE 10.0f
#define FLIP_SPEED 2.0f
#define CHECKBOX_SIZE 24
#define CHECKBOX_STEP 34
#define BURN_SPEED 0.9f
#define GLARE_SPEED 4.0f
#define DIFFRACTION_SPEED 3.0f
#define TERA_SPEED 0.45f
#define LAYER_PARALLAX_X 1.2f
#define LAYER_PARALLAX_Y 0.6f
#define LAYER_HALF 1.3f
#define FRONT_DEPTH 0.2f
#define COLUMN_INSET 0.6f
#define COLUMN_DROP 0.15f

typedef enum { FX_AURA, FX_SPARKLE, FX_HOLO, FX_RIM, FX_FLIP, FX_BURN, FX_GLARE, FX_DIFFRACTION, FX_TERA, FX_COUNT } Effect;
typedef enum { RARITY_NORMAL, RARITY_RARE, RARITY_EPIC, RARITY_LEGENDARY, RARITY_COUNT } Rarity;
typedef struct { float depth, inset, u0, u1, v0, v1; } Column;

static const Column LEFT_COLUMN = {FRONT_DEPTH, COLUMN_INSET, 0.0f, 0.14f, 0.0f, 0.88f};
static const Column RIGHT_COLUMN = {FRONT_DEPTH, -COLUMN_INSET, 0.86f, 1.0f, 0.0f, 0.88f};

static const char *EFFECT_NAMES[FX_COUNT] = {"Aura", "Sparkles", "Holo", "Rim", "Flip", "Burn", "Glare", "Diffraction", "Terastal"};
static const char *RARITY_NAMES[RARITY_COUNT] = {"Normal", "Rare", "Epic", "Legendary"};
static const bool RARITY_EFFECTS[RARITY_COUNT][FX_COUNT] = {
    [RARITY_NORMAL] = {false, false, false, false, false, false, false, false, false},
    [RARITY_RARE] = {false, true, true, false, false, false, false, false, false},
    [RARITY_EPIC] = {true, true, true, true, false, false, false, false, false},
    [RARITY_LEGENDARY] = {true, true, true, true, false, false, true, false, false},
};

static Texture2D load_texture(const char *directory, const char *name)
{
    Texture2D texture = LoadTexture(TextFormat("%sdata/%s", directory, name));
    SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    return texture;
}

static float approach(float value, float target, float blend)
{
    return value + (target - value) * blend;
}

static float move_toward(float value, float target, float step)
{
    if (value < target) return fminf(value + step, target);
    return fmaxf(value - step, target);
}

static bool checkbox(int row, const char *label, bool on)
{
    Rectangle box = {16, 16 + row * CHECKBOX_STEP, CHECKBOX_SIZE, CHECKBOX_SIZE};
    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), box);
    DrawRectangleRec(box, on ? RAYWHITE : BLANK);
    DrawRectangleLinesEx(box, 2, RAYWHITE);
    DrawText(label, (int)box.x + CHECKBOX_SIZE + 10, (int)box.y + 2, 20, RAYWHITE);
    return on != clicked;
}

static void apply_rarity(bool enabled[FX_COUNT], Rarity rarity)
{
    for (int i = 0; i < FX_COUNT; i++) enabled[i] = RARITY_EFFECTS[rarity][i];
}

static void push_card_transform(Vector2 tilt, float flip)
{
    rlPushMatrix();
    rlRotatef(flip * 180.0f, 0, 1, 0);
    rlRotatef(tilt.y * MAX_PITCH_DEGREES, 1, 0, 0);
    rlRotatef(tilt.x * MAX_YAW_DEGREES, 0, 1, 0);
}

static void draw_quad(Texture2D texture, float x0, float y0, float x1, float y1, float u0, float u1, float v0, float v1)
{
    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);
    rlTexCoord2f(u0, v1);
    rlVertex3f(x0, y0, 0);
    rlTexCoord2f(u1, v1);
    rlVertex3f(x1, y0, 0);
    rlTexCoord2f(u1, v0);
    rlVertex3f(x1, y1, 0);
    rlTexCoord2f(u0, v0);
    rlVertex3f(x0, y1, 0);
    rlEnd();
    rlSetTexture(0);
}

static void draw_card(Texture2D frame, Vector2 tilt, float flip)
{
    push_card_transform(tilt, flip);
    draw_quad(frame, -CARD_HALF, -CARD_HALF, CARD_HALF, CARD_HALF, 0.0f, 1.0f, 0.0f, 1.0f);
    rlPopMatrix();
}

static void draw_column(Shader shader, int mapLoc, Texture2D texture, Vector2 tilt, float flip, Column column)
{
    float x0 = column.inset - LAYER_HALF + 2.0f * LAYER_HALF * column.u0;
    float x1 = column.inset - LAYER_HALF + 2.0f * LAYER_HALF * column.u1;
    float y0 = LAYER_HALF - 2.0f * LAYER_HALF * column.v1;
    float y1 = LAYER_HALF - 2.0f * LAYER_HALF * column.v0;
    float map[3] = {
        (column.inset - LAYER_HALF + CARD_HALF) / (2.0f * CARD_HALF),
        0.5f - (LAYER_HALF - COLUMN_DROP) / (2.0f * CARD_HALF),
        LAYER_HALF / CARD_HALF,
    };
    BeginShaderMode(shader);
    SetShaderValue(shader, mapLoc, map, SHADER_UNIFORM_VEC3);
    push_card_transform(tilt, flip);
    rlTranslatef(-tilt.x * column.depth * LAYER_PARALLAX_X, -tilt.y * column.depth * LAYER_PARALLAX_Y - COLUMN_DROP, column.depth);
    draw_quad(texture, x0, y0, x1, y1, column.u0, column.u1, column.v0, column.v1);
    rlPopMatrix();
    EndShaderMode();
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_SIZE, WINDOW_SIZE, "foil card");
    SetTargetFPS(60);

    const char *directory = GetApplicationDirectory();
    Texture2D frame = load_texture(directory, "frame.png");
    Texture2D background = load_texture(directory, "background.jpg");
    Texture2D character = load_texture(directory, "character.png");
    Texture2D mask = load_texture(directory, "mask.jpg");
    Texture2D cardBack = load_texture(directory, "back.png");
    GenTextureMipmaps(&cardBack);
    SetTextureFilter(cardBack, TEXTURE_FILTER_TRILINEAR);
    Shader shader = LoadShader(0, TextFormat("%sdata/foil.fs", directory));

    int backgroundLoc = GetShaderLocation(shader, "background");
    int characterLoc = GetShaderLocation(shader, "character");
    int maskLoc = GetShaderLocation(shader, "mask");
    int cardBackLoc = GetShaderLocation(shader, "cardBack");
    int tiltLoc = GetShaderLocation(shader, "tilt");
    int timeLoc = GetShaderLocation(shader, "time");
    int burnLoc = GetShaderLocation(shader, "burn");
    int rarityLoc = GetShaderLocation(shader, "rarity");
    int edgeOnLoc = GetShaderLocation(shader, "edgeOn");
    int glareLoc = GetShaderLocation(shader, "glare");
    int diffractionLoc = GetShaderLocation(shader, "diffraction");
    int teraLoc = GetShaderLocation(shader, "tera");
    int effectsLoc = GetShaderLocation(shader, "effects");
    int layerMapLoc = GetShaderLocation(shader, "layerMap");

    Camera3D camera = {{0, 0, 2.9f}, {0, 0, 0}, {0, 1, 0}, 55.0f, CAMERA_PERSPECTIVE};
    Vector2 tilt = {0, 0};
    float flip = 0.0f;
    float burn = 0.0f;
    float glare = 0.0f;
    float diffraction = 0.0f;
    float tera = 0.0f;
    Rarity rarity = RARITY_NORMAL;
    bool enabled[FX_COUNT];
    apply_rarity(enabled, rarity);

    rlDisableBackfaceCulling();
    MaximizeWindow();

    while (!WindowShouldClose()) {
        float delta = GetFrameTime();
        Vector2 mouse = GetMousePosition();
        Vector2 target = {(mouse.x / GetScreenWidth() - 0.5f) * 2.0f, (mouse.y / GetScreenHeight() - 0.5f) * 2.0f};
        float blend = fminf(1.0f, TILT_RESPONSE * delta);
        tilt.x = approach(tilt.x, target.x, blend);
        tilt.y = approach(tilt.y, target.y, blend);
        flip = move_toward(flip, enabled[FX_FLIP] ? 1.0f : 0.0f, FLIP_SPEED * delta);
        burn = move_toward(burn, enabled[FX_BURN] ? 1.0f : 0.0f, BURN_SPEED * delta);
        glare = move_toward(glare, enabled[FX_GLARE] ? 1.0f : 0.0f, GLARE_SPEED * delta);
        diffraction = move_toward(diffraction, enabled[FX_DIFFRACTION] ? 1.0f : 0.0f, DIFFRACTION_SPEED * delta);
        tera = move_toward(tera, enabled[FX_TERA] ? 1.0f : 0.0f, TERA_SPEED * delta);
        if (IsKeyPressed(KEY_R)) {
            rarity = (Rarity)((rarity + 1) % RARITY_COUNT);
            apply_rarity(enabled, rarity);
        }

        float yaw = (flip * 180.0f + tilt.x * MAX_YAW_DEGREES) * DEG2RAD;
        float rarityValue = fmaxf((float)rarity - 1.0f, 0.0f);
        float edgeOn = fabsf(sinf(yaw));
        float time = (float)GetTime();
        float cardMap[3] = {0.0f, 0.0f, 0.0f};
        float effects[4] = {enabled[FX_AURA], enabled[FX_SPARKLE], enabled[FX_HOLO], enabled[FX_RIM]};

        BeginDrawing();
        ClearBackground((Color){18, 22, 30, 255});
        BeginMode3D(camera);
        BeginShaderMode(shader);
        SetShaderValueTexture(shader, backgroundLoc, background);
        SetShaderValueTexture(shader, characterLoc, character);
        SetShaderValueTexture(shader, maskLoc, mask);
        SetShaderValueTexture(shader, cardBackLoc, cardBack);
        SetShaderValue(shader, tiltLoc, &tilt, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, timeLoc, &time, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, burnLoc, &burn, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, glareLoc, &glare, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, diffractionLoc, &diffraction, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, teraLoc, &tera, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, rarityLoc, &rarityValue, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, edgeOnLoc, &edgeOn, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, effectsLoc, effects, SHADER_UNIFORM_VEC4);
        SetShaderValue(shader, layerMapLoc, cardMap, SHADER_UNIFORM_VEC3);
        draw_card(frame, tilt, flip);
        EndShaderMode();
        if (cosf(yaw) > 0.0f) {
            draw_column(shader, layerMapLoc, character, tilt, flip, LEFT_COLUMN);
            draw_column(shader, layerMapLoc, character, tilt, flip, RIGHT_COLUMN);
        }
        EndMode3D();
        for (int i = 0; i < FX_COUNT; i++) enabled[i] = checkbox(i, EFFECT_NAMES[i], enabled[i]);
        DrawText(TextFormat("Rarity: %s (R)", RARITY_NAMES[rarity]), 16, GetScreenHeight() - 40, 20, RAYWHITE);
        EndDrawing();
    }

    UnloadShader(shader);
    UnloadTexture(cardBack);
    UnloadTexture(mask);
    UnloadTexture(character);
    UnloadTexture(background);
    UnloadTexture(frame);
    CloseWindow();
    return 0;
}
