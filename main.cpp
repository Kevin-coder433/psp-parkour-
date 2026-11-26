#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>

PSP_MODULE_INFO("Parkour PSP", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCREEN_W 480
#define SCREEN_H 272

static unsigned int __attribute__((aligned(16))) list[262144];

struct Rect { float x, y, w, h; unsigned int color; };

struct Vertex {
    float x, y, z;
    unsigned int color;
};

void drawRect(float x, float y, float w, float h, unsigned int color) {
    Vertex* v = (Vertex*)sceGuGetMemory(2 * sizeof(Vertex));
    v[0].x = x; v[0].y = y; v[0].z = 0; v[0].color = color;
    v[1].x = x + w; v[1].y = y + h; v[1].z = 0; v[1].color = color;
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
}

int main() {
    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void*)0, 512);
    sceGuDispBuffer(SCREEN_W, SCREEN_H, (void*)0x88000, 512);
    sceGuDepthBuffer((void*)0x110000, 512);
    sceGuOffset(2048 - (SCREEN_W / 2), 2048 - (SCREEN_H / 2));
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_CULL_FACE);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    // Player
    float px = 50, py = 200, vx = 0, vy = 0;
    float pw = 20, ph = 28;
    bool onGround = false;
    float camX = 0;

    // Platforms
    Rect platforms[] = {
        {0, 240, 1000, 32, 0xFF00AA00},
        {220, 190, 100, 16, 0xFF00AA00},
        {360, 150, 100, 16, 0xFF00AA00},
        {520, 190, 100, 16, 0xFF00AA00},
        {700, 140, 120, 16, 0xFF00AA00},
        {900, 190, 100, 16, 0xFF00AA00},
        {1100, 150, 150, 16, 0xFFFFAA00},
    };
    int numPlatforms = 7;

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_START) break;

        // Move
        vx = 0;
        if (pad.Buttons & PSP_CTRL_LEFT) vx = -3;
        if (pad.Buttons & PSP_CTRL_RIGHT) vx = 3;
        if (pad.Buttons & PSP_CTRL_CROSS && onGround) {
            vy = -10;
            onGround = false;
        }

        vy += 0.5f;
        if (vy > 10) vy = 10;

        px += vx;
        py += vy;

        // Collide
        onGround = false;
        for (int i = 0; i < numPlatforms; i++) {
            Rect p = platforms[i];
            if (px + pw > p.x && px < p.x + p.w &&
                py + ph > p.y && py + ph < p.y + 20 && vy >= 0) {
                py = p.y - ph;
                vy = 0;
                onGround = true;
            }
        }
        if (py > 400) { px = 50; py = 200; vy = 0; camX = 0; }

        // Camera follows player
        camX = px - 100;
        if (camX < 0) camX = 0;

        // Draw
        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFF87CEEB);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadIdentity();
        sceGumOrtho(0, SCREEN_W, SCREEN_H, 0, -1, 1);
        sceGumMatrixMode(GU_VIEW);
        sceGumLoadIdentity();
        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();

        for (int i = 0; i < numPlatforms; i++) {
            drawRect(platforms[i].x - camX, platforms[i].y, platforms[i].w, platforms[i].h, platforms[i].color);
        }
        // Player - red
        drawRect(px - camX, py, pw, ph, 0xFF0000FF);
        // Goal flag - gold at end
        drawRect(1150 - camX, 100, 10, 50, 0xFFFFFFFF);

        sceGuFinish();
        sceGuSync(0, 0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
