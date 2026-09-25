#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define coin_count ((int)(sizeof(coins) / sizeof(coins[0])))
#define enemy_count ((int)(sizeof(enemies) / sizeof(enemies[0])))

// Output Command: gcc main.c -o game.exe $(pkg-config --cflags --libs raylib) && ./game.exe

#define radius 30
#define brickwidth 180
#define brickheight 100
#define platformbrick_width 50
#define platformbrick_height 50
// bounce physics tuning
#define max_bounces 4
#define bounce_restitution 0.65f
#define bounce_extra_power 90.0f
#define bounce_min_velocity 120.0f
// spike hitbox thoda choto rakha hocche (visual triangle theke), tai edge/corner e halka touch e "random"
// mone hoya damage r hobe na - shudhu real vabe spike er gaye lagle e hp kata jabe
#define spike_hit_radius (radius * 0.72f)
// wall e lagle chotto push-back dibo, jate ball wall e atke na thake r
#define wall_knockback 5.0f
// win trigger x position
#define win_x 47294
// player name max length
#define name_max 15

float health = 100.0f;
float maxhealth = 100.0f;
float damage = 20.0f;
float damagetimes = 0;
int gameover = 0;
int gamestarted = 0;
int gamepaused = 0;
int gamewon = 0;

// game score
int gamescore = 0;
float maxdistance = 0;

// >>>>>>> NOTUN FEATURE: powerup timer, combo, screen shake, mute, checkpoint globals >>>>>>>
float shieldTimer = 0.0f;
float magnetTimer = 0.0f;
float speedTimer = 0.0f;
int comboCount = 0;
float comboTimer = 0.0f;
float shakeTime = 0.0f;
float shakeMagnitude = 0.0f;
int muted = 0;
float lastCheckpointX = 250.0f;
#define shield_duration 6.0f
#define magnet_duration 8.0f
#define speed_duration 6.0f
#define magnet_radius 220.0f
#define combo_window 2.0f
#define combo_max_mult 5
// <<<<<<<

// >>>>>>> player name + leaderboard (top 5) >>>>>>>
char playername[name_max + 1] = "";
int namelen = 0;
int highscore = 0;
char highname[name_max + 1] = "none";
int newrecord = 0;

#define leaderboard_size 5
typedef struct
{
    int score;
    char name[name_max + 1];
} LBEntry;
LBEntry leaderboard[leaderboard_size];

void LoadLeaderboard(void)
{
    for (int i = 0; i < leaderboard_size; i++)
    {
        leaderboard[i].score = 0;
        strcpy(leaderboard[i].name, "---");
    }
    FILE *f = fopen("leaderboard.txt", "r");
    if (f)
    {
        for (int i = 0; i < leaderboard_size; i++)
        {
            char line[64];
            if (!fgets(line, sizeof(line), f))
                break;
            leaderboard[i].score = atoi(line);
            if (fgets(line, sizeof(line), f))
            {
                line[strcspn(line, "\r\n")] = '\0';
                strncpy(leaderboard[i].name, line, name_max);
                leaderboard[i].name[name_max] = '\0';
            }
        }
        fclose(f);
    }
    highscore = leaderboard[0].score;
    strncpy(highname, leaderboard[0].name, name_max);
    highname[name_max] = '\0';
}

void SaveLeaderboard(void)
{
    FILE *f = fopen("leaderboard.txt", "w");
    if (!f)
        return;
    for (int i = 0; i < leaderboard_size; i++)
        fprintf(f, "%d\n%s\n", leaderboard[i].score, leaderboard[i].name);
    fclose(f);
}

// score leaderboard e jogyo hole insert kore, rank return kore (0 = notun best), na hole -1
int TrySubmitScore(int score, const char *name)
{
    for (int i = 0; i < leaderboard_size; i++)
    {
        if (score > leaderboard[i].score)
        {
            for (int j = leaderboard_size - 1; j > i; j--)
                leaderboard[j] = leaderboard[j - 1];
            leaderboard[i].score = score;
            strncpy(leaderboard[i].name, name, name_max);
            leaderboard[i].name[name_max] = '\0';
            SaveLeaderboard();
            highscore = leaderboard[0].score;
            strncpy(highname, leaderboard[0].name, name_max);
            highname[name_max] = '\0';
            return i;
        }
    }
    return -1;
}

// game shesh hole (game over / win) - leaderboard e check kore save kore
void FinishRun(void)
{
    int rank = TrySubmitScore(gamescore, playername);
    if (rank == 0)
    {
        newrecord = 1;
    }
}

// main menu te name type korar function
void UpdateNameInput(void)
{
    int key = GetCharPressed();
    while (key > 0)
    {
        if (key >= 32 && key <= 125 && namelen < name_max)
        {
            if (!(key == 32 && namelen == 0)) // shurute space dewa jabe na
            {
                playername[namelen++] = (char)key;
                playername[namelen] = '\0';
            }
        }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && namelen > 0)
    {
        playername[--namelen] = '\0';
    }
}

void DrawTextCentered(const char *text, int y, int size, Color c)
{
    int w = MeasureText(text, size);
    DrawText(text, (GetScreenWidth() - w) / 2, y, size, c);
}
// <<<<<<< player name + leaderboard <<<<<<<

// >>>>>>> NOTUN FEATURE: particle effect system (coin sparkle, damage burst, dust trail) >>>>>>>
#define max_particles 220
typedef struct
{
    Vector2 pos;
    Vector2 vel;
    float life;
    float maxlife;
    float size;
    Color color;
    int active;
} Particle;
Particle particles[max_particles];

void SpawnParticle(Vector2 pos, Vector2 vel, float life, float size, Color color)
{
    for (int i = 0; i < max_particles; i++)
    {
        if (!particles[i].active)
        {
            particles[i].pos = pos;
            particles[i].vel = vel;
            particles[i].life = life;
            particles[i].maxlife = life;
            particles[i].size = size;
            particles[i].color = color;
            particles[i].active = 1;
            return;
        }
    }
}

void SpawnBurst(Vector2 pos, int count, Color color, float speed, float life, float size)
{
    for (int i = 0; i < count; i++)
    {
        float ang = ((float)GetRandomValue(0, 359)) * DEG2RAD;
        float spd = speed * (0.4f + (float)GetRandomValue(0, 100) / 100.0f);
        Vector2 vel = {cosf(ang) * spd, sinf(ang) * spd};
        SpawnParticle(pos, vel, life, size, color);
    }
}

void UpdateParticles(float dt)
{
    for (int i = 0; i < max_particles; i++)
    {
        if (!particles[i].active)
            continue;
        particles[i].life -= dt;
        if (particles[i].life <= 0)
        {
            particles[i].active = 0;
            continue;
        }
        particles[i].vel.y += 350.0f * dt;
        particles[i].pos.x += particles[i].vel.x * dt;
        particles[i].pos.y += particles[i].vel.y * dt;
    }
}

void DrawParticles(void)
{
    for (int i = 0; i < max_particles; i++)
    {
        if (!particles[i].active)
            continue;
        float t = particles[i].life / particles[i].maxlife;
        Color c = particles[i].color;
        c.a = (unsigned char)(255 * t);
        DrawCircleV(particles[i].pos, particles[i].size * t, c);
    }
}

void ResetParticles(void)
{
    for (int i = 0; i < max_particles; i++)
        particles[i].active = 0;
}
// <<<<<<<

// >>>>>>> NOTUN FEATURE: on-screen achievement / event notifications >>>>>>>
#define max_notifications 5
typedef struct
{
    char text[64];
    float timer;
} Notification;
Notification notifications[max_notifications];

void PushNotification(const char *text)
{
    for (int i = 0; i < max_notifications; i++)
    {
        if (notifications[i].timer <= 0)
        {
            strncpy(notifications[i].text, text, 63);
            notifications[i].text[63] = '\0';
            notifications[i].timer = 2.4f;
            return;
        }
    }
}

void UpdateNotifications(float dt)
{
    for (int i = 0; i < max_notifications; i++)
        if (notifications[i].timer > 0)
            notifications[i].timer -= dt;
}

void DrawNotifications(int width)
{
    int y = 165;
    for (int i = 0; i < max_notifications; i++)
    {
        if (notifications[i].timer > 0)
        {
            float t = notifications[i].timer;
            float a = (t < 1.0f) ? t : 1.0f;
            int w = MeasureText(notifications[i].text, 24);
            DrawRectangle((width - w) / 2 - 14, y - 6, w + 28, 34, Fade(BLACK, 0.55f * a));
            DrawText(notifications[i].text, (width - w) / 2, y, 24, Fade(GOLD, a));
            y += 40;
        }
    }
}
// <<<<<<<

// >>>>>>> NOTUN FEATURE: screen shake (damage feedback) >>>>>>>
void TriggerShake(float duration, float magnitude)
{
    shakeTime = duration;
    shakeMagnitude = magnitude;
}
// <<<<<<<

// >>>>>>> Sound variables (Global) >>>>>>>
Music bgMusic;
Sound jumpSound;
Sound spikeSound;
Sound gameOverSound;
Music gameOverBgSound;
Sound restartSound;
// notun sound effects - coin, diamond, bomb drop, bomb hit
Sound coinSound;     // coin collect - anikcoin2.wav
Sound diamondSound;  // diamond collect - anikcoin.wav
Sound bombDropSound; // bomb drop hoyar somoy - anikmetal.wav
Sound bombHitSound;  // bomb e/theke lagle - anikbomb.wav
// <<<<<<<

