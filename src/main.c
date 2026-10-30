#include <math.h>
#include <time.h>

#include "raylib.h"
#include "rlgl.h"

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
#define CARD_COUNT 22
#define BACK_COUNT 2
#define RING_SPACING 1.2f
#define RING_RADIUS (CARD_HALF * RING_SPACING / sinf(PI / CARD_COUNT))
#define RING_STEP (360.0f / CARD_COUNT)
#define RING_RESPONSE 6.0f
#define CAMERA_DISTANCE 2.9f
#define CAMERA_HEIGHT 0.6f
#define NAV_BUTTON_SIZE 44
#define NAV_BUTTON_GAP 12

typedef enum { FX_AURA, FX_SPARKLE, FX_HOLO, FX_RIM, FX_FLIP, FX_BURN, FX_GLARE, FX_DIFFRACTION, FX_TERA, FX_COUNT } Effect;
typedef enum { RARITY_NORMAL, RARITY_RARE, RARITY_EPIC, RARITY_LEGENDARY, RARITY_COUNT } Rarity;
typedef struct { float depth, inset, u0, u1, v0, v1; } Column;
typedef struct { Texture2D frame, background, character, mask; } CardTextures;
typedef struct { Shader shader; int background, character, mask, cardBack, edgeOn, layerMap; } CardShader;

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

static Texture2D load_texture(const char *path)
{
    Texture2D texture = LoadTexture(path);
    SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    return texture;
}

static Texture2D load_card_texture(const char *directory, int card, const char *name)
{
    return load_texture(TextFormat("%sdata/%02d/%s", directory, card, name));
}

static CardTextures load_card(const char *directory, int card)
{
    return (CardTextures){
        load_card_texture(directory, card, "frame.png"),
        load_card_texture(directory, card, "background.jpg"),
        load_card_texture(directory, card, "character.png"),
        load_card_texture(directory, card, "mask.jpg"),
    };
}

static void unload_card(CardTextures card)
{
    UnloadTexture(card.mask);
    UnloadTexture(card.character);
    UnloadTexture(card.background);
    UnloadTexture(card.frame);
}

