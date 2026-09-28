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
#define sentry_hit_radius 17.0f
#define sentry_damage 15.0f
#define sentry_max_speed 1500.0f
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

// Stone-age science art direction. Generated textures live in assets/stone-age-science/.
static const Color ARENA_PINK = {35, 181, 190, 255};       // cyan science accent
static const Color ARENA_PINK_DARK = {16, 79, 82, 255};    // deep oxidized copper
static const Color ARENA_MINT = {164, 201, 119, 255};       // moss green
static const Color ARENA_INK = {16, 28, 27, 255};           // charcoal ink
static const Color ARENA_CREAM = {239, 226, 184, 255};      // aged paper
static const Color SCIENCE_COPPER = {190, 112, 54, 255};
static Texture2D menuBackground;
static Texture2D gameplayBackground;
static Texture2D playerToken;
static Texture2D platformBlockTexture;
static Texture2D platformTileTexture;
static Texture2D scienceTokenTexture;
static Texture2D scienceCrystalTexture;
static Texture2D powerTextures[3];
static Texture2D checkpointTexture;
static Texture2D bombTexture;
static Texture2D bombLauncherTexture;
static Texture2D groundEnemyTexture;
static Texture2D flyingEnemyTexture;
static Texture2D hazardTextures[3];

enum
{
    MENU_HOME = 0,
    MENU_HOW_TO_PLAY = 1,
    MENU_CREDITS = 2
};
static int menuPage = MENU_HOME;
static float hazardThemeTimer = 0.0f;
static int hazardTheme = 0;
#define hazard_theme_duration 12.0f

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

static void DrawTextInRectCentered(const char *text, Rectangle rect, int size, Color color)
{
    int textWidth = MeasureText(text, size);
    DrawText(text, (int)(rect.x + (rect.width - textWidth) * 0.5f),
             (int)(rect.y + (rect.height - size) * 0.5f), size, color);
}

static void DrawTextureCover(Texture2D texture, Rectangle destination, Color tint)
{
    if (texture.id == 0)
        return;

    float destinationRatio = destination.width / destination.height;
    float textureRatio = (float)texture.width / (float)texture.height;
    Rectangle source = {0, 0, (float)texture.width, (float)texture.height};

    if (textureRatio > destinationRatio)
    {
        source.width = texture.height * destinationRatio;
        source.x = (texture.width - source.width) * 0.5f;
    }
    else
    {
        source.height = texture.width / destinationRatio;
        source.y = (texture.height - source.height) * 0.5f;
    }

    DrawTexturePro(texture, source, destination, (Vector2){0, 0}, 0.0f, tint);
}

static void DrawSpriteCentered(Texture2D texture, float x, float y, float width, float height,
                               float rotation, int flipX, Color tint)
{
    if (texture.id == 0)
        return;
    Rectangle source = {0, 0, (float)texture.width, (float)texture.height};
    if (flipX)
    {
        source.x = (float)texture.width;
        source.width = -(float)texture.width;
    }
    DrawTexturePro(texture, source, (Rectangle){x, y, width, height},
                   (Vector2){width * 0.5f, height * 0.5f}, rotation, tint);
}

static void DrawGlassPanel(Rectangle rect, Color accent)
{
    DrawRectangleRounded(rect, 0.035f, 8, Fade(ARENA_INK, 0.88f));
    DrawRectangleRoundedLinesEx(rect, 0.035f, 8, 2.0f, Fade(accent, 0.85f));
    DrawRectangle((int)rect.x, (int)rect.y, 6, (int)rect.height, accent);
}

static Rectangle GetMenuPlayButton(int width, int height)
{
    (void)width;
    (void)height;
    return (Rectangle){96, 390, 500, 58};
}

static Rectangle GetMenuHowButton(void)
{
    return (Rectangle){96, 462, 240, 54};
}

static Rectangle GetMenuCreditsButton(void)
{
    return (Rectangle){356, 462, 240, 54};
}

static Rectangle GetMenuBackButton(void)
{
    return (Rectangle){675, 610, 250, 54};
}

static int PointInside(Vector2 point, Rectangle rect)
{
    return point.x >= rect.x && point.x <= rect.x + rect.width &&
           point.y >= rect.y && point.y <= rect.y + rect.height;
}