// >>>>>>>platfroms stucrure>>>>>
typedef struct platform
{
    float x;
    float y;
    int length;
    int height;
} platform;
platform platforms[] = {
    {-100, 0, 5, 16},
    {0, 0, 8, 8},
    {600, 0, 20, 5},
    {1000, 550, 3, 3},
    {1700, 550, 3, 3},
    {1800, 550, 10, 3},
    {2450, 300, 26, 3},
    {4000, 300, 40, 2},
    {4500, 550, 4, 2},
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
    {8300, 350, 2, 8},
    {8400, 350, 10, 8},
    {9100, 0, 2, 7},
    {9400, 360, 2, 8},
    {9650, 500, 2, 4},
    {9800, 0, 2, 6},
    {10100, 0, 2, 8},
    {11500, 0, 3, 8},
    {12000, 0, 3, 8},
    {12000, 550, 8, 3},
    {12500, 0, 2, 2},
    {13000, 550, 2, 7},
    {13500, 0, 4, 8},
    {14000, 550, 2, 7},
    {14500, 0, 2, 9},
    {15000, 550, 2, 4},
    {1500, 0, 2, 4},
    {15500, 0, 3, 9},
    {16000, 550, 3, 10},
    {16500, 0, 2, 6},
    {16500, 400, 2, 6},
    {17500, 550, 2, 4},
    {17500, 0, 2, 7},
    {17800, 450, 2, 8},
    {17800, 0, 2, 5},
    {18500, 0, 2, 10},
    {19000, 400, 2, 7},
    {19500, 0, 2, 8},
    {20000, 550, 20, 4},
    {20500, 0, 2, 5},
    {20500, 350, 2, 6},
    {20952, 550, 3, 3},
    {21474, 0, 3, 7},
    {21761, 550, 3, 3},
    {22480, 0, 3, 8},
    {23271, 550, 3, 5},
    {23271, 0, 3, 6},
    {24056, 0, 5, 6},
    {24802, 550, 3, 4},
    {24802, 0, 3, 7},
    {25444, 0, 3, 6},
    {25690, 350, 2, 8},
    {26141, 0, 3, 6},
    {26884, 0, 3, 12},
    {27180, 400, 2, 7},
    {27180, 0, 2, 5},
    {27685, 0, 3, 7},
    {27958, 550, 3, 3},
    {28381, 0, 3, 6},
    {28998, 550, 5, 3},
    {29479, 0, 3, 7},
    {29840, 300, 3, 9},
    {30072, 0, 5, 10},
    {30729, 0, 3, 7},
    {31142, 550, 5, 3},
    {31582, 0, 4, 6},
    {32187, 0, 6, 10},
    {32722, 550, 4, 7},
    {33371, 550, 3, 3},
    {33893, 550, 2, 6},
    {34432, 0, 3, 5},
    {34432, 500, 3, 5},
    {35151, 0, 3, 7},
    {35151, 450, 3, 6},
    {35836, 200, 3, 10},
    {36512, 0, 3, 7},
    {36763, 550, 4, 3},
    {37154, 550, 2, 7},
    {37729, 0, 4, 8},
    {38017, 550, 2, 3},
    {38515, 0, 3, 4},
    {38515, 432, 3, 8},
    {39059, 550, 35, 4},
    {39634, 0, 3, 7},
    {40325, 200, 2, 10},
    {41016, 0, 3, 10},
    {41746, 0, 3, 8},
    {42537, 400, 3, 6},
    {43173, 0, 3, 4},
    {43459, 300, 3, 8},
    {44221, 0, 5, 3},
    {44949, 250, 3, 11},
    {45692, 0, 5, 5},
    {46052, 0, 4, 8},
    {46431, 550, 2, 4},
    {47144, 0, 3, 5},
    {47144, 500, 3, 6},
    {47936, 0, 2, 15}

};

//>>> COIN ADDING

typedef struct
{
    float x;
    float y;
    float coinradius;
    int active; //
} Coin;

Coin coins[] = {
    {300, 650, 15, 1},
    {340, 650, 15, 1},
    {380, 650, 15, 1},
    {700, 650, 15, 1},
    {1100, 500, 15, 1},
    {1140, 500, 15, 1},
    {1180, 500, 15, 1},
    {1220, 500, 15, 1},
    {1800, 500, 15, 1},
    {1840, 480, 15, 1},
    {1880, 460, 15, 1},
    {1920, 480, 15, 1},
    {1960, 500, 15, 1},
    {2200, 400, 15, 1},
    {2600, 250, 15, 1},
    {2640, 250, 15, 1},
    {2680, 250, 15, 1},
    {3200, 550, 15, 1},
    {3240, 550, 15, 1},
    {3280, 550, 15, 1},
    {3320, 550, 15, 1},
    {3700, 400, 15, 1},
    {4200, 250, 15, 1},
    {4240, 250, 15, 1},
    {4280, 250, 15, 1},
    {4320, 250, 15, 1},
    {4360, 250, 15, 1},
    {4900, 450, 15, 1},
    {4940, 450, 15, 1},
    {4980, 450, 15, 1},
    {5300, 500, 15, 1},
    {5800, 250, 15, 1},
    {5840, 250, 15, 1},
    {5880, 250, 15, 1},
    {5920, 250, 15, 1},
    {6500, 350, 15, 1},
    {6540, 350, 15, 1},
    {6580, 350, 15, 1},
    {6620, 350, 15, 1},
    {6660, 350, 15, 1},
    {7200, 500, 15, 1},
    {8400, 300, 15, 1},
    {8440, 300, 15, 1},
    {8480, 300, 15, 1},
    {9500, 400, 15, 1},
    {9540, 400, 15, 1},
    {9580, 400, 15, 1},
    {9620, 400, 15, 1},
    {10500, 550, 15, 1},
    {12000, 500, 15, 1},
    {12040, 500, 15, 1},
    {12080, 500, 15, 1},
    {12120, 500, 15, 1},
    {12160, 500, 15, 1},
    {13200, 450, 15, 1},
    {13240, 450, 15, 1},
    {13280, 450, 15, 1},
    {14200, 350, 15, 1},
    {16000, 500, 15, 1},
    {16040, 500, 15, 1},
    {16080, 500, 15, 1},
    {16120, 500, 15, 1},
    {18000, 350, 15, 1},
    {18040, 350, 15, 1},
    {18080, 350, 15, 1},
    {18120, 350, 15, 1},
    {18160, 350, 15, 1},
    {19500, 450, 15, 1},
    {21000, 500, 15, 1},
    {21040, 500, 15, 1},
    {21080, 500, 15, 1},
    {23000, 450, 15, 1},
    {23040, 450, 15, 1},
    {23080, 450, 15, 1},
    {23120, 450, 15, 1},
    {24500, 300, 15, 1},
    {25000, 250, 15, 1},
    {25040, 250, 15, 1},
    {25080, 250, 15, 1},
    {25120, 250, 15, 1},
    {25160, 250, 15, 1},
    {27000, 350, 15, 1},
    {27070, 350, 15, 1},
    {27100, 350, 15, 1},
    {28500, 500, 15, 1},
    {31000, 500, 15, 1},
    {31040, 500, 15, 1},
    {31080, 500, 15, 1},
    {31120, 500, 15, 1},
    {35000, 350, 15, 1},
    {35040, 350, 15, 1},
    {35080, 350, 15, 1},
    {35120, 350, 15, 1},
    {36178 + 40, 300, 15, 1},
    {36178 + 40, 300, 15, 1},
    {36178 + 40, 300, 15, 1},
    {36178 + 40, 300, 15, 1},
    {37500, 400, 15, 1},
    {39000, 500, 15, 1},
    {39040, 500, 15, 1},
    {39080, 500, 15, 1},
    {40000, 150, 15, 1},
    {40040, 150, 15, 1},
    {40080, 150, 15, 1},
    {40120, 150, 15, 1},
    {40160, 150, 15, 1},
    {42000, 300, 15, 1},
    {44000, 200, 15, 1},
    {44040, 200, 15, 1},
    {44080, 200, 15, 1},
    {44120, 200, 15, 1},
    {45000, 400, 15, 1},
    {45040, 400, 15, 1},
    {45080, 400, 15, 1},
    {45120, 400, 15, 1},
    {45160, 400, 15, 1},
    {46500, 500, 15, 1},
    {46540, 500, 15, 1},
    {46580, 500, 15, 1},
    {47100, 450, 15, 1}};

void drawcoins(void)
{
    for (int i = 0; i < coin_count; i++)
    {
        if (coins[i].active)
        {

            DrawCircle((int)coins[i].x, (int)coins[i].y, coins[i].coinradius, GOLD);
            DrawCircleLines((int)coins[i].x, (int)coins[i].y, coins[i].coinradius, ORANGE);
            DrawCircle((int)coins[i].x, (int)coins[i].y, coins[i].coinradius * 0.4f, YELLOW);
        }
    }
}

void checkcoincollision(Vector2 ballposition, int *score)
{
    for (int i = 0; i < coin_count; i++)
    {
        if (!coins[i].active)
            continue;

        float dx = ballposition.x - coins[i].x;
        float dy = ballposition.y - coins[i].y;
        float rSum = radius + coins[i].coinradius;

        if (dx * dx + dy * dy <= rSum * rSum)
        {
            coins[i].active = 0;
            // >>>>>>> NOTUN: combo system - dhaka dhaka tule score multiplier bare >>>>>>>
            comboCount++;
            comboTimer = combo_window;
            int mult = 1 + (comboCount / 5);
            if (mult > combo_max_mult)
                mult = combo_max_mult;
            *score += 200 * mult;
            if (comboCount % 5 == 0)
                PushNotification(TextFormat("COMBO x%d!", mult));
            // <<<<<<<
            PlaySound(coinSound);
            SpawnBurst((Vector2){coins[i].x, coins[i].y}, 10, GOLD, 120.0f, 0.4f, 4.0f);
        }
    }
}

void resetcoins(void)
{
    for (int i = 0; i < coin_count; i++)
    {
        coins[i].active = 1;
    }
}

// FINISHED ADDING COINS

// ADDING DIAMOND

typedef struct
{
    float x;
    float y;
    float size;
    int active;
} Diamond;

Diamond diamonds[] = {
    {800, 420, 30, 1},
    {2200, 320, 30, 1},
    {4500, 350, 30, 1},
    {7000, 280, 30, 1},
    {9800, 320, 30, 1},
    {12200, 350, 30, 1},
    {14300, 280, 30, 1},
    {16200, 220, 30, 1},
    {18200, 320, 30, 1},
    {19800, 280, 30, 1},
    {21100, 300, 30, 1},
    {23200, 220, 30, 1},
    {24400, 320, 30, 1},
    {26000, 280, 30, 1},
    {27500, 220, 30, 1},
    {28800, 350, 30, 1},
    {30000, 180, 30, 1},
    {31200, 320, 30, 1},
    {32400, 280, 30, 1},
    {33500, 220, 30, 1},
    {34600, 320, 30, 1},
    {35600, 180, 30, 1},
    {36300, 280, 30, 1},
    {37300, 220, 30, 1},
    {38300, 320, 30, 1},
    {39100, 280, 30, 1},
    {39900, 220, 30, 1},
    {40500, 180, 30, 1},
    {41100, 320, 30, 1},
    {41700, 280, 30, 1},
    {42300, 220, 30, 1},
    {42900, 320, 30, 1},
    {43500, 180, 30, 1},
    {44100, 300, 30, 1},
    {44700, 280, 30, 1},
    {45300, 200, 30, 1},
    {45800, 320, 30, 1},
    {46300, 220, 30, 1},
    {46700, 280, 30, 1},
    {47100, 320, 30, 1}};
