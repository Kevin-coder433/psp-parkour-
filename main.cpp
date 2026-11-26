#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>

#ifdef __PSP__
#include <pspkernel.h>
#include <pspctrl.h>
PSP_MODULE_INFO("GTA_Grid_3D", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
#endif

struct Vec3 {
    float x, y, z;
};

struct Camera {
    Vec3 target{0.0f, 1.0f, 0.0f};
    float yaw = 0.0f;
    float pitch = 0.35f;
    float distance = 8.0f;
};

struct Character {
    Vec3 pos{0.0f, 2.5f, 0.0f};
    Vec3 vel{0.0f, 0.0f, 0.0f};
    float facingYaw = 0.0f;
    float halfWidth = 0.35f;
    float height = 1.4f;
    bool onGround = false;
};

struct Button {
    SDL_FRect rect;
    bool pressed = false;
};

Vec3 getCameraPos(const Camera& cam) {
    float hDist = cam.distance * std::cos(cam.pitch);
    float vDist = cam.distance * std::sin(cam.pitch);
    return {
        cam.target.x - std::sin(cam.yaw) * hDist,
        cam.target.y + vDist,
        cam.target.z - std::cos(cam.yaw) * hDist
    };
}

bool project3D(const Vec3& p, const Vec3& camPos,
               float yaw, float pitch, int w, int h,
               SDL_FPoint& out) {
    float dx = p.x - camPos.x;
    float dy = p.y - camPos.y;
    float dz = p.z - camPos.z;

    float cosY = std::cos(yaw);
    float sinY = std::sin(yaw);
    float rx = dx * cosY - dz * sinY;
    float rz = dx * sinY + dz * cosY;

    float cosP = std::cos(pitch);
    float sinP = std::sin(pitch);
    float ry2 = dy * cosP - rz * sinP;
    float rz2 = dy * sinP + rz * cosP;

    if (rz2 <= 0.15f) return false;

    float fov = (w < h ? w : h) * 0.95f;
    out.x = (rx * fov) / rz2 + (w * 0.5f);
    out.y = (-ry2 * fov) / rz2 + (h * 0.5f);
    return true;
}

void drawLine3D(SDL_Renderer* r, const Vec3& a, const Vec3& b,
                const Vec3& camPos, float yaw, float pitch,
                int w, int h) {
    SDL_FPoint p1, p2;
    if (project3D(a, camPos, yaw, pitch, w, h, p1) &&
        project3D(b, camPos, yaw, pitch, w, h, p2)) {
        SDL_RenderDrawLineF(r, p1.x, p1.y, p2.x, p2.y);
    }
}

void drawBlenderGrid(SDL_Renderer* r, const Vec3& center,
                     const Vec3& camPos, float yaw, float pitch,
                     int w, int h) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    const int radius = 24;
    const int step = 2;
    int sx = (int)std::floor(center.x) - radius;
    int ex = (int)std::floor(center.x) + radius;
    int sz = (int)std::floor(center.z) - radius;
    int ez = (int)std::floor(center.z) + radius;

    for (int x = sx; x <= ex; ++x) {
        bool major = (x % 5 == 0);
        for (int z = sz; z < ez; z += step) {
            float d = std::hypot(x - center.x, z - center.z);
            float fade = std::clamp(1.0f - d / radius, 0.0f, 1.0f);
            if (fade <= 0.04f) continue;

            if (x == 0) {
                SDL_SetRenderDrawColor(r, 75, 190, 95, (Uint8)(220 * fade));
            } else if (major) {
                SDL_SetRenderDrawColor(r, 95, 100, 110, (Uint8)(170 * fade));
            } else {
                SDL_SetRenderDrawColor(r, 65, 68, 75, (Uint8)(110 * fade));
            }
            drawLine3D(r, {(float)x, 0, (float)z},
                       {(float)x, 0, (float)(z + step)},
                       camPos, yaw, pitch, w, h);
        }
    }

    for (int z = sz; z <= ez; ++z) {
        bool major = (z % 5 == 0);
        for (int x = sx; x < ex; x += step) {
            float d = std::hypot(x - center.x, z - center.z);
            float fade = std::clamp(1.0f - d / radius, 0.0f, 1.0f);
            if (fade <= 0.04f) continue;

            if (z == 0) {
                SDL_SetRenderDrawColor(r, 215, 75, 85, (Uint8)(220 * fade));
            } else if (major) {
                SDL_SetRenderDrawColor(r, 95, 100, 110, (Uint8)(170 * fade));
            } else {
                SDL_SetRenderDrawColor(r, 65, 68, 75, (Uint8)(110 * fade));
            }
            drawLine3D(r, {(float)x, 0, (float)z},
                       {(float)(x + step), 0, (float)z},
                       camPos, yaw, pitch, w, h);
        }
    }
}
void drawCharacter(SDL_Renderer* r, const Character& c,
                   const Vec3& camPos, float yaw, float pitch,
                   int w, int h) {
    float cosF = std::cos(c.facingYaw);
    float sinF = std::sin(c.facingYaw);

    auto toWorld = [&](float lx, float ly, float lz) -> Vec3 {
        return {
            c.pos.x + lx * cosF + lz * sinF,
            c.pos.y + ly,
            c.pos.z - lx * sinF + lz * cosF
        };
    };

    float hw = c.halfWidth;
    float ht = c.height;

    std::array<Vec3, 8> v = {{
        toWorld(-hw, 0, -hw), toWorld(hw, 0, -hw),
        toWorld( hw, 0,  hw), toWorld(-hw, 0, hw),
        toWorld(-hw, ht,-hw), toWorld(hw, ht,-hw),
        toWorld( hw, ht, hw), toWorld(-hw, ht, hw)
    }};

    std::array<std::pair<int, int>, 12> edges = {{
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    }};

    SDL_SetRenderDrawColor(r, 255, 165, 55, 255);
    for (const auto& e : edges) {
        drawLine3D(r, v.at(e.first), v.at(e.second),
                   camPos, yaw, pitch, w, h);
    }

    Vec3 chest = toWorld(0, ht * 0.7f, 0);
    Vec3 nose  = toWorld(0, ht * 0.7f, hw + 0.45f);
    SDL_SetRenderDrawColor(r, 255, 240, 120, 255);
    drawLine3D(r, chest, nose, camPos, yaw, pitch, w, h);
}

