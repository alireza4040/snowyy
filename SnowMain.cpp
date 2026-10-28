#include "plugin.h"
#include "common.h"
#include "CSprite2d.h"
#include "CWeather.h"
#include "CGame.h"
#include "CTimer.h"
#include "CVehicle.h"
#include <cmath>
#include <cstdlib>
#include <windows.h>

using namespace plugin;

struct Flake { float x, y, speed, size, drift, freq, phase, alpha; };

static const int MAX_FLAKES = 260;
static Flake flakes[MAX_FLAKES];
static bool inited = false;
static bool enabled = false;
static bool keyWasDown = false;
static float intensity = 1.0f;
static float timeSec = 0.0f;

static float Rnd() { return rand() / (float)RAND_MAX; }

static void Respawn(Flake &f, bool anywhere, float W, float H) {
    float r = Rnd();
    f.size = 0.8f + r * r * 2.8f;
    f.x = Rnd() * (W + 200.0f) - 100.0f;
    f.y = anywhere ? Rnd() * H : -10.0f - Rnd() * 40.0f;
    f.speed = 140.0f + f.size * 90.0f;
    f.drift = 8.0f + Rnd() * 22.0f;
    f.freq = 0.6f + Rnd() * 1.4f;
    f.phase = Rnd() * 6.28f;
    float a = 110.0f + f.size * 35.0f;
    f.alpha = a > 235.0f ? 235.0f : a;
}

static void SnowDraw() {
    float W = SCREEN_WIDTH;
    float H = SCREEN_HEIGHT;

    bool down = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    if (down && !keyWasDown) {
        enabled = !enabled;
        if (!enabled) CWeather::ForcedWeatherType = -1;
    }
    keyWasDown = down;

    if (!enabled) return;

    CWeather::ForcedWeatherType = 21;
    CWeather::NewWeatherType = 21;
    CWeather::OldWeatherType = 21;

    if (!inited) {
        for (int i = 0; i < MAX_FLAKES; i++) Respawn(flakes[i], true, W, H);
        inited = true;
    }

    if (CGame::currArea != 0) return;

    float dt = CTimer::ms_fTimeStep / 50.0f;
    timeSec += dt;

    float speedVal = 0.0f;
    CVehicle *veh = FindPlayerVehicle(-1, false);
    if (veh) speedVal = veh->m_vecMoveSpeed.Magnitude();

    float cycle = fmodf(timeSec, 180.0f);
    float target = (cycle < 25.0f) ? 2.0f : 1.0f;
    intensity += (target - intensity) * dt * 0.4f;

    float wind = 0.25f * sinf(timeSec / 20.0f);
    float fallMul = 1.0f + speedVal * 1.5f + (intensity - 1.0f) * 0.5f;
    int active = (int)(120.0f * intensity);
    if (active > MAX_FLAKES) active = MAX_FLAKES;

    float scale = H / 1080.0f;

    for (int i = 0; i < active; i++) {
        Flake &f = flakes[i];
        f.y += f.speed * fallMul * scale * dt;
        f.x += (wind * f.speed * scale + sinf(f.phase + timeSec * f.freq) * f.drift * scale) * dt;

        if (f.y > H + 10.0f || f.x < -150.0f || f.x > W + 150.0f)
            Respawn(f, false, W, H);

        float s = f.size * scale;
        CSprite2d::DrawRect(CRect(f.x - s, f.y + s, f.x + s, f.y - s),
                            CRGBA(235, 240, 255, (unsigned char)f.alpha));
    }
}

class SnowPlugin {
public:
    SnowPlugin() {
        Events::drawingEvent += SnowDraw;
    }
} snowPlugin;