#define diamond_count ((int)(sizeof(diamonds) / sizeof(diamonds[0])))

void drawdiamonds(void)
{
    for (int i = 0; i < diamond_count; i++)
    {
        if (diamonds[i].active)
        {
            float x = diamonds[i].x;
            float y = diamonds[i].y;
            float s = diamonds[i].size;

            Vector2 pTopL = {x - s * 0.5f, y - s * 0.6f};
            Vector2 pTopR = {x + s * 0.5f, y - s * 0.6f};
            Vector2 pMidL = {x - s * 0.9f, y - s * 0.1f};
            Vector2 pMidR = {x + s * 0.9f, y - s * 0.1f};
            Vector2 pBot = {x, y + s * 0.8f};
            Vector2 pCenter = {x, y - s * 0.1f};

            Color cTop = (Color){220, 245, 255, 255};
            Color cLeft = (Color){120, 210, 255, 255};
            Color cRight = (Color){0, 170, 240, 255};
            Color cBotLeft = (Color){0, 130, 215, 255};
            Color cBotRight = (Color){0, 90, 185, 255};

            DrawTriangle(pTopL, pMidL, pCenter, cLeft);
            DrawTriangle(pTopL, pCenter, pTopR, cTop);
            DrawTriangle(pTopR, pCenter, pMidR, cRight);
            DrawTriangle(pMidL, pBot, pCenter, cBotLeft);
            DrawTriangle(pCenter, pBot, pMidR, cBotRight);

            DrawLineEx(pTopL, pTopR, 2.0f, WHITE);
            DrawLineEx(pTopL, pMidL, 1.5f, BLUE);
            DrawLineEx(pTopR, pMidR, 1.5f, BLUE);
            DrawLineEx(pMidL, pBot, 1.5f, DARKBLUE);
            DrawLineEx(pMidR, pBot, 1.5f, DARKBLUE);
            DrawLineEx(pTopL, pCenter, 1.0f, WHITE);
            DrawLineEx(pTopR, pCenter, 1.0f, WHITE);
            DrawLineEx(pBot, pCenter, 1.0f, DARKBLUE);
        }
    }
}

void checkdiamondcollision(Vector2 ballposition, int *score)
{
    for (int i = 0; i < diamond_count; i++)
    {
        if (!diamonds[i].active)
            continue;

        float dx = ballposition.x - diamonds[i].x;
        float dy = ballposition.y - diamonds[i].y;
        float rSum = radius + diamonds[i].size;

        if (dx * dx + dy * dy <= rSum * rSum)
        {
            diamonds[i].active = 0;
            // combo system diamond e o kaj kore
            comboCount++;
            comboTimer = combo_window;
            int mult = 1 + (comboCount / 5);
            if (mult > combo_max_mult)
                mult = combo_max_mult;
            *score += 500 * mult;
            if (comboCount % 5 == 0)
                PushNotification(TextFormat("COMBO x%d!", mult));

            PlaySound(diamondSound);
            SpawnBurst((Vector2){diamonds[i].x, diamonds[i].y}, 16, SKYBLUE, 150.0f, 0.5f, 5.0f);
        }
    }
}

void resetdiamonds(void)
{
    for (int i = 0; i < diamond_count; i++)
    {
        diamonds[i].active = 1;
    }
}
// FINISHED COID CODE

// platform count ekhon auto - notun platform add korle count change korte hobe na
#define platforms_count ((int)(sizeof(platforms) / sizeof(platforms[0])))
// finished of creating platfrom>>>>>

// >>>>>>> NOTUN FEATURE: power-up system (Shield / Magnet / Speed boost) >>>>>>>
typedef enum
{
    POWER_SHIELD = 0,
    POWER_MAGNET = 1,
    POWER_SPEED = 2
} PowerType;

typedef struct
{
    float x;
    float y;
    float r;
    int type;
    int active;
} PowerUp;

// {x, y, r, type, active} - level jure chorano
PowerUp powerups[] = {
    {1500, 450, 20, POWER_SHIELD, 1},
    {3000, 500, 20, POWER_MAGNET, 1},
    {5000, 450, 20, POWER_SPEED, 1},
    {7200, 400, 20, POWER_SHIELD, 1},
    {9500, 300, 20, POWER_MAGNET, 1},
    {11500, 450, 20, POWER_SPEED, 1},
    {13800, 300, 20, POWER_SHIELD, 1},
    {16200, 400, 20, POWER_MAGNET, 1},
    {18500, 300, 20, POWER_SPEED, 1},
    {21000, 400, 20, POWER_SHIELD, 1},
    {23500, 350, 20, POWER_MAGNET, 1},
    {26000, 250, 20, POWER_SPEED, 1},
    {28800, 400, 20, POWER_SHIELD, 1},
    {31200, 400, 20, POWER_MAGNET, 1},
    {33800, 450, 20, POWER_SPEED, 1},
    {36500, 250, 20, POWER_SHIELD, 1},
    {39200, 450, 20, POWER_MAGNET, 1},
    {41800, 300, 20, POWER_SPEED, 1},
    {44300, 200, 20, POWER_SHIELD, 1},
    {46200, 350, 20, POWER_MAGNET, 1}};
#define powerup_count ((int)(sizeof(powerups) / sizeof(powerups[0])))

void drawpowerups(void)
{
    for (int i = 0; i < powerup_count; i++)
    {
        if (!powerups[i].active)
            continue;
        float x = powerups[i].x;
        float y = powerups[i].y;
        float r = powerups[i].r;
        float bob = sinf(GetTime() * 3.0f + i) * 4.0f; // halka bhasha (floating) effect
        y += bob;

        if (powerups[i].type == POWER_SHIELD)
        {
            DrawCircle((int)x, (int)y, r, (Color){80, 180, 255, 255});
            DrawCircleLines((int)x, (int)y, r, (Color){20, 90, 200, 255});
            Vector2 sTop = {x, y - r * 0.6f};
            Vector2 sL = {x - r * 0.55f, y - r * 0.1f};
            Vector2 sR = {x + r * 0.55f, y - r * 0.1f};
            Vector2 sBot = {x, y + r * 0.65f};
            DrawTriangle(sTop, sL, sBot, WHITE);
            DrawTriangle(sTop, sBot, sR, WHITE);
        }
        else if (powerups[i].type == POWER_MAGNET)
        {
            DrawCircle((int)x, (int)y, r, (Color){230, 60, 60, 255});
            DrawCircleLines((int)x, (int)y, r, MAROON);
            DrawRectangle((int)(x - r * 0.35f), (int)(y - r * 0.5f), (int)(r * 0.28f), (int)(r * 0.9f), WHITE);
            DrawRectangle((int)(x + r * 0.07f), (int)(y - r * 0.5f), (int)(r * 0.28f), (int)(r * 0.9f), WHITE);
            DrawRectangle((int)(x - r * 0.35f), (int)(y + r * 0.15f), (int)(r * 0.7f), (int)(r * 0.25f), WHITE);
        }
        else
        {
            DrawCircle((int)x, (int)y, r, (Color){255, 220, 60, 255});
            DrawCircleLines((int)x, (int)y, r, ORANGE);
            Vector2 b1 = {x + r * 0.15f, y - r * 0.55f};
            Vector2 b2 = {x - r * 0.25f, y + r * 0.05f};
            Vector2 b3 = {x + r * 0.05f, y + r * 0.05f};
            Vector2 b4 = {x - r * 0.15f, y + r * 0.55f};
            DrawTriangle(b1, b2, b3, WHITE);
            DrawLineEx(b3, b4, 4.0f, WHITE);
        }
    }
}

void checkpowerupcollision(Vector2 ballposition)
{
    for (int i = 0; i < powerup_count; i++)
    {
        if (!powerups[i].active)
            continue;
        float dx = ballposition.x - powerups[i].x;
        float dy = ballposition.y - powerups[i].y;
        float rSum = radius + powerups[i].r;
        if (dx * dx + dy * dy <= rSum * rSum)
        {
            powerups[i].active = 0;
            Color burstColor = (powerups[i].type == POWER_SHIELD) ? (Color){80, 180, 255, 255} : (powerups[i].type == POWER_MAGNET ? (Color){230, 60, 60, 255} : (Color){255, 220, 60, 255});
            SpawnBurst((Vector2){powerups[i].x, powerups[i].y}, 20, burstColor, 180.0f, 0.6f, 5.0f);
            // NOTUN: powerup tule o coin er moto sound (anikcoin2.wav) bajbe
            PlaySound(coinSound);
            if (powerups[i].type == POWER_SHIELD)
            {
                shieldTimer = shield_duration;
                PushNotification("SHIELD ACTIVATED!");
            }
            else if (powerups[i].type == POWER_MAGNET)
            {
                magnetTimer = magnet_duration;
                PushNotification("MAGNET ACTIVATED!");
            }
            else
            {
                speedTimer = speed_duration;
                PushNotification("SPEED BOOST!");
            }
        }
    }
}

// magnet active thakle asheypasher coin/diamond ball er dike tene ane
void applymagnet(Vector2 ballposition, float dt)
{
    if (magnetTimer <= 0)
        return;
    for (int i = 0; i < coin_count; i++)
    {
        if (!coins[i].active)
            continue;
        float dx = ballposition.x - coins[i].x;
        float dy = ballposition.y - coins[i].y;
        float dist2 = dx * dx + dy * dy;
        if (dist2 < magnet_radius * magnet_radius && dist2 > 1.0f)
        {
            float dist = sqrtf(dist2);
            float pull = 650.0f * dt;
            coins[i].x += (dx / dist) * pull;
            coins[i].y += (dy / dist) * pull;
        }
    }
    for (int i = 0; i < diamond_count; i++)
    {
        if (!diamonds[i].active)
            continue;
        float dx = ballposition.x - diamonds[i].x;
        float dy = ballposition.y - diamonds[i].y;
        float dist2 = dx * dx + dy * dy;
        if (dist2 < magnet_radius * magnet_radius && dist2 > 1.0f)
        {
            float dist = sqrtf(dist2);
            float pull = 650.0f * dt;
            diamonds[i].x += (dx / dist) * pull;
            diamonds[i].y += (dy / dist) * pull;
        }
    }
}

