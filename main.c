#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "raylib.h"

#include "raymath.h"

#define RLIGHTS_IMPLEMENTATION
#include "rlights.h"

#if defined(PLATFORM_DESKTOP)
#define GLSL_VERSION            330
#else   // PLATFORM_RPI, PLATFORM_ANDROID, PLATFORM_WEB
#define GLSL_VERSION            100
#endif

void viewMode(char* s, float scale);

typedef struct Player {
    Model tank;
    Vector3 position;
    float scale;
    int kills;
} Player;

typedef struct Enemy {
    Model tank;
    Vector3 position;
    bool isVisible;
} Enemy;

typedef struct Shell {
    Model shell;
    Vector3 position;
    bool isVisible;
} Shell;

void updateShellLocation(Shell *pShell, Vector3 playerPosition) {
    if (pShell->position.z > playerPosition.z + 50.0f)
        pShell->isVisible = false;
    pShell->position.z += 1.0f;
}

bool checkHit(Shell* pShell, Enemy* enemies) {
    for (int i = 0; i < 3; i++) {
        if (!enemies[i].isVisible)
            continue;
        if (pShell->position.z > enemies[i].position.z - 1.0f
            && pShell->position.z < enemies[i].position.z + 5.0f) {
            if (pShell->position.x > enemies[i].position.x - 2.0f
            && pShell->position.x < enemies[i].position.x + 2.0f) {
                enemies[i].isVisible = false;
                return true;
            }
        }
    }
    return false;
}

Player* initDefaultPlayer() {
    Player* player = malloc(sizeof(Player));
    player->tank = LoadModel("resources/models/german-panzer-ww2-ausf-b-obj/german-panzer-ww2-ausf-b.obj");
    player->position = (Vector3) {0.0f, 0.0f, 0.0f};
    player->scale = 1.0f;
    return player;
}

Texture2D loadBackground() {
    Image background = LoadImage("resources/bg.png");
    Texture2D bg = LoadTextureFromImage(background);
    UnloadImage(background);
    return bg;
}

void GetRandomVectorForSpawn(Vector3 playerPos, Vector3* enemyPos) {
    enemyPos->y = 0.0f;
    enemyPos->z = GetRandomValue(playerPos.z + 30.0f, 80.0f);
    enemyPos->x = GetRandomValue(-50.0f, 80.0f);
}

