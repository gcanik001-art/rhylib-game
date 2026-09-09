#include "raylib.h"
#include <math.h>
#define radius 30
#define brickwidth 180
#define brickheight 100
#define platforms_count 200
#define platformbrick_width 50
#define platformbrick_height 50
float health = 100.0f;
float maxhealth = 100.0f;
float damage = 20.0f;
float damagetimes = 0;
int gameover = 0;
int gamestarted = 0;
int gamepaused = 0;
// game score
int gamescore = 0;
float maxdistance = 0;
// platfroms stucrure
typedef struct platform
{
    float x;
    float y;
    int length;
    int height;
} platform;
platform platforms[platforms_count] = {
    {-100, 0, 5, 16},
    {0, 0, 8, 8},
    {600, 0, 20, 5},
    {1000, 550, 3, 3},
    {1700, 550, 3, 3},
    {1800, 450, 10, 3},
    {2450, 300, 26, 3},
    {4000, 300, 40, 2},
    {4500, 550, 4, 1},
    {4500, 550, 2, 3},
    {4900, 500, 6, 1},
    {5000, 550, 2, 1},
    {5350, 550, 8, 1},
    {5900, 300, 3, 6},
    {6200, 600, 2, 2},
    {6300, 550, 2, 3},
    {6400, 500, 2, 4},
    {6500, 450, 2, 5},
    {6600, 400, 2, 6},
    {6700, 350, 2, 7},
    {6800, 300, 2, 8},
    {7500, 0, 3, 8},
    {8300, 350, 2, 3},
    {8400, 350, 10, 1},
    {9100, 0, 2, 7},
    {9400, 360, 2, 3},
    {9650, 500, 1, 4},
    {9800, 0, 2, 6},
    {10100, 0, 2, 8},
    {11500, 0, 3, 8},
    {12000, 0, 3, 8},
    {12000, 550, 8, 3},
    {12500, 0, 2, 1},
    {13000, 550, 2, 7},
    {13500, 0, 2, 3},
    {14000, 550, 2, 7},
    {14500, 0, 2, 3},
    {15000, 550, 2, 7},
    {15500, 0, 2, 3},
    {16000, 550, 2, 7},
    {16500, 0, 2, 3},
    {17000, 550, 2, 7},
    {17500, 0, 2, 3},
    {18000, 550, 2, 7},
    {18500, 0, 2, 3},
    {19000, 550, 2, 7},
    {19500, 0, 2, 3},
    {20000, 550, 2, 7},
    {20500, 0, 2, 3},
    {20952, 550, 3, 3},
    {21474, 0, 3, 7},
    {21761, 550, 3, 3},
    {22480, 0, 3, 8},
    {23271, 550, 3, 3},
    {24056, 0, 5, 6},
    {24802, 550, 3, 4},
    {25444, 0, 3, 6},
    {25690, 550, 2, 4},
    {26141, 0, 3, 6},
    {26884, 0, 3, 7},
    {27180, 550, 2, 3},
    {27685, 0, 3, 7},
    {27958, 550, 3, 3},
    {28381, 0, 3, 6},
    {28998, 550, 5, 3},
    {29479, 0, 3, 7},
    {30072, 0, 5, 7},
    {30729, 0, 3, 7},
    {31142, 550, 5, 3},
    {31582, 0, 4, 6},
    {32187, 0, 6, 3},
    {32722, 550, 4, 7},
    {33371, 550, 3, 3},
    {33893, 550, 2, 6},
    {34432, 0, 3, 3},
    {35151, 0, 3, 7},
    {35836, 550, 3, 7},
    {36512, 0, 3, 7},
    {36763, 550, 4, 3},
    {37154, 550, 2, 7},
    {37729, 0, 4, 8},
    {38017, 550, 2, 3},
    {38515, 0, 3, 4},
    {39059, 550, 2, 4},
    {39634, 0, 3, 7},
    {40325, 550, 2, 9},
    {41016, 0, 3, 5},
    {41746, 0, 3, 8},
    {42537, 550, 3, 4},
    {43173, 0, 3, 4},
    {43459, 550, 3, 8},
    {44221, 0, 5, 3},
    {44949, 550, 3, 11},
    {45692, 0, 5, 5},
    {46052, 0, 4, 8},
    {46431, 550, 2, 4},
    {47144, 0, 3, 11},
    {47936, 0, 2, 11},
    {48454, 0, 30, 4}};