void resetpowerups(void)
{
    for (int i = 0; i < powerup_count; i++)
        powerups[i].active = 1;
    shieldTimer = 0;
    magnetTimer = 0;
    speedTimer = 0;
}
// <<<<<<< power-up system shesh <<<<<<<

// Background color function>>>>>>
void Back_Ground_Color(float distance, Color *skytop, Color *skybottom)
{
    if (distance < 10000)
    {
        // din
        *skytop = (Color){116, 255, 213, 255};
        *skybottom = (Color){135, 206, 235, 255};
    }
    else if (distance < 20000)
    {
        // din theke sondha
        float t = (distance - 10000) / 10000.0f;
        *skytop = (Color){
            (unsigned char)(135 + t * (255 - 135)),
            (unsigned char)(206 + t * (140 - 206)),
            (unsigned char)(250 + t * (60 - 250)), 255};
        *skybottom = (Color){
            255,
            (unsigned char)(255 + t * (200 - 255)),
            (unsigned char)(255 + t * (120 - 255)), 255};
    }
    else if (distance < 35000)
    {
        // sondha theke raat
        float t = (distance - 20000) / 15000.0f;
        if (t > 1.0f)
            t = 1.0f;
        *skytop = (Color){
            (unsigned char)(255 + t * (40 - 255)),
            (unsigned char)(140 + t * (20 - 140)),
            (unsigned char)(60 + t * (80 - 60)), 255};
        *skybottom = (Color){
            (unsigned char)(255 + t * (120 - 255)),
            (unsigned char)(200 + t * (60 - 200)),
            (unsigned char)(120 + t * (100 - 120)), 255};
    }
    else
    {
        // raat
        *skytop = (Color){15, 32, 39, 255};
        *skybottom = (Color){120, 60, 100, 255};
    }
}
/// finished of background color function>>>>>>

// >>>>>>> NOTUN FEATURE: parallax background - dure pahar + megh, alada gotite move kore (depth) >>>>>>>
void DrawParallaxBackground(float camX, float width, float height)
{
    // durer pahar (far mountains) - khub dhire move kore
    float parallax1 = 0.12f;
    Color mountainColor = Fade(DARKGRAY, 0.30f);
    float offset1 = fmodf(camX * parallax1, 420.0f);
    for (int i = -1; i < (int)(width / 420.0f) + 3; i++)
    {
        float baseX = i * 420 - offset1;
        float baseY = 640;
        DrawTriangle((Vector2){baseX, baseY}, (Vector2){baseX + 210, baseY - 190}, (Vector2){baseX + 420, baseY}, mountainColor);
    }
    // megh (clouds) - moddhom gotite move kore
    float parallax2 = 0.28f;
    float offset2 = fmodf(camX * parallax2, 320.0f);
    for (int i = -1; i < (int)(width / 320.0f) + 3; i++)
    {
        float baseX = i * 320 - offset2;
        float baseY = 120 + 35 * sinf((float)i * 1.7f);
        Color cc = Fade(WHITE, 0.75f);
        DrawEllipse((int)baseX, (int)baseY, 45, 22, cc);
        DrawEllipse((int)(baseX + 30), (int)(baseY - 10), 34, 18, cc);
        DrawEllipse((int)(baseX - 26), (int)(baseY - 6), 30, 16, cc);
    }
}
// <<<<<<<

// platfrom ar sate ball ar collision
void checkcollision(Vector2 *ballposition, Vector2 previousposition, float *verticalvelocity, int *jumpcount, int *bouncesremaining)
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
                // landing on top of a platform - bounce a few times, then stop
                float incoming = fabsf(*verticalvelocity);
                if (incoming > bounce_min_velocity && *bouncesremaining > 0)
                {
                    *verticalvelocity = -(incoming * bounce_restitution + bounce_extra_power);
                    (*bouncesremaining)--;
                    PlaySound(jumpSound); // platform e bounce korle sound
                }
                else
                {
                    *verticalvelocity = 0.0f;
                    *bouncesremaining = max_bounces;
                }
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
                ballposition->x -= wall_knockback;
            }
            else if (prevballleft >= right)
            {
                ballposition->x = right + radius;
                ballposition->x += wall_knockback;
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
spike spikes[] = {
    {800, 655, 45, 45, 150, 655, 500, 1, 0},
    {1450, 655, 45, 45, 100, 655, 400, -1, 0},
    {1900, 405, 45, 45, 100, 550, 700, 1, 2},
    {2100, 405, 45, 45, 100, 550, 300, 1, 2},
    // {2350, 655, 45, 45, 120, 655, 170, 1, 0},
    // {2700, 255, 45, 45, 50, 255, 160, -1, 2},
    //{3000, 450, 45, 45, 100, 450, 160, -1, 3},
    {3850, 655, 45, 45, 100, 655, 700, -1, 0},
    {4200, 255, 45, 45, 50, 255, 500, 1, 2},
    {4400, 655, 45, 45, 150, 655, 170, 1, 0},
    // {4550, 450, 45, 45, 100, 450, 150, 1, 3},
    {4800, 655, 45, 45, 100, 655, 7000, -1, 0},
    {5250, 655, 45, 45, 120, 655, 160, 1, 0},
    {5950, 255, 45, 45, 50, 255, 900, -1, 2},
    // {6000, 450, 45, 45, 100, 450, 160, -1, 3},
    {6100, 655, 45, 45, 100, 655, 1000, -1, 0},
    {6400, 455, 45, 45, 150, 455, 150, 1, 2},
    {6700, 255, 45, 45, 50, 255, 400, -1, 2},
    {8050, 100, 45, 45, 0, 300, 160, 1, 1},
    {8300, 305, 45, 45, 50, 305, 680, 1, 2},
    //{9000, 100, 45, 45, 0, 300, 170, -1, 1},
    {9300, 100, 45, 45, 0, 300, 160, 1, 1},
    {10200, 100, 45, 45, 0, 300, 600, -1, 1},
    {10800, 655, 45, 45, 150, 655, 700, 1, 0},
    {11200, 655, 45, 45, 120, 655, 160, -1, 0},
    {11900, 100, 45, 45, 0, 300, 160, 1, 1},
    {12300, 500, 45, 45, 100, 500, 900, 1, 2},
    {12900, 100, 45, 45, 0, 300, 170, -1, 1},
    {13400, 100, 45, 45, 0, 300, 1000, 1, 1},
    {13900, 500, 45, 45, 70, 660, 1000, -1, 2},
    {14200, 655, 45, 45, 150, 655, 1000, 1, 0},
    {14800, 655, 45, 45, 100, 655, 1200, -1, 0},
    {15300, 100, 45, 45, 0, 300, 160, 1, 1},
    {15800, 655, 45, 45, 120, 655, 1500, -1, 0},
    {16800, 655, 45, 45, 150, 655, 180, -1, 0},
    {17300, 100, 45, 45, 0, 300, 160, 1, 1},
    {17800, 655, 45, 45, 100, 655, 1070, -1, 0},
    {18300, 100, 45, 45, 0, 300, 180, 1, 1},
    {18800, 655, 45, 45, 150, 655, 500, -1, 0},
    {19300, 100, 45, 45, 0, 300, 160, 1, 1},
    {19800, 655, 45, 45, 100, 655, 1200, -1, 0},
    {20300, 100, 45, 45, 0, 300, 1000, 1, 1},
    {20800, 500, 45, 45, 150, 500, 1110, -1, 2},
    {21200, 655, 45, 45, 100, 655, 1000, 1, 0},
    {21800, 500, 45, 45, 150, 500, 1000, -1, 2},
    {22600, 655, 45, 45, 100, 655, 190, 1, 0},
    {23000, 100, 45, 45, 0, 300, 180, -1, 1},
    {23600, 500, 45, 45, 150, 500, 160, 1, 2},
    {24300, 655, 45, 45, 100, 655, 200, -1, 0},
    {25000, 500, 45, 45, 150, 500, 170, 1, 2},
    {25800, 655, 45, 45, 100, 655, 400, -1, 0},
    {26500, 100, 45, 45, 0, 300, 190, 1, 1},
    {27400, 500, 45, 45, 150, 500, 160, -1, 2},
    {28170, 655, 45, 45, 100, 655, 300, 1, 0},
    {28700, 100, 45, 45, 0, 300, 170, -1, 1},
    {29200, 500, 45, 45, 150, 500, 200, 1, 2},
    {30900, 100, 45, 45, 0, 300, 190, 1, 1},
    {31400, 500, 45, 45, 150, 500, 160, -1, 2},
    {32000, 655, 45, 45, 100, 655, 200, 1, 0},
    {32500, 100, 45, 45, 0, 300, 180, -1, 1},
    {33100, 500, 45, 45, 150, 660, 200, 1, 2},
    {33600, 655, 45, 45, 100, 655, 160, -1, 0},
    {34200, 100, 45, 45, 0, 300, 500, 1, 1},
    {34800, 500, 45, 45, 150, 500, 170, -1, 2},
    {35500, 655, 45, 45, 100, 655, 180, 1, 0},
    {36200, 100, 45, 45, 0, 300, 200, -1, 1},
    {37000, 500, 45, 45, 150, 500, 160, 1, 2},
    {37500, 655, 45, 45, 100, 655, 1000, -1, 0},
    {38200, 100, 45, 45, 0, 300, 190, 1, 1},
    {38800, 500, 45, 45, 150, 500, 170, -1, 2},
    {40100, 100, 45, 45, 0, 300, 200, -1, 1},
    {40800, 500, 45, 45, 150, 500, 1500, 1, 2},
    {41400, 655, 45, 45, 100, 655, 1000, -1, 0},
    {42200, 100, 45, 45, 0, 300, 190, 1, 1},
    {43000, 500, 45, 45, 150, 500, 1600, -1, 2},
    {44000, 655, 45, 45, 100, 655, 180, 1, 0},
    // ===== NOTUN: shesh er dike extra enemy (speed dhire dhire barano, 200 -> 220) =====
    {44650, 100, 45, 45, 0, 300, 200, 1, 1},
    {45250, 655, 45, 45, 100, 655, 210, -1, 0},
    {45450, 500, 45, 45, 150, 500, 200, 1, 2},
    {45800, 500, 45, 45, 300, 500, 200, -1, 2},
    {46200, 500, 45, 45, 150, 660, 160, 1, 2},
    {46350, 100, 45, 45, 0, 300, 210, -1, 1},
    {46650, 655, 45, 45, 100, 655, 220, 1, 0},
    {46900, 100, 45, 45, 0, 300, 220, -1, 1},

    {47000, 655, 45, 45, 100, 655, 180, -1, 0},
    {48000, 100, 45, 45, 0, 300, 190, 1, 1}};

// spike count ekhon auto - notun spike add korle count change korte hobe na

#define spike_count ((int)(sizeof(spikes) / sizeof(spikes[0])))

Rectangle getspikerect(spike *s)
{
    return (Rectangle){s->x, s->y, s->width, s->height};
}
// spike ar visible triangle er 3 ta point ber kore, type onujayi (0/2 = upward, 1/3 = downward)
void getspiketriangle(spike *s, Vector2 *p1, Vector2 *p2, Vector2 *p3)
{
    if (s->type == 0 || s->type == 2)
    {
        // spike floor theke upore mukh kore ase (upward pointing)
        *p1 = (Vector2){s->x, s->y + s->height};
        *p2 = (Vector2){s->x + s->width, s->y + s->height};
        *p3 = (Vector2){s->x + s->width / 2, s->y};
    }
    else
    {
        // spike ceiling theke niche mukh kore ase (downward pointing)
        *p1 = (Vector2){s->x, s->y};
        *p2 = (Vector2){s->x + s->width, s->y};
        *p3 = (Vector2){s->x + s->width / 2, s->y + s->height};
    }
}

// point kono triangle er vitore ase kina check kore (sign/barycentric method)
static float trianglesign(Vector2 p1, Vector2 p2, Vector2 p3)
{
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}
static int pointintriangle(Vector2 pt, Vector2 v1, Vector2 v2, Vector2 v3)
{
    float d1 = trianglesign(pt, v1, v2);
    float d2 = trianglesign(pt, v2, v3);
    float d3 = trianglesign(pt, v3, v1);
    int hasneg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    int haspos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasneg && haspos);
}
// circle kono line segment ke chuse/cross kore kina check kore
static int circleintersectssegment(Vector2 center, float r, Vector2 a, Vector2 b)
{
    Vector2 ab = {b.x - a.x, b.y - a.y};
    Vector2 ac = {center.x - a.x, center.y - a.y};
    float ablen2 = ab.x * ab.x + ab.y * ab.y;
    float t = (ablen2 > 0.0f) ? (ac.x * ab.x + ac.y * ab.y) / ablen2 : 0.0f;
    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;
    Vector2 closest = {a.x + ab.x * t, a.y + ab.y * t};
    float dx = center.x - closest.x;
    float dy = center.y - closest.y;
    return (dx * dx + dy * dy) <= (r * r);
}
// ball(circle) ar spike er REAL triangle shape er sathe collision
int checkcirclecollidestriangle(Vector2 center, float r, Vector2 p1, Vector2 p2, Vector2 p3)
{
    if (pointintriangle(center, p1, p2, p3))
        return 1;
    if (circleintersectssegment(center, r, p1, p2))
        return 1;
    if (circleintersectssegment(center, r, p2, p3))
        return 1;
    if (circleintersectssegment(center, r, p3, p1))
        return 1;
    return 0;
}
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