void drawBtn(SDL_Renderer* r, const Button& b) {
    if (b.pressed) {
        SDL_SetRenderDrawColor(r, 255, 165, 55, 180);
    } else {
        SDL_SetRenderDrawColor(r, 45, 48, 56, 160);
    }
    SDL_RenderFillRectF(r, &b.rect);
    SDL_SetRenderDrawColor(r, 180, 185, 195, 220);
    SDL_RenderDrawRectF(r, &b.rect);
}

bool hitTest(float x, float y, const SDL_FRect& r) {
    return x >= r.x && x <= r.x + r.w &&
           y >= r.y && y <= r.y + r.h;
}

int main(int argc, char* argv[]) {
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;

    int winW = 800, winH = 480;
#ifdef __PSP__
    winW = 480;
    winH = 272;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
#endif

    SDL_Window* win = SDL_CreateWindow(
        "GTA Grid 3D",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        winW, winH, SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP
    );
    if (!win) return 1;

    SDL_Renderer* ren = SDL_CreateRenderer(
        win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!ren) return 1;

    Character player;
    Camera cam;

    const float gravity = -25.0f;
    const float jumpForce = 11.0f;
    const float moveSpeed = 6.5f;

    bool draggingOrbit = false;
    SDL_FingerID orbitFinger = 0;

    bool running = true;
    Uint64 lastTicks = SDL_GetTicks64();

    while (running) {
        Uint64 now = SDL_GetTicks64();
        float dt = (now - lastTicks) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        lastTicks = now;

        SDL_GetWindowSize(win, &winW, &winH);

        float pad = 28.0f;
        float bs = winH * 0.14f;
        float bx = pad + bs;
        float by = winH - pad - bs;

        Button btnUp   {{bx,          by - bs - 8, bs, bs}, false};
        Button btnDown {{bx,          by,          bs, bs}, false};
        Button btnLeft {{bx - bs - 8, by,          bs, bs}, false};
        Button btnRight{{bx + bs + 8, by,          bs, bs}, false};
        float jb = winH * 0.20f;
        Button btnJump {{winW - pad - jb, winH - pad - jb, jb, jb}, false};

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN &&
                e.key.keysym.sym == SDLK_SPACE &&
                player.onGround) {
                player.vel.y = jumpForce;
                player.onGround = false;
            }
            if (e.type == SDL_FINGERDOWN) {
                float fx = e.tfinger.x * winW;
                float fy = e.tfinger.y * winH;
                if (hitTest(fx, fy, btnJump.rect) && player.onGround) {
                    player.vel.y = jumpForce;
                    player.onGround = false;
                } else if (!hitTest(fx, fy, btnUp.rect) &&
                           !hitTest(fx, fy, btnDown.rect) &&
                           !hitTest(fx, fy, btnLeft.rect) &&
                           !hitTest(fx, fy, btnRight.rect)) {
                    draggingOrbit = true;
                    orbitFinger = e.tfinger.fingerId;
                }
            }
            if (e.type == SDL_FINGERMOTION &&
                draggingOrbit &&
                e.tfinger.fingerId == orbitFinger) {
                cam.yaw += e.tfinger.dx * winW * 0.007f;
                cam.pitch += e.tfinger.dy * winH * 0.007f;
                cam.pitch = std::clamp(cam.pitch, 0.08f, 1.35f);
            }
            if (e.type == SDL_FINGERUP &&
                e.tfinger.fingerId == orbitFinger) {
                draggingOrbit = false;
            }
        }

        int numDevs = SDL_GetNumTouchDevices();
        for (int d = 0; d < numDevs; ++d) {
            SDL_TouchID tid = SDL_GetTouchDevice(d);
            int numF = SDL_GetNumTouchFingers(tid);
            for (int f = 0; f < numF; ++f) {
                SDL_Finger* fin = SDL_GetTouchFinger(tid, f);
                if (!fin) continue;
                float fx = fin->x * winW;
                float fy = fin->y * winH;
                if (hitTest(fx, fy, btnUp.rect))    btnUp.pressed = true;
                if (hitTest(fx, fy, btnDown.rect))  btnDown.pressed = true;
                if (hitTest(fx, fy, btnLeft.rect))  btnLeft.pressed = true;
                if (hitTest(fx, fy, btnRight.rect)) btnRight.pressed = true;
                if (hitTest(fx, fy, btnJump.rect))  btnJump.pressed = true;
            }
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        float inF = 0.0f, inR = 0.0f;
        if (keys[SDL_SCANCODE_W] || btnUp.pressed)    inF += 1.0f;
        if (keys[SDL_SCANCODE_S] || btnDown.pressed)  inF -= 1.0f;
        if (keys[SDL_SCANCODE_D] || btnRight.pressed) inR += 1.0f;
        if (keys[SDL_SCANCODE_A] || btnLeft.pressed)  inR -= 1.0f;

#ifdef __PSP__
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_START) running = false;
        if ((pad.Buttons & PSP_CTRL_CROSS) && player.onGround) {
            player.vel.y = jumpForce;
            player.onGround = false;
        }
        if (pad.Buttons & PSP_CTRL_LTRIGGER) cam.yaw -= 2.2f * dt;
        if (pad.Buttons & PSP_CTRL_RTRIGGER) cam.yaw += 2.2f * dt;
        if (pad.Buttons & PSP_CTRL_TRIANGLE) cam.pitch += 1.4f * dt;
        if (pad.Buttons & PSP_CTRL_SQUARE)   cam.pitch -= 1.4f * dt;
        cam.pitch = std::clamp(cam.pitch, 0.08f, 1.35f);

        float ax = (pad.Lx - 128) / 128.0f;
        float ay = -(pad.Ly - 128) / 128.0f;
        if (std::fabs(ax) > 0.2f) inR = ax;
        if (std::fabs(ay) > 0.2f) inF = ay;
        if (pad.Buttons & PSP_CTRL_UP)    inF = 1.0f;
        if (pad.Buttons & PSP_CTRL_DOWN)  inF = -1.0f;
        if (pad.Buttons & PSP_CTRL_RIGHT) inR = 1.0f;
        if (pad.Buttons & PSP_CTRL_LEFT)  inR = -1.0f;