static Texture2D load_back(const char *directory, int back)
{
    Texture2D texture = load_texture(TextFormat("%sdata/back%02d.png", directory, back));
    GenTextureMipmaps(&texture);
    SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
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

static bool nav_button(Rectangle box, const char *label)
{
    bool hovered = CheckCollisionPointRec(GetMousePosition(), box);
    DrawRectangleRec(box, hovered ? Fade(RAYWHITE, 0.25f) : BLANK);
    DrawRectangleLinesEx(box, 2, RAYWHITE);
    DrawText(label, (int)(box.x + (box.width - MeasureText(label, 24)) / 2), (int)box.y + 10, 24, RAYWHITE);
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void apply_rarity(bool enabled[FX_COUNT], Rarity rarity)
{
    for (int i = 0; i < FX_COUNT; i++) enabled[i] = RARITY_EFFECTS[rarity][i];
}

static float view_yaw(float angle, Vector2 tilt, float flip)
{
    float position = angle * DEG2RAD;
    float toCameraX = -RING_RADIUS * sinf(position);
    float toCameraZ = RING_RADIUS + CAMERA_DISTANCE - RING_RADIUS * cosf(position);
    return (angle + flip * 180.0f + tilt.x * MAX_YAW_DEGREES) * DEG2RAD - atan2f(toCameraX, toCameraZ);
}

static void push_card_transform(Vector2 tilt, float flip, float angle)
{
    rlPushMatrix();
    rlRotatef(angle, 0, 1, 0);
    rlTranslatef(0, 0, RING_RADIUS);
    rlRotatef(flip * 180.0f, 0, 1, 0);
    rlRotatef(tilt.y * MAX_PITCH_DEGREES, 1, 0, 0);
    rlRotatef(tilt.x * MAX_YAW_DEGREES, 0, 1, 0);
}

static void bind_card(CardShader cardShader, CardTextures card, Texture2D cardBack)
{
    SetShaderValueTexture(cardShader.shader, cardShader.background, card.background);
    SetShaderValueTexture(cardShader.shader, cardShader.character, card.character);
    SetShaderValueTexture(cardShader.shader, cardShader.mask, card.mask);
    SetShaderValueTexture(cardShader.shader, cardShader.cardBack, cardBack);
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

static void draw_card(CardShader cardShader, CardTextures card, Texture2D cardBack, Vector2 tilt, float flip, float angle, float edgeOn)
{
    float noMap[3] = {0.0f, 0.0f, 0.0f};
    BeginShaderMode(cardShader.shader);
    bind_card(cardShader, card, cardBack);
    SetShaderValue(cardShader.shader, cardShader.edgeOn, &edgeOn, SHADER_UNIFORM_FLOAT);
    SetShaderValue(cardShader.shader, cardShader.layerMap, noMap, SHADER_UNIFORM_VEC3);
    push_card_transform(tilt, flip, angle);
    draw_quad(card.frame, -CARD_HALF, -CARD_HALF, CARD_HALF, CARD_HALF, 0.0f, 1.0f, 0.0f, 1.0f);
    rlPopMatrix();
    EndShaderMode();
}

static void draw_column(CardShader cardShader, CardTextures card, Texture2D cardBack, Vector2 tilt, float flip, float angle, Column column)
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
    BeginShaderMode(cardShader.shader);
    bind_card(cardShader, card, cardBack);
    SetShaderValue(cardShader.shader, cardShader.layerMap, map, SHADER_UNIFORM_VEC3);
    push_card_transform(tilt, flip, angle);
    rlTranslatef(-tilt.x * column.depth * LAYER_PARALLAX_X, -tilt.y * column.depth * LAYER_PARALLAX_Y - COLUMN_DROP, column.depth);
    draw_quad(card.character, x0, y0, x1, y1, column.u0, column.u1, column.v0, column.v1);
    rlPopMatrix();
    EndShaderMode();
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280 , 720, "foil card");
    SetRandomSeed((unsigned int)time(NULL));

    const char *directory = GetApplicationDirectory();
    Texture2D backs[BACK_COUNT];
    for (int i = 0; i < BACK_COUNT; i++) backs[i] = load_back(directory, i);
    CardTextures cards[CARD_COUNT];
    int backOf[CARD_COUNT];
    for (int i = 0; i < CARD_COUNT; i++) {
        cards[i] = load_card(directory, i);
        backOf[i] = GetRandomValue(0, BACK_COUNT - 1);
    }
    Shader shader = LoadShader(0, TextFormat("%sdata/foil.fs", directory));

    CardShader cardShader = {
        shader,
        GetShaderLocation(shader, "background"),
        GetShaderLocation(shader, "character"),
        GetShaderLocation(shader, "mask"),
        GetShaderLocation(shader, "cardBack"),
        GetShaderLocation(shader, "edgeOn"),
        GetShaderLocation(shader, "layerMap"),
    };
    int tiltLoc = GetShaderLocation(shader, "tilt");
    int timeLoc = GetShaderLocation(shader, "time");
    int burnLoc = GetShaderLocation(shader, "burn");
    int rarityLoc = GetShaderLocation(shader, "rarity");
    int glareLoc = GetShaderLocation(shader, "glare");
    int diffractionLoc = GetShaderLocation(shader, "diffraction");
    int teraLoc = GetShaderLocation(shader, "tera");
    int effectsLoc = GetShaderLocation(shader, "effects");

    Camera3D camera = {{0, CAMERA_HEIGHT, RING_RADIUS + CAMERA_DISTANCE}, {0, 0, RING_RADIUS}, {0, 1, 0}, 55.0f, CAMERA_PERSPECTIVE};
    Vector2 tilt = {0, 0};
    float flip = 0.0f;
    float burn = 0.0f;
    float glare = 0.0f;
    float diffraction = 0.0f;
    float tera = 0.0f;
    float ringAngle = 0.0f;
    int selection = 0;
    int step = 0;
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
        selection += step + (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) - (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A));
        step = 0;
        ringAngle = approach(ringAngle, selection * RING_STEP, fminf(1.0f, RING_RESPONSE * delta));
        flip = move_toward(flip, enabled[FX_FLIP] ? 1.0f : 0.0f, FLIP_SPEED * delta);
        burn = move_toward(burn, enabled[FX_BURN] ? 1.0f : 0.0f, BURN_SPEED * delta);
        glare = move_toward(glare, enabled[FX_GLARE] ? 1.0f : 0.0f, GLARE_SPEED * delta);
        diffraction = move_toward(diffraction, enabled[FX_DIFFRACTION] ? 1.0f : 0.0f, DIFFRACTION_SPEED * delta);
        tera = move_toward(tera, enabled[FX_TERA] ? 1.0f : 0.0f, TERA_SPEED * delta);
        if (IsKeyPressed(KEY_R)) {
            rarity = (Rarity)((rarity + 1) % RARITY_COUNT);
            apply_rarity(enabled, rarity);
        }

        int current = ((selection % CARD_COUNT) + CARD_COUNT) % CARD_COUNT;
        float rarityValue = fmaxf((float)rarity - 1.0f, 0.0f);
        float time = (float)GetTime();
        float effects[4] = {enabled[FX_AURA], enabled[FX_SPARKLE], enabled[FX_HOLO], enabled[FX_RIM]};

        BeginDrawing();
        ClearBackground((Color){18, 22, 30, 255});
        BeginMode3D(camera);
        SetShaderValue(shader, tiltLoc, &tilt, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, timeLoc, &time, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, burnLoc, &burn, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, glareLoc, &glare, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, diffractionLoc, &diffraction, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, teraLoc, &tera, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, rarityLoc, &rarityValue, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, effectsLoc, effects, SHADER_UNIFORM_VEC4);
        for (int i = 0; i < CARD_COUNT; i++) {
            float angle = i * RING_STEP - ringAngle;
            float yaw = view_yaw(angle, tilt, flip);
            draw_card(cardShader, cards[i], backs[backOf[i]], tilt, flip, angle, fabsf(sinf(yaw)));
            if (cosf(yaw) > 0.0f) {
                draw_column(cardShader, cards[i], backs[backOf[i]], tilt, flip, angle, LEFT_COLUMN);
                draw_column(cardShader, cards[i], backs[backOf[i]], tilt, flip, angle, RIGHT_COLUMN);
            }
        }
        EndMode3D();
        for (int i = 0; i < FX_COUNT; i++) enabled[i] = checkbox(i, EFFECT_NAMES[i], enabled[i]);
        DrawText(TextFormat("Rarity: %s (R)", RARITY_NAMES[rarity]), 16, GetScreenHeight() - 40, 20, RAYWHITE);
        const char *stats = TextFormat("%d FPS  %.2f ms", GetFPS(), delta * 1000.0f);
        DrawText(stats, GetScreenWidth() - MeasureText(stats, 20) - 16, 16, 20, RAYWHITE);
        const char *counter = TextFormat("%02d / %02d", current + 1, CARD_COUNT);
        int counterWidth = MeasureText(counter, 24);
        float navY = (float)GetScreenHeight() - NAV_BUTTON_SIZE - 20;
        float navX = (GetScreenWidth() - counterWidth) / 2.0f;
        DrawText(counter, (int)navX, (int)navY + 10, 24, RAYWHITE);
        if (nav_button((Rectangle){navX - NAV_BUTTON_GAP - NAV_BUTTON_SIZE, navY, NAV_BUTTON_SIZE, NAV_BUTTON_SIZE}, "<")) step = -1;
        if (nav_button((Rectangle){navX + counterWidth + NAV_BUTTON_GAP, navY, NAV_BUTTON_SIZE, NAV_BUTTON_SIZE}, ">")) step = 1;
        EndDrawing();
    }

    UnloadShader(shader);
    for (int i = 0; i < CARD_COUNT; i++) unload_card(cards[i]);
    for (int i = 0; i < BACK_COUNT; i++) UnloadTexture(backs[i]);
    CloseWindow();
    return 0;
}