void game() {
    time_t t;
    srand((unsigned) time(&t));
    InitAudioDevice();

    Texture2D background = loadBackground();
    Model ground = LoadModelFromMesh(GenMeshPlane(200.0f, 200.0f, 3, 3));
    ground.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = LoadTexture("resources/grass.png");

    Sound tankFire = LoadSound("resources/sound/tank_shot.wav");
    SetSoundVolume(tankFire, 1.0f);
    Sound enemyDestroyed = LoadSound("resources/sound/destroy.wav");
    SetSoundVolume(enemyDestroyed, 1.0f);

    Player* player = initDefaultPlayer();
    player->kills = 0;

    Shell* shell = (Shell*)malloc(sizeof(Shell));
    shell->shell = LoadModel("resources/models/bullet.obj");
    shell->isVisible = false;

    player->tank.transform = MatrixRotateY(3.1f);

    Shader shader = LoadShader(TextFormat("resources/shaders/glsl%i/lighting.vs", GLSL_VERSION),
                               TextFormat("resources/shaders/glsl%i/lighting.fs", GLSL_VERSION));
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");

    int ambientLoc = GetShaderLocation(shader, "ambient");
    SetShaderValue(shader, ambientLoc, (float[4]){ 0.1f, 0.1f, 0.1f, 1.0f }, SHADER_UNIFORM_VEC4);

    Enemy enemies[3];
    for (int i = 0; i < 3; i++) {
        enemies[i].tank = LoadModel("resources/models/german-panzer-ww2-ausf-b-obj/german-panzer-ww2-ausf-b.obj");
        enemies[i].position.x = GetRandomValue(-50.0f, 80.0f);
        enemies[i].position.y = 0;
        enemies[i].position.z = GetRandomValue(player->position.z + 30.0f, 80.0f);
        enemies[i].isVisible = true;

        for (int j = 0; j < enemies[i].tank.materialCount; j++)
            enemies[i].tank.materials[0].shader = shader;
    }

    ground.materials[0].shader = shader;
    for (int i = 0; i < player->tank.materialCount; i++)
        player->tank.materials[i].shader = shader;
    for (int i = 0; i < shell->shell.materialCount; i++)
        shell->shell.materials[i].shader = shader;


    Light lights[MAX_LIGHTS] = { 0 };
    lights[0] = CreateLight(LIGHT_POINT, (Vector3){ -2, 2, -2 }, Vector3Zero(), YELLOW, shader);
    lights[1] = CreateLight(LIGHT_POINT, (Vector3){ 2, 2, 2 }, Vector3Zero(), RED, shader);
    lights[2] = CreateLight(LIGHT_POINT, (Vector3){ -2, 2, 2 }, Vector3Zero(), GREEN, shader);
    lights[3] = CreateLight(LIGHT_POINT, (Vector3){ 2, 2,  -2}, Vector3Zero(), BLUE, shader);
    lights[4] = CreateLight(LIGHT_POINT, (Vector3){0, 4, 0}, Vector3Zero(), PURPLE, shader);

    Camera camera = { 0 };
    camera.position.x = player->position.x + 2.0f;
    camera.position.y = player->position.y + 15.0f;
    camera.position.z = player->position.z + 6.0f;
    camera.target = player->position;
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    SetCameraMode(camera, CAMERA_THIRD_PERSON);

    SetTargetFPS(60);

    while(!WindowShouldClose()) {
        camera.target = player->position;

        UpdateCamera(&camera);

        float cameraPos[3] = { camera.position.x, camera.position.y, camera.position.z };
        SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

        if (IsKeyPressed(KEY_Y)) { lights[0].enabled = !lights[0].enabled; }
        if (IsKeyPressed(KEY_R)) { lights[1].enabled = !lights[1].enabled; }
        if (IsKeyPressed(KEY_G)) { lights[2].enabled = !lights[2].enabled; }
        if (IsKeyPressed(KEY_B)) { lights[3].enabled = !lights[3].enabled; }
        if (IsKeyPressed(KEY_P)) { lights[4].enabled = !lights[4].enabled; }

        if (IsKeyDown(KEY_W)) {
            for (int i = 0; i < MAX_LIGHTS; i++)
                lights[i].position.z += 0.3f;
            player->position.z += 0.3;
        }
        if (IsKeyDown(KEY_S)) {
            for (int i = 0; i < MAX_LIGHTS; i++)
                lights[i].position.z -= 0.3f;
            player->position.z -= 0.3f;
        }
        if (IsKeyDown(KEY_A)) {
            for (int i = 0; i < MAX_LIGHTS; i++)
                lights[i].position.x += 0.3f;
            player->position.x += 0.3f;
        }
        if (IsKeyDown(KEY_D)) {
            for (int i = 0; i < MAX_LIGHTS; i++)
                lights[i].position.x -= 0.3f;
            player->position.x -= 0.3f;
        }


        if (IsKeyPressed(KEY_SPACE)) {
            shell->position.x = player->position.x - 0.3f;
            shell->position.y = player->position.y + 2.35f;
            shell->position.z = player->position.z + 6.4f;
            shell->isVisible = true;
            PlaySoundMulti(tankFire);
        }


        for (int i = 0; i < MAX_LIGHTS; i++)
            UpdateLightValues(shader, lights[i]);

        BeginDrawing();

        ClearBackground(BLACK);

        DrawTexture(background, 0, 0, WHITE);

        BeginMode3D(camera);

        DrawModel(ground, Vector3Zero(), 1.0f, WHITE);

        DrawModel(player->tank, player->position, 1.0f, WHITE);

        if (shell->isVisible) {
            DrawModel(shell->shell, shell->position, 0.1f, WHITE);
            updateShellLocation(shell, player->position);
            if (checkHit(shell, &enemies)) {
                shell->isVisible = false;
                StopSound(tankFire);
                PlaySoundMulti(enemyDestroyed);
                player->kills++;
                if (player->kills > 0 && player->kills %3 == 0) {
                    for (int i = 0; i < 3; i++) {
                        GetRandomVectorForSpawn(player->position, &enemies[i].position);
                        enemies[i].isVisible = true;
                    }
                }
            }

        }

        for (int i = 0; i < 3; i++) {
            if (enemies[i].isVisible) {
                DrawModel(enemies[i].tank, enemies[i].position, 1.0f, WHITE);
            }
        }

        for (int i = 0; i < MAX_LIGHTS; i++)
        {
            if (lights[i].enabled) DrawSphereEx(lights[i].position, 0.2f, 8, 8, lights[i].color);
            else
                DrawSphereWires(lights[i].position, 0.2f, 8, 8, ColorAlpha(lights[i].color, 0.3f));
        }

        DrawGrid(150, 1.0f);

        EndMode3D();

        DrawFPS(10, 10);

        DrawText(TextFormat("Kills: %d", player->kills), 670, 0, 20, BLACK);
        DrawText("Use keys [P][Y][R][G][B] to toggle lights", 10, 40, 16, BLACK);
        DrawText("Use [W][A][S][D] keys to move your tank", 10, 80, 16, BLACK);

        EndDrawing();

    }

    for (int i = 0; i < 3; i++) {
        UnloadModel(enemies[i].tank);
    }

    StopSoundMulti();

    UnloadModel(player->tank);
    UnloadModel(shell->shell);
    UnloadTexture(ground.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture);
    UnloadModel(ground);
    UnloadTexture(background);
    UnloadShader(shader);
    UnloadSound(tankFire);
    UnloadSound(enemyDestroyed);
    CloseAudioDevice();
    lightsCount = 0;

    free(player);

    return;
}

