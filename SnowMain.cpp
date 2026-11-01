#include "plugin.h"
#include "common.h"
#include "CSprite2d.h"
#include "CWeather.h"
#include "CGame.h"
#include "CTimer.h"
#include "CClock.h"
#include "CVehicle.h"
#include "CPlayerPed.h"
#include "CWorld.h"
#include "CColPoint.h"
#include "CEntity.h"
#include <cmath>
#include <cstdlib>
#include <windows.h>

using namespace plugin;

struct Flake { float x, y, speed, size, drift, freq, phase, alpha; };

static const int MAX_FLAKES = 360;
static Flake flakes[MAX_FLAKES];
static bool inited = false;
static bool enabled = false;
static bool keyWasDown = false;
static float intensity = 1.0f;
static float timeSec = 0.0f;
static float shelter = 0.0f;
static int roofTimer = 0;
static bool roofHit = false;

static float Rnd() { return rand() / (float)RAND_MAX; }
static float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
static float phaseEnd = 0.0f;
static float phaseTarget = 1.0f;
static int lastPhase = 1;

static void NextPhase() {
    int p;
    float r = Rnd();
    if (r < 0.30f) p = 0;
    else if (r < 0.75f) p = 1;
    else p = 2;
    if (p == lastPhase) p = 1;
    if (p == lastPhase) p = 0;
    lastPhase = p;

    if (p == 0) {
        phaseTarget = 0.5f;
        phaseEnd = timeSec + 40.0f + Rnd() * 60.0f;
    } else if (p == 1) {
        phaseTarget = 1.0f;
        phaseEnd = timeSec + 30.0f + Rnd() * 60.0f;
    } else {
        phaseTarget = 2.0f;
        phaseEnd = timeSec + 15.0f + Rnd() * 20.0f;
    }
}
static void Respawn(Flake &f, bool anywhere, float W, float H) {
    float r = Rnd();
    f.size = 1.1f + r * r * 3.4f;
    f.x = Rnd() * (W + 200.0f) - 100.0f;
    f.y = anywhere ? Rnd() * H : -20.0f - Rnd() * 60.0f;
    f.speed = 110.0f + f.size * 65.0f;
    f.drift = 10.0f + Rnd() * 24.0f;
    f.freq = 0.6f + Rnd() * 1.4f;
    f.phase = Rnd() * 6.28f;
    float a = 110.0f + f.size * 28.0f;
    f.alpha = a > 235.0f ? 235.0f : a;
}

static void DrawDot(float x, float y, float s, float stretch, float a, float bright) {
    unsigned char R = (unsigned char)(235.0f * bright);
    unsigned char G = (unsigned char)(242.0f * bright);
    unsigned char B = (unsigned char)(255.0f * bright);
    float sy = s * stretch;
    float g = s * 2.0f;
    float gy = sy * 2.0f;
    unsigned char ga = (unsigned char)(a * 0.16f);
    unsigned char ca = (unsigned char)a;

    CSprite2d::DrawRect(CRect(x - g, y + gy * 0.7f, x + g, y - gy * 0.7f), CRGBA(R, G, B, ga));
    CSprite2d::DrawRect(CRect(x - g * 0.7f, y + gy, x + g * 0.7f, y - gy), CRGBA(R, G, B, ga));
    CSprite2d::DrawRect(CRect(x - s, y + sy * 0.7f, x + s, y - sy * 0.7f), CRGBA(R, G, B, ca));
    CSprite2d::DrawRect(CRect(x - s * 0.7f, y + sy, x + s * 0.7f, y - sy), CRGBA(R, G, B, ca));
}

static bool HasRoofAbove() {
    CPlayerPed *p = FindPlayerPed(-1);
    if (!p) return false;
    CVector from = p->GetPosition();
    CVector to = from;
    to.z += 25.0f;
    CColPoint col;
    CEntity *hit = nullptr;
    return CWorld::ProcessLineOfSight(from, to, col, hit, true, false, false, true, true, false, false, false);
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

    roofTimer++;
    if (roofTimer >= 3) {
        roofTimer = 0;
        roofHit = HasRoofAbove();
    }
    if (roofHit) {
        shelter += dt * 3.0f;
        if (shelter > 1.0f) shelter = 1.0f;
    } else {
        shelter -= dt * 2.2f;
        if (shelter < 0.0f) shelter = 0.0f;
    }
    float open = 1.0f - Clamp01(shelter);
    if (open < 0.02f) return;

    float speedVal = 0.0f;
    CVehicle *veh = FindPlayerVehicle(-1, false);
    if (veh) speedVal = veh->m_vecMoveSpeed.Magnitude();

    if (timeSec >= phaseEnd) NextPhase();
    intensity += (phaseTarget - intensity) * dt * 0.4f;

    float wind = 0.18f * sinf(timeSec / 20.0f) + 0.12f * sinf(timeSec / 7.3f + 1.3f);
    float fallMul = 1.0f + speedVal * 1.5f + (intensity - 1.0f) * 0.5f;
    int active = (int)(170.0f * intensity);
    if (active > MAX_FLAKES) active = MAX_FLAKES;

    float stretch = 1.0f + speedVal * 3.0f;
    if (stretch > 2.8f) stretch = 2.8f;

    int hour = CClock::ms_nGameClockHours;
    float bright = 1.0f;
    if (hour >= 21 || hour < 5) bright = 0.55f;
    else if (hour < 7 || hour >= 19) bright = 0.8f;

    float scale = H / 1080.0f;

    for (int i = 0; i < active; i++) {
        Flake &f = flakes[i];
        f.y += f.speed * fallMul * scale * dt;
        f.x += (wind * f.speed * scale + sinf(f.phase + timeSec * f.freq) * f.drift * scale) * dt;

        if (f.y > H + 20.0f || f.x < -150.0f || f.x > W + 150.0f)
            Respawn(f, false, W, H);

        float fadeIn = Clamp01(f.y / (H * 0.08f));
        float fadeOut = Clamp01((H - f.y) / (H * 0.10f));
        float a = f.alpha * fadeIn * fadeOut * open;
        if (a < 2.0f) continue;

        DrawDot(f.x, f.y, f.size * scale, stretch, a, bright);
    }
}

class SnowPlugin {
public:
    SnowPlugin() {
        Events::drawingEvent += SnowDraw;
    }
} snowPlugin;