#endif

        if (inF != 0.0f || inR != 0.0f) {
            float len = std::hypot(inF, inR);
            if (len > 1.0f) { inF /= len; inR /= len; }

            float fX = std::sin(cam.yaw);
            float fZ = std::cos(cam.yaw);
            float rX = std::cos(cam.yaw);
            float rZ = -std::sin(cam.yaw);

            float mX = fX * inF + rX * inR;
            float mZ = fZ * inF + rZ * inR;

            player.pos.x += mX * moveSpeed * dt;
            player.pos.z += mZ * moveSpeed * dt;
            player.facingYaw = std::atan2(mX, mZ);
        }

        player.vel.y += gravity * dt;
        player.pos.y += player.vel.y * dt;
        if (player.pos.y <= 0.0f) {
            player.pos.y = 0.0f;
            player.vel.y = 0.0f;
            player.onGround = true;
        }

        Vec3 want{player.pos.x, player.pos.y + 1.1f, player.pos.z};
        float sm = std::clamp(10.0f * dt, 0.0f, 1.0f);
        cam.target.x += (want.x - cam.target.x) * sm;
        cam.target.y += (want.y - cam.target.y) * sm;
        cam.target.z += (want.z - cam.target.z) * sm;

        Vec3 camPos = getCameraPos(cam);

        SDL_SetRenderDrawColor(ren, 38, 40, 44, 255);
        SDL_RenderClear(ren);

        drawBlenderGrid(ren, player.pos, camPos, cam.yaw, cam.pitch, winW, winH);
        drawCharacter(ren, player, camPos, cam.yaw, cam.pitch, winW, winH);

#ifndef __PSP__
        drawBtn(ren, btnUp);
        drawBtn(ren, btnDown);
        drawBtn(ren, btnLeft);
        drawBtn(ren, btnRight);
        drawBtn(ren, btnJump);
#endif

        SDL_RenderPresent(ren);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
#ifdef __PSP__
    sceKernelExitGame();
#endif
    return 0;
}