void viewMenu() {
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(WHITE);
        DrawText("Choose [1] to view Tiger II", 500, 250, 30, BLACK);
        DrawText("Choose [2] to view Low-Polygon tank", 500, 320, 30, BLACK);
        DrawText("Choose [3] to view Tiger H1", 500, 390, 30, BLACK);
        EndDrawing();

        if (IsKeyPressed(KEY_ONE))
            viewMode("resources/models/german-panzer-ww2-ausf-b-obj/german-panzer-ww2-ausf-b.obj", 1.0f);
        if (IsKeyPressed(KEY_TWO))
            viewMode("resources/models/Tank_LowP.obj", 0.05f);
        if (IsKeyPressed(KEY_THREE))
            viewMode("resources/models/tiger-tank-wot-obj/tiger-tank-wot.obj", 100.0f);
    }
    return;
}

void viewMode(char* path, float scale) {
    Texture2D background = loadBackground();
    Model ground = LoadModelFromMesh(GenMeshPlane(10.0f, 10.0f, 3, 3));

    //Player* player = initDefaultPlayer();
    Player* player = (Player*) malloc(sizeof(Player));
    player->tank = LoadModel(path);
    if (scale == 1.0f)
        player->position = Vector3Zero();
    if (scale == 0.05f)
        player->position = (Vector3) {0.0f, 1.2f,  0.0f};
    player->scale = scale;


    Shader shader = LoadShader(TextFormat("resources/shaders/glsl%i/lighting.vs", GLSL_VERSION),
                               TextFormat("resources/shaders/glsl%i/lighting.fs", GLSL_VERSION));
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");

    int ambientLoc = GetShaderLocation(shader, "ambient");
    SetShaderValue(shader, ambientLoc, (float[4]){ 0.1f, 0.1f, 0.1f, 1.0f }, SHADER_UNIFORM_VEC4);

    ground.materials[0].shader = shader;
    for (int i = 0; i < player->tank.materialCount; i++)
        player->tank.materials[i].shader = shader;

    Light lights[MAX_LIGHTS] = { 0 };
    lights[0] = CreateLight(LIGHT_POINT, (Vector3){ -2, 2, -2 }, Vector3Zero(), YELLOW, shader);
    lights[1] = CreateLight(LIGHT_POINT, (Vector3){ 2, 2, 2 }, Vector3Zero(), RED, shader);
    lights[2] = CreateLight(LIGHT_POINT, (Vector3){ -2, 2, 2 }, Vector3Zero(), GREEN, shader);
    lights[3] = CreateLight(LIGHT_POINT, (Vector3){ 2, 2,  -2}, Vector3Zero(), BLUE, shader);
    lights[4] = CreateLight(LIGHT_POINT, (Vector3){0, 4, 0}, Vector3Zero(), PURPLE, shader);

    Camera camera = { 0 };
    camera.position = (Vector3) {2.0f, 2.5f, 6.0f};
    camera.target = player->position;
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    SetCameraMode(camera, CAMERA_FIRST_PERSON);

    SetTargetFPS(60);

    while(!WindowShouldClose()) {
        UpdateCamera(&camera);

        float cameraPos[3] = { camera.position.x, camera.position.y, camera.position.z };
        SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

        if (IsKeyPressed(KEY_Y)) { lights[0].enabled = !lights[0].enabled; }
        if (IsKeyPressed(KEY_R)) { lights[1].enabled = !lights[1].enabled; }
        if (IsKeyPressed(KEY_G)) { lights[2].enabled = !lights[2].enabled; }
        if (IsKeyPressed(KEY_B)) { lights[3].enabled = !lights[3].enabled; }
        if (IsKeyPressed(KEY_P)) { lights[4].enabled = !lights[4].enabled; }

        for (int i = 0; i < MAX_LIGHTS; i++)
            UpdateLightValues(shader, lights[i]);

        BeginDrawing();

        ClearBackground(BLACK);

        DrawTexture(background, 0, 0, WHITE);

        BeginMode3D(camera);

        DrawModel(ground, Vector3Zero(), 1.0f, WHITE);

        DrawModel(player->tank, player->position, player->scale, WHITE);

        for (int i = 0; i < MAX_LIGHTS; i++)
        {
            if (lights[i].enabled) DrawSphereEx(lights[i].position, 0.2f, 8, 8, lights[i].color);
            else
                DrawSphereWires(lights[i].position, 0.2f, 8, 8, ColorAlpha(lights[i].color, 0.3f));
        }

        DrawGrid(150, 1.0f);

        EndMode3D();

        DrawFPS(10, 10);

        DrawText("Use keys [P][Y][R][G][B] to toggle lights", 10, 40, 16, BLACK);
        DrawText("Use [W]A][A][D] keys to move", 10, 80, 16, BLACK);

        EndDrawing();

    }
    UnloadModel(player->tank);
    UnloadModel(ground);
    UnloadTexture(background);
    UnloadShader(shader);
    lightsCount = 0;

    free(player);

    return;
}

void credits() {
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(WHITE);
        DrawText("Course work on computer graphics", 500, 150, 30, BLACK);
        DrawText("Done by Vasilev Vladimir, IDB-20-02", 500, 200, 30, BLACK);
        EndDrawing();
    }
    return;
}

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main() {
    const int screenWidth = 1400;
    const int screenHeight = 750;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "kursach");

    SetTargetFPS(60);

    bool isInMenu = true;
    while (!WindowShouldClose()) {
        while (isInMenu) {
            BeginDrawing();
            ClearBackground(WHITE);
            DrawText("Choose [1] to start a game", 500, 250, 30, BLACK);
            DrawText("Choose [2] to start view mode", 500, 320, 30, BLACK);
            DrawText("Choose [3] to show credits", 500, 390, 30, BLACK);
            EndDrawing();

            if (IsKeyPressed(KEY_ONE)) {
                game();
            }
            if (IsKeyPressed(KEY_TWO)) {
                viewMenu();
            }
            if (IsKeyPressed(KEY_THREE)) {
                credits();
            }

            if (IsKeyPressed(KEY_ESCAPE)) {
                break;
            }
        }

    }
    CloseWindow();

    return 0;
}