// SPIKE COLLISION

void checkspikecollision(Vector2 *ballposition, float *health, float *damagetimes)
{
    for (int i = 0; i < spike_count; i++)
    {
        // fast broad-phase check (bounding box) - shudhu perf er jonno, real hit exact triangle diye
        Rectangle broad = getspikerect(&spikes[i]);
        if (!(ballposition->x + radius > broad.x && ballposition->x - radius < broad.x + broad.width &&
              ballposition->y + radius > broad.y && ballposition->y - radius < broad.y + broad.height))
            continue;
        Vector2 p1, p2, p3;
        getspiketriangle(&spikes[i], &p1, &p2, &p3);
        // narrow-phase e thoda choto (forgiving) hitbox diye check
        if (checkcirclecollidestriangle(*ballposition, spike_hit_radius, p1, p2, p3))
        {
            if (*damagetimes <= 0)
            {
                if (shieldTimer > 0)
                {
                    // shield thakle damage lage na, shudhu chotto grace cooldown
                    *damagetimes = 0.4f;
                    SpawnBurst(*ballposition, 14, SKYBLUE, 150.0f, 0.4f, 4.0f);
                }
                else
                {
                    *health -= damage;
                    if (*health < 0)
                        *health = 0;
                    *damagetimes = 0.7f;
                    PlaySound(spikeSound); // spike e lagle sound
                    TriggerShake(0.25f, 6.0f);
                    SpawnBurst(*ballposition, 14, RED, 160.0f, 0.4f, 4.0f);
                }
            }
            break;
        }
    }
}

void drawspikes(void)
{
    for (int i = 0; i < spike_count; i++)
    {
        spike *s = &spikes[i];
        Vector2 p1, p2, p3;
        getspiketriangle(s, &p1, &p2, &p3);
        DrawTriangle(p1, p2, p3, BLACK);
    }
}

// >>>>>>> NOTUN: moving enemy (spike er moto damage + sound) >>>>>>>
// kind 0 = ground walker (mati te left-right hete beray)
// kind 1 = flyer (left-right ure, sathe upor-niche dulte thake)
typedef struct
{
    float x;
    float y;
    float r;
    float minx;
    float maxx;
    float speed;
    int direction;
    float basey; // flyer er moddho height
    float amp;   // flyer er upor-niche dolar poriman
    float phase;
    int kind;
} enemy;

// {x, y, r, minx, maxx, speed, direction, basey, amp, phase, kind}
enemy enemies[] = {
    // ---- ground walker (y = 700 - r = 675) ----
    {43700, 675, 25, 43700, 43950, 110, 1, 675, 0, 0, 0},
    {44500, 675, 25, 44120, 44880, 120, -1, 675, 0, 0, 0},
    {45400, 675, 25, 45150, 45650, 130, 1, 675, 0, 0, 0},
    {46000, 675, 25, 45700, 46400, 140, -1, 675, 0, 0, 0},
    {46800, 675, 25, 46560, 47050, 150, 1, 675, 0, 0, 0},
    // ---- flyer ----
    {43900, 420, 22, 43700, 44250, 140, 1, 420, 70, 0.0f, 1},
    {44700, 380, 22, 44500, 44900, 150, -1, 380, 60, 1.5f, 1},
    {45400, 450, 22, 45150, 45650, 160, 1, 450, 70, 3.0f, 1},
    {46200, 480, 22, 45950, 46400, 160, -1, 480, 50, 4.5f, 1},
    {46800, 420, 22, 46550, 47050, 170, 1, 420, 80, 2.0f, 1}};

void updateenemies(float dt)
{
    static float enemytime = 0.0f;
    enemytime += dt;
    for (int i = 0; i < enemy_count; i++)
    {
        enemy *e = &enemies[i];
        e->x += e->speed * e->direction * dt;
        if (e->x >= e->maxx)
        {
            e->x = e->maxx;
            e->direction = -1;
        }
        else if (e->x <= e->minx)
        {
            e->x = e->minx;
            e->direction = 1;
        }
        if (e->kind == 1)
        {
            e->y = e->basey + sinf(enemytime * 2.5f + e->phase) * e->amp;
        }
    }
}

// enemy te lagle spike er moto e health kombe + same sound
void checkenemycollision(Vector2 *ballposition, float *health, float *damagetimes)
{
    for (int i = 0; i < enemy_count; i++)
    {
        enemy *e = &enemies[i];
        float dx = ballposition->x - e->x;
        float dy = ballposition->y - e->y;
        float rr = spike_hit_radius + e->r * 0.85f; // forgiving hitbox
        if (dx * dx + dy * dy <= rr * rr)
        {
            if (*damagetimes <= 0)
            {
                if (shieldTimer > 0)
                {
                    *damagetimes = 0.4f;
                    SpawnBurst(*ballposition, 14, SKYBLUE, 150.0f, 0.4f, 4.0f);
                }
                else
                {
                    *health -= damage;
                    if (*health < 0)
                        *health = 0;
                    *damagetimes = 0.7f;
                    PlaySound(spikeSound);
                    TriggerShake(0.25f, 6.0f);
                    SpawnBurst(*ballposition, 14, RED, 160.0f, 0.4f, 4.0f);
                }
            }
            break;
        }
    }
}

void drawenemies(void)
{
    for (int i = 0; i < enemy_count; i++)
    {
        enemy *e = &enemies[i];
        Color body = (e->kind == 0) ? PURPLE : ORANGE;
        Vector2 c = {e->x, e->y};
        DrawCircleV(c, e->r, body);
        DrawCircleLines((int)e->x, (int)e->y, e->r, BLACK);
        // chokh - je dike jachche sedike takay
        float look = e->direction * e->r * 0.12f;
        Vector2 eye1 = {e->x - e->r * 0.35f + look, e->y - e->r * 0.15f};
        Vector2 eye2 = {e->x + e->r * 0.35f + look, e->y - e->r * 0.15f};
        DrawCircleV(eye1, e->r * 0.24f, WHITE);
        DrawCircleV(eye2, e->r * 0.24f, WHITE);
        DrawCircleV((Vector2){eye1.x + look, eye1.y}, e->r * 0.11f, BLACK);
        DrawCircleV((Vector2){eye2.x + look, eye2.y}, e->r * 0.11f, BLACK);
        // ragi bhru
        DrawLineEx((Vector2){eye1.x - e->r * 0.25f, eye1.y - e->r * 0.35f}, (Vector2){eye1.x + e->r * 0.2f, eye1.y - e->r * 0.15f}, 3, BLACK);
        DrawLineEx((Vector2){eye2.x + e->r * 0.25f, eye2.y - e->r * 0.35f}, (Vector2){eye2.x - e->r * 0.2f, eye2.y - e->r * 0.15f}, 3, BLACK);
    }
}
// <<<<<<< enemy shesh <<<<<<<