static void DrawArenaBlock(float x, float y, float w, float h)
{
    Texture2D texture = (w / h > 1.25f) ? platformBlockTexture : platformTileTexture;
    if (texture.id != 0)
    {
        float drawW = w * 1.06f;
        float drawH = h * ((w / h > 1.25f) ? 1.35f : 1.08f);
        DrawSpriteCentered(texture, x + w * 0.5f, y + h * 0.5f, drawW, drawH, 0.0f, 0, WHITE);
        return;
    }
    Color top = (Color){42, 94, 98, 255};
    Color bottom = (Color){13, 39, 45, 255};
    DrawRectangleGradientV((int)x, (int)y, (int)w, (int)h, top, bottom);
    DrawRectangleLinesEx((Rectangle){x, y, w, h}, 2.0f, Fade(ARENA_MINT, 0.82f));
    DrawLineEx((Vector2){x + 5, y + 7}, (Vector2){x + w - 5, y + 7}, 2.0f, Fade(ARENA_CREAM, 0.34f));
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
            float pulse = 1.0f + sinf((float)GetTime() * 4.0f + i * 0.35f) * 0.08f;
            float size = coins[i].coinradius * 2.75f * pulse;
            if (scienceTokenTexture.id != 0)
            {
                DrawSpriteCentered(scienceTokenTexture, coins[i].x, coins[i].y,
                                   size, size, (float)GetTime() * 18.0f, 0, WHITE);
            }
            else
            {
                DrawCircle((int)coins[i].x, (int)coins[i].y, coins[i].coinradius, GOLD);
                DrawCircleLines((int)coins[i].x, (int)coins[i].y, coins[i].coinradius, ORANGE);
            }
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

            if (scienceCrystalTexture.id != 0)
            {
                float bob = sinf((float)GetTime() * 2.5f + i) * 4.0f;
                DrawSpriteCentered(scienceCrystalTexture, x, y + bob, s * 2.65f, s * 2.85f,
                                   0.0f, 0, WHITE);
                continue;
            }

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

        int powerType = powerups[i].type;
        if (powerType >= 0 && powerType < 3 && powerTextures[powerType].id != 0)
        {
            float size = r * 3.2f;
            DrawCircle((int)x, (int)y, r * 1.22f,
                       Fade(powerType == POWER_SPEED ? LIME : SKYBLUE, 0.13f));
            DrawSpriteCentered(powerTextures[powerType], x, y, size, size, 0.0f, 0, WHITE);
            continue;
        }

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
                PushNotification("STONE SHIELD READY!");
            }
            else if (powerups[i].type == POWER_MAGNET)
            {
                magnetTimer = magnet_duration;
                PushNotification("LODESTONE ACTIVE!");
            }
            else
            {
                speedTimer = speed_duration;
                PushNotification("STEAM BOOST READY!");
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
    // Floating pollen and drifting firefly motes reinforce the reclaimed-wilderness mood.
    float offset = fmodf(camX * 0.08f, 260.0f);
    for (int i = -1; i < (int)(width / 260.0f) + 3; i++)
    {
        float x = i * 260.0f - offset;
        float y = 92.0f + 18.0f * sinf((float)i * 1.7f);
        DrawCircle((int)x, (int)y, 4.0f, Fade(ARENA_MINT, 0.48f));
        DrawCircle((int)x, (int)y, 15.0f, Fade(ARENA_MINT, 0.08f));
        DrawLineEx((Vector2){x - 7, y + 11}, (Vector2){x + 3, y + 6}, 2.0f,
                   Fade(ARENA_CREAM, 0.22f));
    }
    DrawRectangleGradientV(0, (int)(height * 0.52f), (int)width, (int)(height * 0.48f),
                           Fade(ARENA_MINT, 0.0f), Fade(ARENA_INK, 0.20f));
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
    {5250, 655, 45, 45, 120, 655, 600, 1, 0},
    {5950, 255, 45, 45, 50, 255, 900, -1, 2},
    // {6000, 450, 45, 45, 100, 450, 160, -1, 3},
    {6100, 655, 50, 45, 100, 655, 1000, -1, 0},
    {6400, 455, 45, 45, 150, 455, 150, 1, 2},
    {6700, 255, 45, 45, 50, 255, 400, -1, 2},
    {8050, 100, 45, 45, 80, 300, 560, 1, 1},
    {8300, 305, 45, 45, 50, 305, 680, 1, 2},
    //{9000, 100, 45, 45, 0, 300, 170, -1, 1},
   // {9300, 100, 45, 45, 0, 300, 160, 1, 1},
    {10200, 100, 45, 45, 80, 300, 600, -1, 1},
    {10800, 655, 45, 45, 150, 655, 700, 1, 0},
    {11200, 655, 45, 45, 120, 655, 400, -1, 0},
    {11900, 100, 45, 45, 80, 300, 700, 1, 1},
    {12300, 500, 45, 45, 100, 500, 900, 1, 2},
    {12900, 100, 45, 45, 80, 300, 500, -1, 1},
    {13400, 100, 45, 45, 80, 300, 1000, 1, 1},
    {13900, 500, 45, 45, 70, 660, 1000, -1, 2},
    {14200, 655, 45, 45, 150, 655, 1000, 1, 0},
    {14800, 655, 45, 45, 100, 655, 1200, -1, 0},
    {15300, 100, 45, 45, 80, 300, 500, 1, 1},
    {15800, 655, 45, 45, 120, 655, 1500, -1, 0},
    {16800, 655, 45, 45, 150, 655, 180, -1, 0},
    {17300, 100, 45, 45, 80, 300, 160, 1, 1},
    {17800, 655, 45, 45, 100, 655, 1070, -1, 0},
    {18300, 100, 45, 45, 80, 300, 580, 1, 1},
    {18800, 655, 45, 45, 150, 655, 500, -1, 0},
    {19300, 100, 45, 45, 80, 300, 1160, 1, 1},
    {19800, 655, 45, 45, 100, 655, 1200, -1, 0},
    {20300, 100, 45, 45, 80, 300, 1000, 1, 1},
    {20800, 500, 45, 45, 150, 500, 1110, -1, 2},
    {21200, 655, 45, 45, 100, 655, 1000, 1, 0},
    {21800, 500, 45, 45, 150, 500, 1000, -1, 2},
    {22600, 655, 45, 45, 100, 655, 190, 1, 0},
    {23000, 100, 45, 45, 80, 300, 180, -1, 1},
    {23600, 500, 45, 45, 150, 500, 160, 1, 2},
    {24300, 655, 45, 45, 100, 655, 200, -1, 0},
    {25000, 500, 45, 45, 150, 500, 1070, 1, 2},
    {25800, 655, 45, 45, 100, 655, 400, -1, 0},
    {26500, 100, 45, 45, 80, 300, 190, 1, 1},
    {27400, 500, 45, 45, 150, 500, 160, -1, 2},
    {28170, 655, 45, 45, 100, 655, 300, 1, 0},
    {28700, 100, 45, 45,80, 300, 1000, -1, 1},
    {29200, 500, 45, 45, 150, 500, 200, 1, 2},
    {30900, 100, 45, 45, 80, 300, 600, 1, 1},
    {31400, 500, 45, 45, 150, 500, 700, -1, 2},
    {32000, 655, 45, 45, 100, 655, 1000, 1, 0},
    {32500, 100, 45, 45, 80, 300, 180, -1, 1},
    {33100, 500, 45, 45, 150, 660, 200, 1, 2},
    {33600, 655, 45, 45, 100, 655, 1160, -1, 0},
    {34200, 100, 45, 45, 80, 300, 500, 1, 1},
    {34800, 500, 45, 45, 150, 500, 170, -1, 2},
    {35500, 655, 45, 45, 100, 655, 580, 1, 0},
    {36200, 100, 45, 45, 80, 300, 1000, -1, 1},
    {37000, 500, 45, 45, 150, 500, 1160, 1, 2},
    {37500, 655, 45, 45, 100, 655, 1000, -1, 0},
    {38200, 100, 45, 45, 80, 300, 190, 1, 1},
    {38800, 500, 45, 45, 150, 500, 170, -1, 2},
    {40100, 100, 45, 45, 80, 300, 200, -1, 1},
    {40800, 500, 45, 45, 150, 500, 1500, 1, 2},
    {41400, 655, 45, 45, 100, 655, 1000, -1, 0},
    {42200, 100, 45, 45, 80, 300, 190, 1, 1},
    {43000, 500, 45, 45, 150, 500, 1600, -1, 2},
    {44000, 655, 45, 45, 100, 655, 180, 1, 0},
    // ===== NOTUN: shesh er dike extra enemy (speed dhire dhire barano, 200 -> 220) =====
    {44650, 100, 45, 45, 0, 300, 200, 1, 1},
    {45250, 655, 45, 45, 100, 655, 210, -1, 0},
    {45450, 500, 45, 45, 150, 500, 1000, 1, 2},
    {45800, 500, 45, 45, 300, 500, 200, -1, 2},
    {46200, 500, 45, 45, 150, 660, 1060, 1, 2},
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
        // Sentry movement is deliberately capped so every pass can be read and dodged.
        float movementSpeed = fminf(spikes[i].speed, sentry_max_speed);
        spikes[i].y += movementSpeed * spikes[i].direction * dt;
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
        // Fast broad-phase check before the fair circular sentry hitbox.
        Rectangle broad = getspikerect(&spikes[i]);
        if (!(ballposition->x + radius > broad.x && ballposition->x - radius < broad.x + broad.width &&
              ballposition->y + radius > broad.y && ballposition->y - radius < broad.y + broad.height))
            continue;
        Vector2 center = {spikes[i].x + spikes[i].width * 0.5f,
                          spikes[i].y + spikes[i].height * 0.5f};
        int hit = CheckCollisionCircles(*ballposition, spike_hit_radius, center, sentry_hit_radius);
        Texture2D activeHazard = hazardTextures[hazardTheme];
        if (activeHazard.id == 0)
        {
            Vector2 p1, p2, p3;
            getspiketriangle(&spikes[i], &p1, &p2, &p3);
            hit = checkcirclecollidestriangle(*ballposition, spike_hit_radius, p1, p2, p3);
        }
        if (hit)
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
                    *health -= sentry_damage;
                    if (*health < 0)
                        *health = 0;
                    *damagetimes = 0.7f;
                    PlaySound(spikeSound); // spike e lagle sound
                    TriggerShake(0.25f, 6.0f);
                    SpawnBurst(*ballposition, 14, ORANGE, 160.0f, 0.4f, 4.0f);
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
        Texture2D activeHazard = hazardTextures[hazardTheme];
        if (activeHazard.id != 0)
        {
            float cx = s->x + s->width * 0.5f;
            float cy = s->y + s->height * 0.5f;
            float pulse = 1.0f + 0.05f * sinf((float)GetTime() * 5.0f + i * 0.45f);
            float h = 62.0f * pulse;
            float w = h * ((float)activeHazard.width / (float)activeHazard.height);
            float rotation = (s->type == 1 || s->type == 3) ? 180.0f : 0.0f;
            DrawCircle((int)cx, (int)cy, 24.0f, Fade(ORANGE, 0.10f));
            DrawSpriteCentered(activeHazard, cx, cy, w, h, rotation, 0, WHITE);
            continue;
        }
        Vector2 p1, p2, p3;
        getspiketriangle(s, &p1, &p2, &p3);
        DrawTriangle(p1, p2, p3, ARENA_PINK);
        DrawLineEx(p1, p2, 2.0f, ARENA_INK);
        DrawLineEx(p2, p3, 2.0f, ARENA_INK);
        DrawLineEx(p3, p1, 2.0f, ARENA_INK);
        Vector2 center = {(p1.x + p2.x + p3.x) / 3.0f, (p1.y + p2.y + p3.y) / 3.0f};
        DrawCircleV(center, 3.0f, ARENA_CREAM);
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
    // ---- early-game enemies: late-game er kichu enemy age niye asha ----
    // ground walker
    {3200, 675, 25, 3000, 3450, 110, 1, 675, 0, 0, 0},
    {7200, 675, 25, 7000, 7300, 120, -1, 675, 0, 0, 0},
    // flyer
   // {9000, 450, 22, 8700, 9050, 140, 1, 450, 65, 0.0f, 1},
    {11000, 420, 22, 10750, 11250, 150, -1, 420, 60, 1.5f, 1},

    // ---- late-game enemies: original hard section ----
    {45400, 675, 25, 45150, 45650, 130, 1, 675, 0, 0, 0},
    {46000, 675, 25, 45700, 46400, 140, -1, 675, 0, 0, 0},
    {46800, 675, 25, 46560, 47050, 150, 1, 675, 0, 0, 0},
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
        Texture2D enemyTexture = (e->kind == 0) ? groundEnemyTexture : flyingEnemyTexture;
        if (enemyTexture.id != 0)
        {
            float w = e->r * (e->kind == 0 ? 4.35f : 4.8f);
            float h = w * ((float)enemyTexture.height / (float)enemyTexture.width);
            DrawSpriteCentered(enemyTexture, e->x, e->y, w, h, 0.0f,
                               e->direction < 0, WHITE);
            continue;
        }
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
            PushNotification("SCIENCE BEACON RESTORED!");
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
        if (checkpointTexture.id != 0)
        {
            Color tint = checkpoints[i].reached ? WHITE : (Color){150, 160, 150, 220};
            DrawSpriteCentered(checkpointTexture, checkpoints[i].x, 400.0f,
                               300.0f, 600.0f, 0.0f, 0, tint);
            if (checkpoints[i].reached)
                DrawCircle((int)checkpoints[i].x, 111, 24, Fade(SKYBLUE, 0.18f));
            continue;
        }
        Color poleColor = checkpoints[i].reached ? ARENA_MINT : ARENA_INK;
        Color flagColor = checkpoints[i].reached ? (Color){83, 232, 175, 255} : ARENA_PINK;
        DrawRectangle((int)checkpoints[i].x - 5, 100, 10, 600, poleColor);
        DrawRectangle((int)checkpoints[i].x - 2, 100, 3, 600, Fade(ARENA_CREAM, 0.65f));
        Vector2 f1 = {checkpoints[i].x + 4, 110};
        Vector2 f2 = {checkpoints[i].x + 4, 150};
        Vector2 f3 = {checkpoints[i].x + 50, 130};
        DrawTriangle(f1, f2, f3, flagColor);
        DrawCircle((int)checkpoints[i].x, 100, 9, ARENA_CREAM);
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
    DrawRectangle(0, 0, width, height, Fade(ARENA_INK, 0.88f));
    DrawCircleLines(width / 2 - 66, 76, 24, SCIENCE_COPPER);
    DrawCircle(width / 2 - 66, 76, 6, titlecolor);
    DrawLineEx((Vector2){width / 2 - 66, 49}, (Vector2){width / 2 - 66, 103}, 3.0f, SCIENCE_COPPER);
    DrawLineEx((Vector2){width / 2 - 93, 76}, (Vector2){width / 2 - 39, 76}, 3.0f, SCIENCE_COPPER);
    DrawCircle(width / 2 + 10, 76, 10, titlecolor);
    DrawCircleLines(width / 2 + 58, 76, 18, SCIENCE_COPPER);
    DrawTextCentered(title, height / 2 - 235, 52, ARENA_CREAM);
    DrawTextCentered(TextFormat("EXPLORER  %s", playername), height / 2 - 170, 24, Fade(ARENA_CREAM, 0.78f));
    DrawTextCentered(TextFormat("FINAL SCORE  %d", gamescore), height / 2 - 132, 30, WHITE);
    if (newrecord)
    {
        DrawTextCentered("NEW DISCOVERY RECORD", height / 2 - 91, 27, GOLD);
    }

    // leaderboard box
    int boxw = 320;
    int boxx = (width - boxw) / 2;
    int boxy = height / 2 - 25;
    DrawGlassPanel((Rectangle){boxx, boxy, boxw, 130}, titlecolor);
    DrawTextCentered("HALL OF INVENTORS", boxy + 7, 20, ARENA_CREAM);
    for (int i = 0; i < leaderboard_size; i++)
    {
        Color rowc = (i == 0) ? GOLD : Fade(ARENA_CREAM, 0.82f);
        DrawText(TextFormat("%d. %-10s %6d", i + 1, leaderboard[i].name, leaderboard[i].score), boxx + 15, boxy + 30 + i * 20, 18, rowc);
    }

    DrawTextCentered("ENTER  RESTART EXPERIMENT", boxy + 152, 22, ARENA_CREAM);
    if (!gamewon && lastCheckpointX > 250.0f)
    {
        DrawTextCentered(TextFormat("C  RESTORE AT BEACON %.0f", lastCheckpointX), boxy + 181, 20, ARENA_MINT);
    }
    DrawTextCentered("M  MAIN MENU     ESC  EXIT", boxy + 213, 20, Fade(ARENA_CREAM, 0.70f));
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
        if (bombLauncherTexture.id != 0)
        {
            float h = 92.0f;
            float w = h * ((float)bombLauncherTexture.width / (float)bombLauncherTexture.height);
            DrawLineEx((Vector2){dx, 100}, (Vector2){dx, 155}, 3.0f, Fade(BROWN, 0.70f));
            DrawSpriteCentered(bombLauncherTexture, dx, 135, w, h, 0.0f, 0, WHITE);
            continue;
        }
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
            if (bombTexture.id != 0)
            {
                DrawSpriteCentered(bombTexture, b->x, b->y, 50.0f, 53.0f,
                                   b->y * 0.32f, 0, WHITE);
                continue;
            }
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

    InitWindow(width, height, "Stonebound Science - Wilderness Expedition");
    menuBackground = LoadTexture("assets/stone-age-science/menu-background.png");
    gameplayBackground = LoadTexture("assets/stone-age-science/gameplay-background.png");
    playerToken = LoadTexture("assets/stone-age-science/player-orb.png");
    platformBlockTexture = LoadTexture("assets/stone-age-science/platform-block.png");
    platformTileTexture = LoadTexture("assets/stone-age-science/platform-tile.png");
    scienceTokenTexture = LoadTexture("assets/stone-age-science/science-token.png");
    scienceCrystalTexture = LoadTexture("assets/stone-age-science/science-crystal.png");
    powerTextures[POWER_SHIELD] = LoadTexture("assets/stone-age-science/power-shield.png");
    powerTextures[POWER_MAGNET] = LoadTexture("assets/stone-age-science/power-magnet.png");
    powerTextures[POWER_SPEED] = LoadTexture("assets/stone-age-science/power-speed.png");
    checkpointTexture = LoadTexture("assets/stone-age-science/checkpoint-totem.png");
    bombTexture = LoadTexture("assets/stone-age-science/clay-bomb.png");
    bombLauncherTexture = LoadTexture("assets/stone-age-science/bomb-launcher.png");
    groundEnemyTexture = LoadTexture("assets/stone-age-science/enemy-ground.png");
    flyingEnemyTexture = LoadTexture("assets/stone-age-science/enemy-flying.png");
    hazardTextures[0] = LoadTexture("assets/stone-age-science/hazard-stone.png");
    hazardTextures[1] = LoadTexture("assets/stone-age-science/hazard-crystal.png");
    hazardTextures[2] = LoadTexture("assets/stone-age-science/hazard-steam.png");

    Texture2D *artTextures[] = {
        &menuBackground, &gameplayBackground, &playerToken,
        &platformBlockTexture, &platformTileTexture,
        &scienceTokenTexture, &scienceCrystalTexture,
        &powerTextures[0], &powerTextures[1], &powerTextures[2],
        &checkpointTexture, &bombTexture, &bombLauncherTexture,
        &groundEnemyTexture, &flyingEnemyTexture,
        &hazardTextures[0], &hazardTextures[1], &hazardTextures[2]};
    const char *artNames[] = {
        "menu background", "gameplay background", "player orb",
        "platform block", "platform tile", "science token", "science crystal",
        "shield power-up", "lodestone power-up", "steam power-up",
        "checkpoint totem", "clay bomb", "bomb launcher",
        "ground enemy", "flying enemy",
        "stone hazard", "crystal hazard", "steam hazard"};
    int artTextureCount = (int)(sizeof(artTextures) / sizeof(artTextures[0]));
    for (int i = 0; i < artTextureCount; i++)
    {
        if (artTextures[i]->id != 0)
            SetTextureFilter(*artTextures[i], TEXTURE_FILTER_BILINEAR);
        else
            TraceLog(LOG_WARNING, "Could not load generated %s", artNames[i]);
    }
    InitAudioDevice();

    // >>>>>>> NOTUN: sob theke gurutwopurno check - audio device e ki ashole ready hoyeche? >>>>>>>
    // ei check ta fail korle, code e kono bhul nai - tomar system/environment e
    // kono audio driver/speaker/output e pawa jacche na (WSL, remote server, Docker,
    // virtual machine e eta k common - shegulote by default sound card thake na)
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
    bgMusic = LoadMusicStream(FileExists("assert/anik.mp3") ? "assert/anik.mp3" : "music.mp3/music.mp3");
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
        if (menuScreen && menuPage == MENU_HOME)
        {
            UpdateNameInput();
        }

        Rectangle menuPlayButton = GetMenuPlayButton((int)width, (int)height);
        Rectangle menuHowButton = GetMenuHowButton();
        Rectangle menuCreditsButton = GetMenuCreditsButton();
        Rectangle menuBackButton = GetMenuBackButton();
        Vector2 menuMouse = GetMousePosition();
        int menuPlayClicked = menuScreen && menuPage == MENU_HOME && namelen > 0 &&
                              PointInside(menuMouse, menuPlayButton) &&
                              IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        if (menuScreen && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (menuPage == MENU_HOME && PointInside(menuMouse, menuHowButton))
                menuPage = MENU_HOW_TO_PLAY;
            else if (menuPage == MENU_HOME && PointInside(menuMouse, menuCreditsButton))
                menuPage = MENU_CREDITS;
            else if (menuPage != MENU_HOME && PointInside(menuMouse, menuBackButton))
                menuPage = MENU_HOME;
        }
        if (menuScreen && menuPage != MENU_HOME && IsKeyPressed(KEY_BACKSPACE))
            menuPage = MENU_HOME;

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
        if ((IsKeyPressed(KEY_ENTER) && (!menuScreen || menuPage == MENU_HOME)) || menuPlayClicked)
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
            hazardThemeTimer = 0.0f;
            hazardTheme = 0;
            menuPage = MENU_HOME;
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
            PushNotification("RESTORED AT SCIENCE BEACON");
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
            hazardThemeTimer += dt;
            int nextHazardTheme = ((int)(hazardThemeTimer / hazard_theme_duration)) % 3;
            if (nextHazardTheme != hazardTheme)
            {
                static const char *hazardNames[] = {"STONE FANGS", "CRYSTAL CRAWLERS", "STEAM BEETLES"};
                hazardTheme = nextHazardTheme;
                PushNotification(TextFormat("HAZARDS EVOLVED: %s", hazardNames[hazardTheme]));
            }
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

            // Distance er sathe ball aste aste faster hobe.
            // Start = 1.0x, finish er dike = maximum 1.5x.
            float progress = maxdistance / (float)win_x;
            if (progress < 0.0f)
                progress = 0.0f;
            if (progress > 1.0f)
                progress = 1.0f;

            float distanceSpeedMultiplier = 1.0f + 0.5f * progress;

            // Speed power-up thakleo final speed 1.5x er beshi jabe na.
            float speedMultiplier = distanceSpeedMultiplier;
            if (speedTimer > 0)
                speedMultiplier *= 1.6f;
            if (speedMultiplier > 1.5f)
                speedMultiplier = 1.5f;

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
        ClearBackground(ARENA_INK);
        if (menuScreen)
        {
            if (menuBackground.id != 0)
                DrawTextureCover(menuBackground, (Rectangle){0, 0, width, height}, WHITE);
            else
                DrawRectangleGradientV(0, 0, width, height, ARENA_INK, ARENA_PINK_DARK);
            DrawRectangleGradientV(0, 0, width, height, Fade(ARENA_INK, 0.18f), Fade(ARENA_INK, 0.58f));
        }
        else
        {
            if (gameplayBackground.id != 0)
                DrawTextureCover(gameplayBackground, (Rectangle){0, 0, width, height}, WHITE);
            else
                DrawRectangleGradientV(0, 0, width, height, skytop, skybottom);
            DrawRectangle(0, 0, (int)width, (int)height, Fade(skytop, 0.10f));
            DrawParallaxBackground(camera.target.x, width, height);
        }

        if (!menuScreen)
        {
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
            DrawArenaBlock(gx, gy, brickwidth, brickheight);
        }
        // ceiling
        for (int j = 0; j < 400; j++)
        {
            float cx = brickwidth * j;
            DrawArenaBlock(cx, 0, brickwidth, brickheight);
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
                    DrawArenaBlock(dx, dy, platformbrick_width, platformbrick_height);
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
        // generated contestant token (physics still uses the original circular hitbox)
        DrawEllipse((int)ballposition.x, (int)(ballposition.y + radius * 0.82f), radius * 0.82f, radius * 0.28f, Fade(ARENA_INK, 0.28f));
        if (playerToken.id != 0)
        {
            float spriteSize = radius * 3.15f;
            DrawTexturePro(playerToken,
                           (Rectangle){0, 0, (float)playerToken.width, (float)playerToken.height},
                           (Rectangle){ballposition.x, ballposition.y, spriteSize, spriteSize},
                           (Vector2){spriteSize * 0.5f, spriteSize * 0.5f}, ballrotation, WHITE);
        }
        else
        {
            DrawCircle((int)ballposition.x, (int)ballposition.y, radius, (Color){55, 68, 58, 255});
        }
        // NOTUN: particle gulo ball er upore ashe
        DrawParticles();
        EndMode2D();
        }

        // ---------- main menu ----------
        if (!gamestarted && !gameover && !gamepaused && !gamewon)
        {
            DrawCircleLines(107, 79, 24, SCIENCE_COPPER);
            DrawCircle(107, 79, 7, ARENA_PINK);
            DrawLineEx((Vector2){79, 79}, (Vector2){135, 79}, 3.0f, SCIENCE_COPPER);
            DrawLineEx((Vector2){107, 51}, (Vector2){107, 107}, 3.0f, SCIENCE_COPPER);
            DrawCircle(158, 79, 11, ARENA_MINT);
            DrawText("STONEBOUND SCIENCE", 82, 108, 54, ARENA_CREAM);
            DrawText("REBUILD THE WORLD // ONE EXPERIMENT AT A TIME", 86, 166, 19, Fade(ARENA_CREAM, 0.76f));

            if (menuPage == MENU_HOME)
            {
                Rectangle intakePanel = {70, 205, 566, 380};
                DrawGlassPanel(intakePanel, ARENA_PINK);
                DrawText("EXPLORER REGISTRATION", 96, 232, 24, ARENA_CREAM);
                DrawText("NAME YOUR SCIENTIST", 96, 282, 17, Fade(ARENA_CREAM, 0.62f));

                Rectangle nameBox = {96, 310, 500, 58};
                DrawRectangleRounded(nameBox, 0.08f, 8, Fade(BLACK, 0.64f));
                DrawRectangleRoundedLinesEx(nameBox, 0.08f, 8, 2.0f,
                                            namelen > 0 ? ARENA_MINT : Fade(ARENA_CREAM, 0.45f));
                const char *cursor = (((int)(GetTime() * 2)) % 2 == 0) ? "_" : "";
                DrawText(TextFormat("%s%s", playername, cursor), 115, 325, 28, ARENA_CREAM);

                Rectangle playButton = GetMenuPlayButton((int)width, (int)height);
                int playHover = namelen > 0 && PointInside(GetMousePosition(), playButton);
                Color buttonColor = namelen > 0 ? (playHover ? (Color){49, 213, 210, 255} : ARENA_PINK)
                                                 : Fade(GRAY, 0.72f);
                DrawRectangleRounded(playButton, 0.12f, 8, buttonColor);
                DrawTextInRectCentered(namelen > 0 ? "BEGIN EXPEDITION" : "SCIENTIST NAME REQUIRED",
                                       playButton, 24, namelen > 0 ? ARENA_INK : Fade(WHITE, 0.60f));

                int howHover = PointInside(GetMousePosition(), menuHowButton);
                int creditsHover = PointInside(GetMousePosition(), menuCreditsButton);
                DrawRectangleRounded(menuHowButton, 0.12f, 8,
                                     howHover ? SCIENCE_COPPER : Fade(SCIENCE_COPPER, 0.82f));
                DrawRectangleRounded(menuCreditsButton, 0.12f, 8,
                                     creditsHover ? ARENA_MINT : Fade(ARENA_MINT, 0.82f));
                DrawTextInRectCentered("HOW TO PLAY", menuHowButton, 20, ARENA_CREAM);
                DrawTextInRectCentered("CREDITS", menuCreditsButton, 20, ARENA_INK);
                DrawText("CLICK OR PRESS ENTER TO START", 190, 542, 16, Fade(ARENA_CREAM, 0.58f));

                Rectangle scoresPanel = {1180, 205, 350, 294};
                DrawGlassPanel(scoresPanel, ARENA_MINT);
                DrawText("HALL OF INVENTORS", 1210, 233, 23, ARENA_CREAM);
                DrawLineEx((Vector2){1210, 271}, (Vector2){1500, 271}, 1.0f, Fade(ARENA_MINT, 0.45f));
                for (int i = 0; i < leaderboard_size; i++)
                {
                    Color row = (i == 0) ? GOLD : Fade(ARENA_CREAM, 0.82f);
                    DrawText(TextFormat("%02d", i + 1), 1212, 292 + i * 36, 19, row);
                    DrawText(leaderboard[i].name, 1260, 292 + i * 36, 19, row);
                    const char *scoreText = TextFormat("%d", leaderboard[i].score);
                    DrawText(scoreText, 1494 - MeasureText(scoreText, 19), 292 + i * 36, 19, row);
                }

                Rectangle controlsPanel = {380, 650, 840, 68};
                DrawRectangleRounded(controlsPanel, 0.08f, 8, Fade(ARENA_INK, 0.82f));
                DrawText("MOVE", 420, 674, 18, ARENA_PINK);
                DrawText("LEFT / RIGHT", 480, 674, 18, ARENA_CREAM);
                DrawText("JUMP", 665, 674, 18, ARENA_PINK);
                DrawText("SPACE x2", 725, 674, 18, ARENA_CREAM);
                DrawText("PAUSE", 870, 674, 18, ARENA_PINK);
                DrawText("P", 941, 674, 18, ARENA_CREAM);
                DrawText("SOUND", 1010, 674, 18, ARENA_PINK);
                DrawText("N", 1082, 674, 18, ARENA_CREAM);
            }
            else
            {
                Rectangle infoPanel = {350, 190, 900, 400};
                DrawGlassPanel(infoPanel, menuPage == MENU_HOW_TO_PLAY ? ARENA_PINK : SCIENCE_COPPER);
                if (menuPage == MENU_HOW_TO_PLAY)
                {
                    DrawText("HOW TO PLAY", 390, 222, 36, ARENA_CREAM);
                    DrawText("LEFT / RIGHT", 400, 286, 20, ARENA_PINK);
                    DrawText("Move through the reclaimed world", 610, 286, 20, ARENA_CREAM);
                    DrawText("SPACE x2", 400, 330, 20, ARENA_PINK);
                    DrawText("Jump and double-jump", 610, 330, 20, ARENA_CREAM);
                    DrawText("P / N", 400, 374, 20, ARENA_PINK);
                    DrawText("Pause / toggle sound", 610, 374, 20, ARENA_CREAM);
                    DrawText("Collect bronze science tokens and rare cyan crystals.", 400, 430, 20, Fade(ARENA_CREAM, 0.88f));
                    DrawText("Use the stone shield, lodestone, and steam turbine power-ups.", 400, 466, 20, Fade(ARENA_CREAM, 0.88f));
                    DrawText("Restore beacons. Avoid beasts, bombs, and moving hazards.", 400, 502, 20, Fade(ARENA_CREAM, 0.88f));
                    DrawText("TRY TO AVOID THE MOVING THINGS    .", 400, 538, 20, ARENA_MINT);
                }
                else
                {
                    DrawText("CREDITS", 390, 222, 36, ARENA_CREAM);
                    DrawText("DESIGN & PROGRAMMING", 400, 294, 20, SCIENCE_COPPER);
                    DrawText("Original raylib game project", 720, 294, 20, ARENA_CREAM);
                    DrawText("VISUAL DIRECTION", 400, 344, 20, SCIENCE_COPPER);
                    DrawText("Stone-age science anime adventure", 720, 344, 20, ARENA_CREAM);
                    DrawText("ART ASSETS", 400, 394, 20, SCIENCE_COPPER);
                    DrawText("FROM DR STONE", 720, 394, 20, ARENA_CREAM);
                    DrawText("ENGINE", 400, 444, 20, SCIENCE_COPPER);
                    DrawText("raylib", 720, 444, 20, ARENA_CREAM);
                    DrawText("SPECIAL THANKS", 400, 494, 20, SCIENCE_COPPER);
                    DrawText("HABIB.2505105 AND SAKIB 2505104", 720, 494, 20, ARENA_CREAM);
                }

                int backHover = PointInside(GetMousePosition(), menuBackButton);
                DrawRectangleRounded(menuBackButton, 0.12f, 8,
                                     backHover ? (Color){49, 213, 210, 255} : ARENA_PINK);
                DrawTextInRectCentered("BACK TO MAIN MENU", menuBackButton, 20, ARENA_INK);
            }
        }
        // ---------- pause menu ----------
        if (gamepaused && !gameover && !gamewon)
        {
            DrawRectangle(0, 0, width, height, Fade(ARENA_INK, 0.82f));
            DrawGlassPanel((Rectangle){width / 2 - 250, height / 2 - 120, 500, 240}, ARENA_PINK);
            DrawTextCentered("EXPERIMENT PAUSED", height / 2 - 76, 42, ARENA_CREAM);
            DrawTextCentered("P  RESUME EXPEDITION", height / 2 + 12, 23, ARENA_MINT);
            DrawTextCentered("N  TOGGLE SOUND", height / 2 + 49, 18, Fade(ARENA_CREAM, 0.64f));
        }
        // ---------- game over screen ----------
        if (gameover)
        {
            DrawEndScreen("PETRIFIED", SCIENCE_COPPER, width, height);
        }
        // ---------- win screen ----------
        if (gamewon)
        {
            DrawEndScreen("KINGDOM RESTORED", ARENA_MINT, width, height);
        }
        // ---------- HUD ----------
       if (gamestarted || gamepaused)
{
    int hudX = (int)width - 390;
    int hudY = 18;

    Rectangle hudPanel = {hudX, hudY, 370, 156};

    DrawRectangleRounded(
        hudPanel,
        0.06f,
        8,
        Fade(ARENA_INK, 0.86f)
    );

    DrawRectangleRoundedLinesEx(
        hudPanel,
        0.06f,
        8,
        1.5f,
        Fade(ARENA_MINT, 0.52f)
    );

    // Player name
    DrawText(
        TextFormat("EXPLORER  %s", playername),
        hudX + 18,
        hudY + 16,
        18,
        ARENA_CREAM
    );

    // Score
    DrawText(
        TextFormat("SCORE  %06d", gamescore),
        hudX + 18,
        hudY + 73,
        26,
        WHITE
    );

    // Record
    DrawText(
        TextFormat("RECORD  %06d", highscore),
        hudX + 18,
        hudY + 108,
        17,
        Fade(ARENA_CREAM, 0.70f)
    );

    // Health bar background
    DrawRectangle(
        hudX + 18,
        hudY + 47,
        330,
        13,
        Fade(BLACK, 0.60f)
    );

    // Health bar
    DrawRectangle(
        hudX + 18,
        hudY + 47,
        (int)(330 * (health / maxhealth)),
        13,
        health > 35
            ? (Color){83, 232, 175, 255}
            : ARENA_PINK
    );

    // Life
    DrawText(
        TextFormat("LIFE %.0f", health),
        hudX + 264,
        hudY + 25,
        16,
        Fade(ARENA_CREAM, 0.76f)
    );

    // Progress bar
    float progress = fminf(ballposition.x / win_x, 1.0f);

    DrawRectangle(
        hudX,
        hudY + 163,
        370,
        8,
        Fade(ARENA_INK, 0.75f)
    );

    DrawRectangle(
        hudX,
        hudY + 163,
        (int)(370 * progress),
        8,
        ARENA_PINK
    );

            // NOTUN: combo counter
            if (comboCount > 0)
            {
                int mult = 1 + comboCount / 5;
                if (mult > combo_max_mult)
                    mult = combo_max_mult;
                DrawText(TextFormat("COMBO x%d", mult), 405, 24, 22, GOLD);
            }

            // NOTUN: active powerup timer bar (upper right)
            int py = 20;
            int px = (int)width - 230;
            if (shieldTimer > 0)
            {
                DrawRectangleRounded((Rectangle){px, py, 210, 30}, 0.10f, 6, Fade((Color){75, 180, 255, 255}, 0.86f));
                DrawText(TextFormat("SHIELD   %.1fs", shieldTimer), px + 12, py + 6, 18, WHITE);
                py += 36;
            }
            if (magnetTimer > 0)
            {
                DrawRectangleRounded((Rectangle){px, py, 210, 30}, 0.10f, 6, Fade(ARENA_PINK, 0.86f));
                DrawText(TextFormat("MAGNET   %.1fs", magnetTimer), px + 12, py + 6, 18, WHITE);
                py += 36;
            }
            if (speedTimer > 0)
            {
                DrawRectangleRounded((Rectangle){px, py, 210, 30}, 0.10f, 6, Fade(GOLD, 0.88f));
                DrawText(TextFormat("STEAM    %.1fs", speedTimer), px + 12, py + 6, 18, ARENA_INK);
                py += 36;
            }
            static const char *hazardHudNames[] = {"STONE", "CRYSTAL", "STEAM"};
            float hazardTimeRemaining = hazard_theme_duration - fmodf(hazardThemeTimer, hazard_theme_duration);
            DrawRectangleRounded((Rectangle){(int)width - 292, 126, 272, 38}, 0.10f, 6,
                                 Fade(ARENA_INK, 0.82f));
            DrawText(TextFormat("HAZARD  %s  %.0fs", hazardHudNames[hazardTheme], hazardTimeRemaining),
                     (int)width - 276, 136, 17, ARENA_MINT);
            DrawText(muted ? "N  SOUND OFF" : "N  SOUND ON", (int)width - 170, (int)height - 32, 18, ARENA_CREAM);
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
    for (int i = 0; i < artTextureCount; i++)
    {
        if (artTextures[i]->id != 0)
            UnloadTexture(*artTextures[i]);
    }

    CloseAudioDevice();
    CloseWindow();
    return 0;
}