// platfrom ar sate ball ar collision
void checkcollision(Vector2 *ballposition, Vector2 previousposition, float *verticalvelocity, int *jumpcount)
{
    for (int i = 0; i < platforms_count; i++)
    {
        if (platforms[i].length <= 0 || platforms[i].height <= 0)
            continue;
        float left = platforms[i].x;
        float right = left + platforms[i].length * platformbrick_width;
        float top = platforms[i].y;
        float bottom = top + platforms[i].height * platformbrick_height;
        if (ballposition->x + radius > left && ballposition->x - radius < right && ballposition->y + radius > top && ballposition->y - radius < bottom)
        {
            float prevballleft = previousposition.x - radius;
            float prevballright = previousposition.x + radius;
            float prevballtop = previousposition.y - radius;
            float prevballbottom = previousposition.y + radius;
            if (prevballbottom <= top)
            {
                ballposition->y = top - radius;
                *verticalvelocity = 0.0f;
                *jumpcount = 0;
            }
            else if (prevballtop >= bottom)
            {
                ballposition->y = bottom + radius;
                *verticalvelocity = fabsf(*verticalvelocity) * 0.5f + 100.0f;
            }
            else if (prevballright <= left)
            {
                ballposition->x = left - radius;
            }
            else if (prevballleft >= right)
            {
                ballposition->x = right + radius;
            }
        }
    }
}
// spike stucture
typedef struct
{
    float x;
    float y;
    float width;
    float height;
    float miny;
    float maxy;
    float speed;
    int direction;
    int type;
} spike;
#define spike_count 75
spike spikes[spike_count] = {
    {800, 655, 45, 45, 150, 655, 160, 1, 0},
    {1450, 655, 45, 45, 100, 655, 180, -1, 0},
    {1900, 405, 45, 45, 100, 405, 150, 1, 2},
    {2100, 600, 45, 45, 200, 700, 150, 1, 3},
    {2350, 655, 45, 45, 120, 655, 170, 1, 0},
    {2700, 255, 45, 45, 50, 255, 160, -1, 2},
    {3000, 450, 45, 45, 100, 450, 160, -1, 3},
    {3850, 655, 45, 45, 100, 655, 160, -1, 0},
    {4200, 255, 45, 45, 50, 255, 150, 1, 2},
    {4400, 655, 45, 45, 150, 655, 170, 1, 0},
    {4550, 450, 45, 45, 100, 450, 150, 1, 3},
    {4800, 655, 45, 45, 100, 655, 180, -1, 0},
    {5250, 655, 45, 45, 120, 655, 160, 1, 0},
    {5950, 255, 45, 45, 50, 255, 160, -1, 2},
    {6000, 450, 45, 45, 100, 450, 160, -1, 3},
    {6100, 655, 45, 45, 100, 655, 170, -1, 0},
    {6400, 455, 45, 45, 150, 455, 150, 1, 2},
    {6700, 255, 45, 45, 50, 255, 160, -1, 2},
    {8050, 100, 45, 45, 0, 300, 160, 1, 1},
    {8300, 305, 45, 45, 50, 305, 150, 1, 2},
    {9000, 100, 45, 45, 0, 300, 170, -1, 1},
    {9300, 100, 45, 45, 0, 300, 160, 1, 1},
    {9400, 315, 45, 45, 50, 315, 160, -1, 2},
    {10200, 100, 45, 45, 0, 300, 170, -1, 1},
    {10800, 655, 45, 45, 150, 655, 190, 1, 0},
    {11200, 655, 45, 45, 120, 655, 160, -1, 0},
    {11900, 100, 45, 45, 0, 300, 160, 1, 1},
    {12300, 500, 45, 45, 100, 500, 180, 1, 2},
    {12900, 100, 45, 45, 0, 300, 170, -1, 1},
    {13400, 100, 45, 45, 0, 300, 160, 1, 1},
    {13900, 100, 45, 45, 0, 300, 170, -1, 1},
    {14200, 655, 45, 45, 150, 655, 180, 1, 0},
    {14800, 655, 45, 45, 100, 655, 200, -1, 0},
    {15300, 100, 45, 45, 0, 300, 160, 1, 1},
    {15800, 655, 45, 45, 120, 655, 170, -1, 0},
    {16300, 100, 45, 45, 0, 300, 190, 1, 1},
    {16800, 655, 45, 45, 150, 655, 180, -1, 0},
    {17300, 100, 45, 45, 0, 300, 160, 1, 1},
    {17800, 655, 45, 45, 100, 655, 170, -1, 0},
    {18300, 100, 45, 45, 0, 300, 180, 1, 1},
    {18800, 655, 45, 45, 150, 655, 190, -1, 0},
    {19300, 100, 45, 45, 0, 300, 160, 1, 1},
    {19800, 655, 45, 45, 100, 655, 200, -1, 0},
    {20300, 100, 45, 45, 0, 300, 170, 1, 1},
    {20800, 500, 45, 45, 150, 500, 160, -1, 2},
    {21200, 655, 45, 45, 100, 655, 180, 1, 0},
    {21800, 500, 45, 45, 150, 500, 170, -1, 2},
    {22600, 655, 45, 45, 100, 655, 190, 1, 0},
    {23000, 100, 45, 45, 0, 300, 180, -1, 1},
    {23600, 500, 45, 45, 150, 500, 160, 1, 2},
    {24300, 655, 45, 45, 100, 655, 200, -1, 0},
    {25000, 500, 45, 45, 150, 500, 170, 1, 2},
    {25800, 655, 45, 45, 100, 655, 180, -1, 0},
    {26500, 100, 45, 45, 0, 300, 190, 1, 1},
    {27400, 500, 45, 45, 150, 500, 160, -1, 2},
    {28100, 655, 45, 45, 100, 655, 180, 1, 0},
    {28700, 100, 45, 45, 0, 300, 170, -1, 1},
    {29200, 500, 45, 45, 150, 500, 200, 1, 2},
    {30300, 655, 45, 45, 100, 655, 180, -1, 0},
    {30900, 100, 45, 45, 0, 300, 190, 1, 1},
    {31400, 500, 45, 45, 150, 500, 160, -1, 2},
    {32000, 655, 45, 45, 100, 655, 170, 1, 0},
    {32500, 100, 45, 45, 0, 300, 180, -1, 1},
    {33100, 500, 45, 45, 150, 500, 200, 1, 2},
    {33600, 655, 45, 45, 100, 655, 160, -1, 0},
    {34200, 100, 45, 45, 0, 300, 190, 1, 1},
    {34800, 500, 45, 45, 150, 500, 170, -1, 2},
    {35500, 655, 45, 45, 100, 655, 180, 1, 0},
    {36200, 100, 45, 45, 0, 300, 200, -1, 1},
    {37000, 500, 45, 45, 150, 500, 160, 1, 2},
    {37500, 655, 45, 45, 100, 655, 180, -1, 0},
    {38200, 100, 45, 45, 0, 300, 190, 1, 1},
    {38800, 500, 45, 45, 150, 500, 170, -1, 2},
    {39400, 655, 45, 45, 100, 655, 180, 1, 0},
    {40100, 100, 45, 45, 0, 300, 200, -1, 1},
    {40800, 500, 45, 45, 150, 500, 160, 1, 2},
    {41400, 655, 45, 45, 100, 655, 180, -1, 0},
    {42200, 100, 45, 45, 0, 300, 190, 1, 1},
    {43000, 500, 45, 45, 150, 500, 170, -1, 2},
    {44000, 655, 45, 45, 100, 655, 180, 1, 0},
    {45000, 100, 45, 45, 0, 300, 200, -1, 1},
    {46200, 500, 45, 45, 150, 500, 160, 1, 2},
    {47000, 655, 45, 45, 100, 655, 180, -1, 0},
    {48000, 100, 45, 45, 0, 300, 190, 1, 1}};