// >>>>>>> NOTUN FEATURE: checkpoint system (flag pole + continue-after-death) >>>>>>>
typedef struct
{
    float x;
    int reached;
} Checkpoint;

Checkpoint checkpoints[] = {
    {5000, 0}, {10000, 0}, {15000, 0}, {20000, 0}, {25000, 0}, {30000, 0}, {35000, 0}, {40000, 0}, {45000, 0}};
#define checkpoint_count ((int)(sizeof(checkpoints) / sizeof(checkpoints[0])))

void checkcheckpoints(Vector2 ballposition, float *health)
{
    for (int i = 0; i < checkpoint_count; i++)
    {
        if (!checkpoints[i].reached && ballposition.x >= checkpoints[i].x)
        {
            checkpoints[i].reached = 1;
            lastCheckpointX = checkpoints[i].x;
            *health = fminf(maxhealth, *health + 15.0f);
            PushNotification("CHECKPOINT REACHED!");
            SpawnBurst(ballposition, 24, SKYBLUE, 200.0f, 0.7f, 5.0f);
            // NOTUN: checkpoint e pouche o coin er moto sound (anikcoin2.wav) bajbe
            PlaySound(coinSound);
        }
    }
}

void drawcheckpoints(void)
{
    for (int i = 0; i < checkpoint_count; i++)
    {
        Color poleColor = checkpoints[i].reached ? GREEN : GRAY;
        DrawRectangle((int)checkpoints[i].x - 4, 100, 8, 600, poleColor);
        Vector2 f1 = {checkpoints[i].x + 4, 110};
        Vector2 f2 = {checkpoints[i].x + 4, 150};
        Vector2 f3 = {checkpoints[i].x + 50, 130};
        DrawTriangle(f1, f2, f3, checkpoints[i].reached ? LIME : LIGHTGRAY);
    }
}

void resetcheckpoints(void)
{
    for (int i = 0; i < checkpoint_count; i++)
        checkpoints[i].reached = 0;
    lastCheckpointX = 250;
}
// <<<<<<< checkpoint system shesh <<<<<<<

// game over / win screen ek jaygay
void DrawEndScreen(const char *title, Color titlecolor, int width, int height)
{
    DrawRectangle(0, 0, width, height, Fade(RAYWHITE, 0.92f));
    DrawTextCentered(title, height / 2 - 210, 50, titlecolor);
    DrawTextCentered(TextFormat("player: %s", playername), height / 2 - 145, 26, DARKGRAY);
    DrawTextCentered(TextFormat("final score: %d", gamescore), height / 2 - 105, 30, BLACK);
    if (newrecord)
    {
        DrawTextCentered("NEW HIGH SCORE!", height / 2 - 65, 30, ORANGE);
    }

    // leaderboard box
    int boxw = 320;
    int boxx = (width - boxw) / 2;
    int boxy = height / 2 - 25;
    DrawRectangle(boxx, boxy, boxw, 130, Fade(BLACK, 0.06f));
    DrawRectangleLines(boxx, boxy, boxw, 130, DARKGRAY);
    DrawTextCentered("TOP 5", boxy + 6, 20, MAROON);
    for (int i = 0; i < leaderboard_size; i++)
    {
        Color rowc = (i == 0) ? GOLD : DARKGRAY;
        DrawText(TextFormat("%d. %-10s %6d", i + 1, leaderboard[i].name, leaderboard[i].score), boxx + 15, boxy + 30 + i * 20, 18, rowc);
    }

    DrawTextCentered("press enter to restart", boxy + 150, 22, DARKGRAY);
    if (!gamewon && lastCheckpointX > 250.0f)
    {
        DrawTextCentered(TextFormat("press c to continue from checkpoint (x:%.0f)", lastCheckpointX), boxy + 178, 20, DARKGREEN);
    }
    DrawTextCentered("press m for main menu", boxy + 206, 22, DARKGRAY);
    DrawTextCentered("press esc to exit", boxy + 232, 20, DARKGRAY);
}

// BOM ADDING

// >>>>>>> NOTUN: ceiling bomb dropper >>>>>>>
#define max_bombs 16
#define bomb_r 14
#define bomb_gravity 700.0f
#define bomb_blast_radius 60.0f
#define bomb_blast_time 0.35f

typedef struct
{
    float x, y, vy;
    int state; // 0 = nai, 1 = porche, 2 = blast hocche
    float timer;
} bomb;
bomb bombs[max_bombs];

// je zone e ball dhukle dropper active hobe
typedef struct
{
    float startx, endx;
    float interval; // koto second por por bomb
    float timer;
    float dropx; // dropper er current x
    int active;
} bombzone;

// {startx, endx, interval, timer, dropx, active}
bombzone bombzones[] = {
    {10300, 11400, 0.15f, 0, 0, 0},
    {18000, 19000, 0.15f, 0, 0, 0},
    {41000, 42400, 0.1f, 0, 0, 0},
    {6960, 8200, .08F, 0, 0, 0},
    {12435, 13495, .1F, 0, 0, 0},
    {21999, 23200, .09F, 0, 0, 0},
    {16650, 17500, .09F, 0, 0, 0},
    {25955, 26838, .1F, 0, 0, 0},
    {27448, 29645, .11111F, 0, 0, 0},
    {30389, 30759, .1F, 0, 0, 0},
    {32741, 35650, .07F, 0, 0, 0},

};
#define bombzone_count ((int)(sizeof(bombzones) / sizeof(bombzones[0])))

static void spawnbomb(float x)
{
    for (int i = 0; i < max_bombs; i++)
    {
        if (bombs[i].state == 0)
        {
            bombs[i] = (bomb){x, 150, 0, 1, 0};
            PlaySound(bombDropSound); // bomb drop howar somoy sound
            return;
        }
    }
}

static void explodebomb(bomb *b, Vector2 ball, float *health, float *damagetimes)
{
    b->state = 2;
    b->timer = bomb_blast_time;
    float dx = ball.x - b->x;
    float dy = ball.y - b->y;
    float rr = bomb_blast_radius + radius * 0.5f;
    if (dx * dx + dy * dy <= rr * rr && *damagetimes <= 0)
    {
        if (shieldTimer > 0)
        {
            *damagetimes = 0.4f;
            SpawnBurst(ball, 14, SKYBLUE, 150.0f, 0.4f, 4.0f);
        }
        else
        {
            *health -= damage;
            if (*health < 0)
                *health = 0;
            *damagetimes = 0.7f;
            PlaySound(bombHitSound); // ball e bomb lagle sound
            TriggerShake(0.3f, 8.0f);
        }
    }
}

void updatebombs(float dt, Vector2 ball, float *health, float *damagetimes)
{
    // zone check + dropper move + bomb spawn
    for (int z = 0; z < bombzone_count; z++)
    {
        bombzone *bz = &bombzones[z];
        if (ball.x >= bz->startx && ball.x <= bz->endx)
        {
            if (!bz->active)
            {
                bz->active = 1;
                bz->dropx = fminf(ball.x + 500, bz->endx); // samne theke ashbe
                bz->timer = 0.8f;
            }
            float diff = ball.x - bz->dropx;
            float step = 260.0f * dt;
            if (fabsf(diff) <= step)
                bz->dropx = ball.x;
            else
                bz->dropx += (diff > 0 ? step : -step);

            bz->timer -= dt;
            if (bz->timer <= 0)
            {
                bz->timer = bz->interval;
                spawnbomb(bz->dropx + GetRandomValue(-40, 40));
            }
        }
        else
        {
            bz->active = 0;
        }
    }

    // bomb update
    for (int i = 0; i < max_bombs; i++)
    {
        bomb *b = &bombs[i];
        if (b->state == 1)
        {
            b->vy += bomb_gravity * dt;
            b->y += b->vy * dt;

            int hit = 0;
            // ground
            if (b->y + bomb_r >= 700)
            {
                b->y = 700 - bomb_r;
                hit = 1;
            }
            // platform
            for (int p = 0; p < platforms_count && !hit; p++)
            {
                float left = platforms[p].x;
                float right = left + platforms[p].length * platformbrick_width;
                float top = platforms[p].y;
                float bottom = top + platforms[p].height * platformbrick_height;
                if (b->x + bomb_r > left && b->x - bomb_r < right && b->y + bomb_r > top && b->y - bomb_r < bottom)
                {
                    hit = 1;
                }
            }
            // ball e sorasori lagle
            float dx = ball.x - b->x;
            float dy = ball.y - b->y;
            float rs = radius + bomb_r;
            if (dx * dx + dy * dy <= rs * rs)
                hit = 1;

            if (hit)
                explodebomb(b, ball, health, damagetimes);
        }
        else if (b->state == 2)
        {
            b->timer -= dt;
            if (b->timer <= 0)
                b->state = 0;
        }
    }
}

void drawbombs(void)
{
    // dropper enemy (ceiling er niche)
    for (int z = 0; z < bombzone_count; z++)
    {
        if (!bombzones[z].active)
            continue;
        float dx = bombzones[z].dropx;
        DrawCircle((int)dx, 125, 22, MAROON);
        DrawCircleLines((int)dx, 125, 22, BLACK);
        DrawCircle((int)dx - 8, 120, 6, WHITE);
        DrawCircle((int)dx + 8, 120, 6, WHITE);
        DrawCircle((int)dx - 8, 121, 3, BLACK);
        DrawCircle((int)dx + 8, 121, 3, BLACK);
        DrawLineEx((Vector2){dx - 16, 108}, (Vector2){dx - 3, 114}, 3, BLACK);
        DrawLineEx((Vector2){dx + 16, 108}, (Vector2){dx + 3, 114}, 3, BLACK);
    }
    // bomb
    for (int i = 0; i < max_bombs; i++)
    {
        bomb *b = &bombs[i];
        if (b->state == 1)
        {
            DrawCircleV((Vector2){b->x, b->y}, bomb_r, BLACK);
            DrawCircleV((Vector2){b->x - 4, b->y - 4}, 4, DARKGRAY);
            DrawLineEx((Vector2){b->x, b->y - bomb_r}, (Vector2){b->x + 5, b->y - bomb_r - 8}, 3, BROWN);
            DrawCircleV((Vector2){b->x + 5, b->y - bomb_r - 8}, 4, (((int)(GetTime() * 20)) % 2) ? ORANGE : RED);
        }
        else if (b->state == 2)
        {
            float t = 1.0f - b->timer / bomb_blast_time;
            float r = bomb_blast_radius * (0.4f + 0.6f * t);
            DrawCircleV((Vector2){b->x, b->y}, r, Fade(ORANGE, 1.0f - t));
            DrawCircleV((Vector2){b->x, b->y}, r * 0.6f, Fade(YELLOW, 1.0f - t));
        }
    }
}