Rectangle getspikerect(spike *s)
{
    return (Rectangle){s->x, s->y, s->width, s->height};
}
// spike kototok upore niche jabe and hosse je //platfrom ar sate collision hole jah jah hobe
void updatespikes(float dt)
{
    for (int i = 0; i < spike_count; i++)
    {
        // moving
        spikes[i].y += spikes[i].speed * spikes[i].direction * dt;
        // standard boundaries check (before platform check)
        if (spikes[i].y >= spikes[i].maxy)
        {
            spikes[i].y = spikes[i].maxy;
            spikes[i].direction = -1;
        }
        else if (spikes[i].y <= spikes[i].miny)
        {
            spikes[i].y = spikes[i].miny;
            spikes[i].direction = 1;
        }
        // platform collision check
        Rectangle srect = getspikerect(&spikes[i]);
        for (int p = 0; p < platforms_count; p++)
        {
            if (platforms[p].length <= 0 || platforms[p].height <= 0)
                continue;
            Rectangle prect = {
                platforms[p].x, platforms[p].y, platforms[p].length * platformbrick_width, platforms[p].height * platformbrick_height};
            if (CheckCollisionRecs(srect, prect))
            {
                // upward spike platfrom ar niche lage
                if (spikes[i].direction == -1)
                {
                    spikes[i].y = prect.y + prect.height + 1.0f;
                    spikes[i].direction = 1;
                }
                // downward spike platfrom ar upore lage
                else if (spikes[i].direction == 1)
                {
                    spikes[i].y = prect.y - spikes[i].height - 1.0f;
                    spikes[i].direction = -1;
                }
                break;
            }
        }
    }
}
void checkspikecollision(Vector2 *ballposition, float *health, float *damagetimes)
{
    Rectangle ballrect = {ballposition->x - radius, ballposition->y - radius, radius * 2, radius * 2};
    for (int i = 0; i < spike_count; i++)
    {
        Rectangle spikerect = getspikerect(&spikes[i]);
        if (CheckCollisionRecs(ballrect, spikerect))
        {
            if (*damagetimes <= 0)
            {
                *health -= damage;
                if (*health < 0)
                    *health = 0;
                *damagetimes = 0.7f;
            }
        }
    }
}
void drawspikes(void)
{
    for (int i = 0; i < spike_count; i++)
    {
        spike *s = &spikes[i];
        if (s->type == 0 || s->type == 2)
        {
            DrawTriangle((Vector2){s->x, s->y + s->height}, (Vector2){s->x + s->width, s->y + s->height}, (Vector2){s->x + s->width / 2, s->y}, BLACK);
        }
        else if (s->type == 1 || s->type == 3)
        {
            DrawTriangle((Vector2){s->x, s->y}, (Vector2){s->x + s->width, s->y}, (Vector2){s->x + s->width / 2, s->y + s->height}, BLACK);
        }
    }
}
int main(void)
{
    float height = 760;
    float width = 1600;
    float x = 250;
    float y = 670;
    float brickx = 0;
    float bricky = 700;
    int ground = 700;
    Vector2 ballposition = {x, y};
    Vector2 direction = {0, 0};
    float speed = 350.0f;
    Camera2D camera = {0};
    camera.target = ballposition;
    camera.target.y = ground;
    camera.offset = (Vector2){250, 670};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;
    float ballrotation = 0.0f;
    float vertialvelocity = 0.0f;
    float gravity = 2200.0f;
    float jumppower = -800.0f;
    int jumpcount = 0;
    int jumpmax = 2;
    InitWindow(width, height, "bouncing classic game");
    SetWindowState(FLAG_VSYNC_HINT);
    SetTargetFPS(70);
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        Vector2 previousballposition = ballposition;
        // spike update
        updatespikes(dt);
        if (damagetimes > 0)
        {
            damagetimes -= dt;
        }
        // enter key
        if (IsKeyPressed(KEY_ENTER))
        {
            // main menu game start
            if (!gamestarted && !gameover)
            {
                gamestarted = 1;
                gamepaused = 0;
            }
            // game over restart
            else if (gameover)
            {
                gameover = 0;
                gamestarted = 1;
                gamepaused = 0;
                // reset health
                health = maxhealth;
                damagetimes = 0;
                // reset player
                ballposition.x = 250;
                ballposition.y = 670;
                // reset physics
                vertialvelocity = 0;
                jumpcount = 0;
                // reset score
                maxdistance = 0;
                gamescore = 0;
                // reset camera
                camera.target.x = ballposition.x;
                camera.target.y = ground;
            }
        }
        // pause
        if (IsKeyPressed(KEY_P) && /* ai conditon mane game choltese */ gamestarted && !gameover)
        {
            gamepaused = !gamepaused;
        }
        // player input
        direction.x = 0;
        if (gamestarted && !gamepaused && !gameover)
        {
            if (IsKeyDown(KEY_RIGHT))
            {
                direction.x = 1;
            }
            if (IsKeyDown(KEY_LEFT))
            {
                direction.x = -1;
            }
            // jump
            if (IsKeyPressed(KEY_SPACE) && jumpcount < jumpmax)
            {
                vertialvelocity = jumppower;
                jumpcount++;
            }
        }
        // game update
        if (gamestarted && !gamepaused && !gameover)
        {
            // score
            if (ballposition.x > maxdistance)
            {
                maxdistance = ballposition.x;
                gamescore = (int)(maxdistance / 20.0f);
            }
            // gravity
            vertialvelocity += gravity * dt;
            // movement
            ballposition.x += direction.x * speed * dt;
            ballposition.y += vertialvelocity * dt;
            // platform collision
            checkcollision(&ballposition, previousballposition, &vertialvelocity, &jumpcount);
            if (ballposition.y + radius >= ground)
            {
                ballposition.y = ground - radius;
                vertialvelocity = 0.0f;
                jumpcount = 0;
            }
            // spike collision
            checkspikecollision(&ballposition, &health, &damagetimes);
            // ball rotation
            ballrotation += (direction.x * speed * dt) / radius * 5;
            // camera
            camera.target.x = ballposition.x;
            // game over
            if (health <= 0)
            {
                health = 0;
                gameover = 1;
                gamestarted = 0;
                gamepaused = 0;
                direction.x = 0;
                vertialvelocity = 0;
            }
        }
        // camera rounding
        Camera2D camera_rounded = camera;
        camera_rounded.target.x = roundf(camera.target.x);
        camera_rounded.target.y = roundf(camera.target.y);
        // drawing start
        BeginDrawing();
        BeginMode2D(camera_rounded);
        ClearBackground(RAYWHITE);
        // ground
        for (int j = 0; j < 400; j++)
        {
            float gx = brickx + brickwidth * j;
            float gy = bricky;
            DrawRectangle(gx, gy, brickwidth, brickheight, RED);
            DrawRectangleLines(gx, gy, brickwidth, brickheight, MAROON);
        }
        // ceiling
        for (int j = 0; j < 400; j++)
        {
            float cx = brickwidth * j;
            DrawRectangle(cx, 0, brickwidth, brickheight, RED);
            DrawRectangleLines(cx, 0, brickwidth, brickheight, MAROON);
        }
        // platforms
        for (int p = 0; p < platforms_count; p++)
        {
            for (int i = 0; i < platforms[p].height; i++)
            {
                for (int j = 0; j < platforms[p].length; j++)
                {
                    float dx = platforms[p].x + j * platformbrick_width;
                    float dy = platforms[p].y + i * platformbrick_height;
                    DrawRectangle(dx, dy, platformbrick_width, platformbrick_height, RED);
                    DrawRectangleLines(dx, dy, platformbrick_width, platformbrick_height, MAROON);
                }
            }
        }
        // spikes
        drawspikes();
        // ball
        DrawCircle(ballposition.x, ballposition.y, radius, RED);
        EndMode2D();
        // main menu
        if (!gamestarted && !gameover && !gamepaused) /* intial point a */
        {
            DrawRectangle(0, 0, width, height, BLACK);
            DrawText("bouncing classic", width / 2 - 230, 180, 50, RED);
            DrawText("press enter to start", width / 2 - 150, 300, 25, BLACK);
            DrawText("left / right = move", width / 2 - 125, 360, 20, DARKGRAY);
            DrawText("space = jump", width / 2 - 90, 395, 20, DARKGRAY);
            DrawText("p = pause", width / 2 - 65, 430, 20, DARKGRAY);
        }
        // pause menu
        if (gamepaused && !gameover)
        {
            DrawRectangle(0, 0, width, height, Fade(RAYWHITE, 0.90f));
            DrawText("game paused", width / 2 - 150, height / 2 - 100, 45, BLACK);
            DrawText("press p to resume", width / 2 - 120, height / 2 - 20, 25, DARKGRAY);
            DrawText("enter = continue", width / 2 - 115, height / 2 + 25, 20, DARKGRAY);
        }
        // game over screen
        if (gameover)
        {
            DrawRectangle(0, 0, width, height, Fade(RAYWHITE, 0.92f));
            DrawText("game over", width / 2 - 150, height / 2 - 120, 50, RED);
            DrawText(TextFormat("final score: %d", gamescore), width / 2 - 110, height / 2 - 40, 25, BLACK);
            DrawText("press enter to restart", width / 2 - 145, height / 2 + 30, 22, DARKGRAY);
            DrawText("press esc to exit", width / 2 - 100, height / 2 + 70, 20, DARKGRAY);
        }
        // score
        if (gamestarted || gamepaused || gameover)
        {
            DrawText(TextFormat("score: %d", gamescore), 20, 50, 30, BLACK);
        }
        // health bar
        if (gamestarted || gamepaused || gameover)
        {
            // background
            DrawRectangle(20, 20, 300, 30, DARKGRAY);
            // health
            DrawRectangle(20, 20, (int)(300 * (health / maxhealth)), 30, GREEN);
            // border
            DrawRectangleLines(20, 20, 300, 30, BLACK);
            DrawText(TextFormat("hp: %.0f / %.0f", health, maxhealth), 35, 23, 22, BLACK);
        }
        EndDrawing();
    }
    CloseWindow();
    return 0;
}