void resetbombs(void)
{
    for (int i = 0; i < max_bombs; i++)
        bombs[i].state = 0;
    for (int z = 0; z < bombzone_count; z++)
    {
        bombzones[z].active = 0;
        bombzones[z].timer = 0;
    }
}
// <<<<<<< bomb dropper shesh <<<<<

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
    int bouncesremaining = max_bounces;
    int musicPaused = 0;
    static float dustTimer = 0.0f;

    InitWindow(width, height, "bouncing classic game");
    InitAudioDevice();

    // >>>>>>> NOTUN: sob theke gurutwopurno check - audio device e ki ashole ready hoyeche? >>>>>>>
    // ei check ta fail korle, code e kono bhul nai - tomar system/environment e
    // kono audio driver/speaker/output e pawa jacche na (WSL, remote server, Docker,
    // virtual machine e eta khub common - shegulote by default sound card thake na)
    if (!IsAudioDeviceReady())
    {
        TraceLog(LOG_ERROR, "AUDIO DEVICE READY HOYNI! Tomar system/environment e kono sound output pawa jacche na. WSL/remote server/VM hole eta e main karon - normal Windows/laptop e run korle ei error asha uchit na.");
    }
    else
    {
        TraceLog(LOG_INFO, "Audio device ready ache - sound system thik ache.");
    }
    // path shothik jaygay ache kina check korar jonno current working directory print kora hocche
    TraceLog(LOG_INFO, "Current working directory: %s", GetWorkingDirectory());
    // <<<<<<<

    // >>>>>>> asol 6 ta sound + notun 4 ta sound (coin/diamond/bomb) sob load hocche >>>>>>>
    bgMusic = LoadMusicStream("assert/anik.mp3");
    jumpSound = LoadSound("assert/anik2.mp3");
    spikeSound = LoadSound("assert/anik3.mp3");
    gameOverSound = LoadSound("assert/anik4.mp3");
    gameOverBgSound = LoadMusicStream("assert/anik5.wav");
    restartSound = LoadSound("assert/anik6.mp3");
    // notun sound file gulo load
    coinSound = LoadSound("assert/anikcoin2.wav");
    diamondSound = LoadSound("assert/anikcoin.wav");
    bombDropSound = LoadSound("assert/anikmetal.wav");
    bombHitSound = LoadSound("assert/anikbomb.wav");

    // load thikmoto hoyeche kina console e check (frameCount==0 hole file pawa jayni)
    if (jumpSound.frameCount == 0) TraceLog(LOG_WARNING, "jumpSound load fail hoyeche! (assert/anik2.mp3)");
    if (spikeSound.frameCount == 0) TraceLog(LOG_WARNING, "spikeSound load fail hoyeche! (assert/anik3.mp3)");
    if (gameOverSound.frameCount == 0) TraceLog(LOG_WARNING, "gameOverSound load fail hoyeche! (assert/anik4.mp3)");
    if (restartSound.frameCount == 0) TraceLog(LOG_WARNING, "restartSound load fail hoyeche! (assert/anik6.mp3)");
    if (bgMusic.frameCount == 0) TraceLog(LOG_WARNING, "bgMusic load fail hoyeche! (assert/anik.mp3)");
    if (gameOverBgSound.frameCount == 0) TraceLog(LOG_WARNING, "gameOverBgSound load fail hoyeche! (assert/anik5.wav)");
    if (coinSound.frameCount == 0) TraceLog(LOG_WARNING, "coinSound load fail hoyeche! (assert/anikcoin2.wav)");
    if (diamondSound.frameCount == 0) TraceLog(LOG_WARNING, "diamondSound load fail hoyeche! (assert/anikcoin.wav)");
    if (bombDropSound.frameCount == 0) TraceLog(LOG_WARNING, "bombDropSound load fail hoyeche! (assert/anikmetal.wav)");
    if (bombHitSound.frameCount == 0) TraceLog(LOG_WARNING, "bombHitSound load fail hoyeche! (assert/anikbomb.wav)");
    // <<<<<<<

    SetMasterVolume(1.0f);
    muted = 0;

    LoadLeaderboard();

    SetWindowState(FLAG_VSYNC_HINT);
    SetTargetFPS(70);

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        Vector2 previousballposition = ballposition;
        int menuScreen = (!gamestarted && !gameover && !gamepaused && !gamewon);
        int playing = (gamestarted && !gamepaused && !gameover && !gamewon);

        // ---------- music ----------
        if (playing)
        {
            if (musicPaused)
            {
                ResumeMusicStream(bgMusic);
                musicPaused = 0;
            }
            else if (!IsMusicStreamPlaying(bgMusic))
            {
                PlayMusicStream(bgMusic);
            }
            UpdateMusicStream(bgMusic);
        }
        else if (gamepaused && !gameover && !gamewon)
        {
            // pause e music thamiye rakhbo, resume korle jekhan theke thamse sekhan theke cholbe
            if (!musicPaused && IsMusicStreamPlaying(bgMusic))
            {
                PauseMusicStream(bgMusic);
                musicPaused = 1;
            }
        }
        else
        {
            StopMusicStream(bgMusic);
            musicPaused = 0;
        }

        if (gameover)
        {
            UpdateMusicStream(gameOverBgSound);
            if (!IsMusicStreamPlaying(gameOverBgSound))
            {
                PlayMusicStream(gameOverBgSound);
            }
        }
        else
        {
            StopMusicStream(gameOverBgSound);
        }

        // ---------- main menu: name input ----------
        if (menuScreen)
        {
            UpdateNameInput();
        }

        // ---------- NOTUN: mute toggle (shudhu menu name-typing er baire, na hole 'n' likhle mute hoye jay) ----------
        if (!menuScreen && IsKeyPressed(KEY_N))
        {
            muted = !muted;
            SetMasterVolume(muted ? 0.0f : 1.0f);
        }

        // ---------- enter / m / c key ----------
        int wantRestart = 0;
        int wantMenu = 0;
        int wantContinue = 0;
        if (IsKeyPressed(KEY_ENTER))
        {
            if (menuScreen)
            {
                // name na dile game start hobe na
                if (namelen > 0)
                {
                    gamestarted = 1;
                    gamepaused = 0;
                    newrecord = 0;
                }
            }
            else if (gameover || gamewon)
            {
                wantRestart = 1;
            }
        }
        if ((gameover || gamewon) && IsKeyPressed(KEY_M))
        {
            wantMenu = 1;
            while (GetCharPressed() > 0)
            {
            } // 'm' key ta name box e jate na dhoke
        }
        // NOTUN: checkpoint theke continue korar option (game over hole, win na hole)
        if (gameover && !gamewon && IsKeyPressed(KEY_C) && lastCheckpointX > 250.0f)
        {
            wantContinue = 1;
        }
        if (wantRestart || wantMenu)
        {
            if (wantRestart)
                PlaySound(restartSound);

            gameover = 0;
            gamewon = 0;
            gamestarted = wantRestart ? 1 : 0;
            gamepaused = 0;
            newrecord = 0;
            // reset health
            health = maxhealth;
            damagetimes = 0;
            // reset player
            ballposition.x = 250;
            ballposition.y = 670;
            // reset physics
            vertialvelocity = 0;
            jumpcount = 0;
            bouncesremaining = max_bounces;
            // reset score
            maxdistance = 0;
            gamescore = 0;
            // reset camera
            camera.target.x = ballposition.x;
            camera.target.y = ground;
            resetcoins();
            resetdiamonds();
            resetbombs();
            // NOTUN: notun feature gulor reset
            resetpowerups();
            resetcheckpoints();
            ResetParticles();
            comboCount = 0;
            comboTimer = 0;
            shakeTime = 0;
        }
        else if (wantContinue)
        {
            // NOTUN: pura restart na kore, last checkpoint theke abar shuru
            PlaySound(restartSound);
            gameover = 0;
            gamestarted = 1;
            gamepaused = 0;
            health = maxhealth * 0.6f;
            damagetimes = 0.5f;
            ballposition.x = lastCheckpointX;
            ballposition.y = ground - 250;
            vertialvelocity = 0;
            jumpcount = 0;
            bouncesremaining = max_bounces;
            camera.target.x = ballposition.x;
            shieldTimer = 2.0f; // continue korar por halka grace period
            PushNotification("CONTINUING FROM CHECKPOINT");
        }

        // ---------- pause ----------
        if (IsKeyPressed(KEY_P) && gamestarted && !gameover && !gamewon)
        {
            gamepaused = !gamepaused;
        }

        // ---------- player input ----------
        direction.x = 0;
        if (gamestarted && !gamepaused && !gameover && !gamewon)
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
                bouncesremaining = max_bounces;
            }
        }

        // ---------- game update ----------
        if (gamestarted && !gamepaused && !gameover && !gamewon)
        {
            // spike ar damage timer shudhu game cholar somoy update hobe (pause e freeze thakbe)
            updatespikes(dt);
            updateenemies(dt);
            updatebombs(dt, ballposition, &health, &damagetimes);
            UpdateParticles(dt);
            UpdateNotifications(dt);
            if (damagetimes > 0)
            {
                damagetimes -= dt;
            }

            // NOTUN: powerup/combo/shake timer gulo count-down
            if (shieldTimer > 0)
            {
                shieldTimer -= dt;
                if (shieldTimer < 0)
                    shieldTimer = 0;
            }
            if (magnetTimer > 0)
            {
                magnetTimer -= dt;
                if (magnetTimer < 0)
                    magnetTimer = 0;
            }
            if (speedTimer > 0)
            {
                speedTimer -= dt;
                if (speedTimer < 0)
                    speedTimer = 0;
            }
            if (comboTimer > 0)
            {
                comboTimer -= dt;
                if (comboTimer <= 0)
                {
                    comboTimer = 0;
                    comboCount = 0;
                }
            }
            if (shakeTime > 0)
            {
                shakeTime -= dt;
                if (shakeTime < 0)
                    shakeTime = 0;
            }

            applymagnet(ballposition, dt);

            if (ballposition.x > maxdistance)
            {
                maxdistance = ballposition.x;

                int distanceScore = (int)(maxdistance / 20.0f);

                if (distanceScore > gamescore)
                {
                    gamescore = distanceScore;
                }
            }

            float speedMultiplier = (speedTimer > 0) ? 1.6f : 1.0f;

            // gravity
            vertialvelocity += gravity * dt;
            // movement
            ballposition.x += direction.x * speed * speedMultiplier * dt;
            ballposition.y += vertialvelocity * dt;
            // platform collision
            // CALLING COIND FUNCTION

            checkcoincollision(ballposition, &gamescore);
            checkcollision(&ballposition, previousballposition, &vertialvelocity, &jumpcount, &bouncesremaining);

            if (ballposition.y + radius >= ground)
            {
                ballposition.y = ground - radius;
                // ground e landing - kom kom bounce kore tarpor thambe
                float incoming = fabsf(vertialvelocity);
                if (incoming > bounce_min_velocity && bouncesremaining > 0)
                {
                    vertialvelocity = -(incoming * bounce_restitution + bounce_extra_power);
                    bouncesremaining--;
                    PlaySound(jumpSound); // ground e bounce korle sound
                }
                else
                {
                    vertialvelocity = 0.0f;
                    bouncesremaining = max_bounces;
                }
                jumpcount = 0;
            }
            // Ceiling collision
            else if (ballposition.y - radius <= 80)
            {
                ballposition.y = 80 + radius;

                if (vertialvelocity < 0)
                {
                    vertialvelocity = -vertialvelocity * 0.5f; //
                }
            }

            // FUNCTON CALLING
            checkdiamondcollision(ballposition, &gamescore);
            checkspikecollision(&ballposition, &health, &damagetimes);
            checkenemycollision(&ballposition, &health, &damagetimes);
            // NOTUN: powerup ar checkpoint check
            checkpowerupcollision(ballposition);
            checkcheckpoints(ballposition, &health);

            // NOTUN: mati te dourale dhula (dust trail) particle
            dustTimer -= dt;
            if (fabsf(vertialvelocity) < 60.0f && direction.x != 0 && dustTimer <= 0)
            {
                Vector2 dp = {ballposition.x - direction.x * radius * 0.6f, ballposition.y + radius * 0.7f};
                Vector2 dv = {-direction.x * 40.0f, -30.0f};
                SpawnParticle(dp, dv, 0.35f, 4.0f, Fade(LIGHTGRAY, 0.7f));
                dustTimer = 0.06f;
            }
            // NOTUN: speed boost thakle trail
            if (speedTimer > 0)
            {
                SpawnParticle((Vector2){ballposition.x, ballposition.y}, (Vector2){-direction.x * 30.0f, 10.0f}, 0.3f, 5.0f, Fade(YELLOW, 0.6f));
            }

            // ball rotation
            ballrotation += (direction.x * speed * speedMultiplier * dt) / radius * 5;
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
                PlaySound(gameOverSound);
                FinishRun();
            }
            // win check
            if (!gameover && ballposition.x >= win_x)
            {
                gamewon = 1;
                gamestarted = 0;
                gamepaused = 0;
                direction.x = 0;
                vertialvelocity = 0;
                PlaySound(restartSound); // jeetle celebration sound
                FinishRun();
            }
        }

        // camera rounding
        Camera2D camera_rounded = camera;
        camera_rounded.target.x = roundf(camera.target.x);
        camera_rounded.target.y = roundf(camera.target.y);
        // NOTUN: damage khele screen shake
        if (shakeTime > 0)
        {
            camera_rounded.offset.x += (float)GetRandomValue(-100, 100) / 100.0f * shakeMagnitude;
            camera_rounded.offset.y += (float)GetRandomValue(-100, 100) / 100.0f * shakeMagnitude;
        }

        // ---------- drawing ----------
        BeginDrawing();

        // background (screen space e - jate camera move korleo gradient thake)
        Color skytop, skybottom;
        Back_Ground_Color(maxdistance, &skytop, &skybottom);
        ClearBackground(skytop);
        DrawRectangleGradientV(0, 0, width, height, skytop, skybottom);
        DrawParallaxBackground(camera.target.x, width, height);

        BeginMode2D(camera_rounded);
        drawcheckpoints();
        drawcoins();
        drawdiamonds();
        drawpowerups();

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
        // enemies
        drawenemies();
        drawbombs();

        // NOTUN: shield thakle ball er charpashe glow ring
        if (shieldTimer > 0)
        {
            DrawCircleLines((int)ballposition.x, (int)ballposition.y, radius + 6, Fade(SKYBLUE, 0.9f));
            DrawCircleLines((int)ballposition.x, (int)ballposition.y, radius + 10, Fade(SKYBLUE, 0.5f));
        }
        // NOTUN: magnet range ring
        if (magnetTimer > 0)
        {
            DrawCircleLines((int)ballposition.x, (int)ballposition.y, magnet_radius, Fade(RED, 0.25f));
        }
        // ball
        DrawCircle(ballposition.x, ballposition.y, radius, BLUE);
        // NOTUN: particle gulo ball er upore ashe
        DrawParticles();
        EndMode2D();

        // ---------- main menu ----------
        if (!gamestarted && !gameover && !gamepaused && !gamewon)
        {
            DrawRectangle(0, 0, width, height, BLACK);
            DrawTextCentered("bouncing classic", 55, 55, RED);

            // NOTUN: top 5 leaderboard main menu te
            DrawTextCentered("TOP 5 SCORES", 128, 22, GOLD);
            for (int i = 0; i < leaderboard_size; i++)
            {
                DrawTextCentered(TextFormat("%d. %-10s %6d", i + 1, leaderboard[i].name, leaderboard[i].score), 156 + i * 20, 18, (i == 0) ? GOLD : LIGHTGRAY);
            }

            DrawTextCentered("enter your name:", 272, 22, RAYWHITE);

            // name input box
            int boxw = 420;
            int boxx = ((int)width - boxw) / 2;
            DrawRectangle(boxx, 302, boxw, 46, DARKGRAY);
            DrawRectangleLines(boxx, 302, boxw, 46, RAYWHITE);
            const char *cursor = (((int)(GetTime() * 2)) % 2 == 0) ? "_" : "";
            DrawText(TextFormat("%s%s", playername, cursor), boxx + 12, 313, 26, RAYWHITE);

            if (namelen > 0)
                DrawTextCentered("press enter to start", 362, 24, GREEN);
            else
                DrawTextCentered("type your name to begin", 362, 24, GRAY);

            DrawTextCentered("left / right = move", 420, 20, LIGHTGRAY);
            DrawTextCentered("space = jump (double jump)", 445, 20, LIGHTGRAY);
            DrawTextCentered("p = pause    n = mute/unmute", 470, 20, LIGHTGRAY);
            DrawTextCentered("blue = shield   red = magnet   yellow = speed boost", 495, 18, LIGHTGRAY);
        }
        // ---------- pause menu ----------
        if (gamepaused && !gameover && !gamewon)
        {
            DrawRectangle(0, 0, width, height, Fade(RAYWHITE, 0.90f));
            DrawTextCentered("game paused", height / 2 - 100, 45, BLACK);
            DrawTextCentered("press p to resume", height / 2 - 20, 25, DARKGRAY);
        }
        // ---------- game over screen ----------
        if (gameover)
        {
            DrawEndScreen("game over", RED, width, height);
        }
        // ---------- win screen ----------
        if (gamewon)
        {
            DrawEndScreen("YOU WIN THE GAME", GREEN, width, height);
        }
        // ---------- HUD ----------
        if (gamestarted || gamepaused || gameover || gamewon)
        {
            // health bar
            DrawRectangle(20, 20, 300, 30, DARKGRAY);
            DrawRectangle(20, 20, (int)(300 * (health / maxhealth)), 30, GREEN);
            DrawRectangleLines(20, 20, 300, 30, BLACK);
            DrawText(TextFormat("hp: %.0f / %.0f", health, maxhealth), 35, 24, 22, BLACK);

            DrawText(TextFormat("score: %d", gamescore), 20, 58, 30, BLACK);
            DrawText(TextFormat("player: %s", playername), 20, 95, 22, BLACK);
            DrawText(TextFormat("best: %d (%s)", highscore, highname), 20, 120, 22, BLACK);
            DrawText(TextFormat("x: %.0f", ballposition.x), 20, 145, 22, BLACK);

            // NOTUN: combo counter
            if (comboCount > 0)
            {
                int mult = 1 + comboCount / 5;
                if (mult > combo_max_mult)
                    mult = combo_max_mult;
                DrawText(TextFormat("combo x%d", mult), 20, 170, 22, ORANGE);
            }

            // NOTUN: active powerup timer bar (upper right)
            int py = 20;
            int px = (int)width - 230;
            if (shieldTimer > 0)
            {
                DrawRectangle(px, py, 210, 26, Fade(SKYBLUE, 0.85f));
                DrawText(TextFormat("SHIELD %.1fs", shieldTimer), px + 8, py + 4, 18, DARKBLUE);
                py += 30;
            }
            if (magnetTimer > 0)
            {
                DrawRectangle(px, py, 210, 26, Fade(RED, 0.55f));
                DrawText(TextFormat("MAGNET %.1fs", magnetTimer), px + 8, py + 4, 18, MAROON);
                py += 30;
            }
            if (speedTimer > 0)
            {
                DrawRectangle(px, py, 210, 26, Fade(YELLOW, 0.85f));
                DrawText(TextFormat("SPEED %.1fs", speedTimer), px + 8, py + 4, 18, ORANGE);
                py += 30;
            }
            DrawText(muted ? "sound: off (n)" : "sound: on (n)", (int)width - 190, (int)height - 30, 18, DARKGRAY);
        }
        // NOTUN: notification popup gulo sobar upore
        DrawNotifications((int)width);
        EndDrawing();
    }

    UnloadMusicStream(bgMusic);
    UnloadSound(jumpSound);
    UnloadSound(spikeSound);
    UnloadSound(gameOverSound);
    UnloadMusicStream(gameOverBgSound);
    UnloadSound(restartSound);
    UnloadSound(coinSound);
    UnloadSound(diamondSound);
    UnloadSound(bombDropSound);
    UnloadSound(bombHitSound);

    CloseAudioDevice();
    CloseWindow();
    return 0;
}
