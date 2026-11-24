#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "plugin_common.h"
#include <orbis/libkernel.h>
#include <orbis/Pad.h>

attr_public const char *g_pluginName = "Winesaiya";
attr_public const char *g_pluginDesc = "Winesaiya Neon Menu";
attr_public const char *g_pluginAuth = "winesaiya";
attr_public uint32_t g_pluginVersion = 0x00000100;

struct proc_info procInfo;
bool menuOpen = false;
int currentMenu = 0;
int currentOption = 0;
bool godmode = false;
bool superJump = false;
bool neverWanted = false;

volatile uint32_t g_gameButtons = 0;
int (*orig_scePadReadState)(int handle, OrbisPadData *pData) = NULL;
int (*orig_scePadRead)(int handle, OrbisPadData *pData, int num) = NULL;

typedef struct {
    void *ReturnValue;
    uint32_t ArgCount;
    uint32_t Pad1;
    void *Args;
    uint32_t DataCount;
    uint32_t Pad2;
    uint8_t Space;
} scrNativeCallContext;

typedef void (*RageNativeHandler_t)(scrNativeCallContext *ctx);

uint64_t g_gameBase = 0;
Detour Detour_DrawRect;
bool g_isDrawingOurMenu = false;

uint32_t FloatToU32(float f)
{
    uint32_t u = 0;
    memcpy(&u, &f, sizeof(uint32_t));
    return u;
}

void CallNative8(uint64_t offset, uint32_t argCount, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7)
{
    if (!g_gameBase) return;
    uint64_t *argBuf = (uint64_t *)calloc(8, sizeof(uint64_t));
    if (!argBuf) return;
    *(argBuf + 0) = a0; *(argBuf + 1) = a1; *(argBuf + 2) = a2; *(argBuf + 3) = a3;
    *(argBuf + 4) = a4; *(argBuf + 5) = a5; *(argBuf + 6) = a6; *(argBuf + 7) = a7;

    scrNativeCallContext ctx;
    uint64_t retVal = 0;
    memset(&ctx, 0, sizeof(ctx));
    ctx.ReturnValue = &retVal;
    ctx.ArgCount = argCount;
    ctx.Args = argBuf;

    if (offset == 0xDCCAC0 && Detour_DrawRect.StubPtr) {
        RageNativeHandler_t orig = (RageNativeHandler_t)Detour_DrawRect.StubPtr;
        orig(&ctx);
    } else {
        RageNativeHandler_t fn = (RageNativeHandler_t)(g_gameBase + offset);
        fn(&ctx);
    }
    free(argBuf);
}

void GTA_DrawRect(float x, float y, float w, float h, int r, int g, int b, int a)
{
    CallNative8(0xDCCAC0, 8, FloatToU32(x), FloatToU32(y), FloatToU32(w), FloatToU32(h), (uint64_t)r, (uint64_t)g, (uint64_t)b, (uint64_t)a);
}

void GTA_DrawText(const char *text, float x, float y, float scale, int font, int r, int g, int b, int a)
{
    CallNative8(0xDE19F0, 1, (uint64_t)font, 0, 0, 0, 0, 0, 0, 0);
    CallNative8(0xDE1890, 2, FloatToU32(scale), FloatToU32(scale), 0, 0, 0, 0, 0, 0);
    CallNative8(0xDE18F0, 4, (uint64_t)r, (uint64_t)g, (uint64_t)b, (uint64_t)a, 0, 0, 0, 0);
    CallNative8(0xDE0690, 1, (uint64_t)"STRING", 0, 0, 0, 0, 0, 0, 0);
    CallNative8(0xDE08F0, 1, (uint64_t)text, 0, 0, 0, 0, 0, 0, 0);
    CallNative8(0xDE06C0, 3, FloatToU32(x), FloatToU32(y), 0, 0, 0, 0, 0, 0);
}

int GetOptionCount(int menu)
{
    if (menu == 0) return 5;
    if (menu == 1) return 3;
    if (menu == 2) return 3;
    if (menu == 3) return 2;
    return 1;
}

void RenderWinesaiyaInGame()
{
    if (!menuOpen || g_isDrawingOurMenu) return;
    g_isDrawingOurMenu = true;

    float menuX = 0.83f;
    float menuW = 0.23f;
    float curY  = 0.14f;

    float headerH = 0.075f;
    float subH    = 0.034f;
    float rowH    = 0.036f;
    int total     = GetOptionCount(currentMenu);
    float totalH  = headerH + subH + (total * rowH);

    GTA_DrawRect(menuX, curY + (totalH * 0.5f), menuW + 0.005f, totalH + 0.007f, 57, 255, 20, 255);
    GTA_DrawRect(menuX, curY + (headerH * 0.5f), menuW, headerH, 57, 255, 20, 255);
    GTA_DrawText("WINESAIYA", menuX - (menuW * 0.42f), curY + 0.015f, 0.72f, 1, 10, 10, 10, 255);
    curY += headerH;

    GTA_DrawRect(menuX, curY + (subH * 0.5f), menuW, subH, 8, 8, 8, 245);
    const char *cat = (currentMenu == 1) ? "SPIELER" : (currentMenu == 2) ? "TELEPORT" : (currentMenu == 3) ? "GELD & RANK" : (currentMenu == 4) ? "WAFFEN" : (currentMenu == 5) ? "FAHRZEUGE" : "HAUPTMENU";
    GTA_DrawText(cat, menuX - (menuW * 0.46f), curY + 0.005f, 0.36f, 4, 57, 255, 20, 255);

    char *cnt = (char *)calloc(24, 1);
    if (cnt) {
        snprintf(cnt, 24, "%d / %d", currentOption + 1, total);
        GTA_DrawText(cnt, menuX + (menuW * 0.30f), curY + 0.005f, 0.36f, 4, 57, 255, 20, 255);
        free(cnt);
    }
    curY += subH;

    for (int i = 0; i < total; i++) {
        bool sel = (i == currentOption);
        float rowMidY = curY + (rowH * 0.5f);

        if (sel) GTA_DrawRect(menuX, rowMidY, menuW, rowH, 57, 255, 20, 245);
        else GTA_DrawRect(menuX, rowMidY, menuW, rowH, 12, 12, 12, 225);

        int tr = sel ? 10 : 255;
        int tg = sel ? 10 : 255;
        int tb = sel ? 10 : 255;

        const char *lbl = "", *st = "";
        if (currentMenu == 0) {
            if (i == 0) { lbl = "Spieler"; st = ">"; }
            else if (i == 1) { lbl = "Teleport"; st = ">"; }
            else if (i == 2) { lbl = "Geld & Rank"; st = ">"; }
            else if (i == 3) { lbl = "Waffen"; st = ">"; }
            else { lbl = "Fahrzeuge"; st = ">"; }
        } else if (currentMenu == 1) {
            if (i == 0) { lbl = "Godmode"; st = godmode ? "[AN]" : "[AUS]"; }
            else if (i == 1) { lbl = "Super Sprung"; st = superJump ? "[AN]" : "[AUS]"; }
            else { lbl = "Keine Fahndung"; st = neverWanted ? "[AN]" : "[AUS]"; }
        } else if (currentMenu == 2) {
            lbl = (i == 0) ? "Wegpunkt" : (i == 1) ? "Mount Chiliad" : "Flughafen LS";
        } else if (currentMenu == 3) {
            lbl = (i == 0) ? "Geld +10.000.000$" : "Set Rank: 999";
        } else if (currentMenu == 4) lbl = "Alle Waffen geben";
        else lbl = "Zentorno spawnen";

        GTA_DrawText(lbl, menuX - (menuW * 0.46f), curY + 0.006f, 0.37f, 4, tr, tg, tb, 255);
        if (st && st[0] != '\0') {
            int sr = sel ? 10 : (strstr(st, "AN") ? 57 : 255);
            int sg = sel ? 10 : 255;
            int sb = sel ? 10 : (strstr(st, "AN") ? 20 : 255);
            GTA_DrawText(st, menuX + (menuW * 0.31f), curY + 0.006f, 0.37f, 4, sr, sg, sb, 255);
        }
        curY += rowH;
    }

    g_isDrawingOurMenu = false;
}

void Hooked_DrawRect(scrNativeCallContext *ctx)
{
    RageNativeHandler_t orig = (RageNativeHandler_t)Detour_DrawRect.StubPtr;
    if (orig) orig(ctx);

    static uint64_t lastDrawTick = 0;
    uint64_t now = sceKernelGetProcessTime();
    if (now - lastDrawTick > 12000) {
        lastDrawTick = now;
        RenderWinesaiyaInGame();
    }
}
void ShowPS4Notification(const char *text)
{
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof(req));
    req.type = 0;
    req.targetId = -1;
    snprintf(req.message, sizeof(req.message), "%s", text);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

int Hooked_scePadReadState(int handle, OrbisPadData *pData)
{
    int ret = orig_scePadReadState ? orig_scePadReadState(handle, pData) : -1;
    if (ret == 0 && pData && pData->connected) g_gameButtons = pData->buttons;
    return ret;
}

int Hooked_scePadRead(int handle, OrbisPadData *pData, int num)
{
    int ret = orig_scePadRead ? orig_scePadRead(handle, pData, num) : -1;
    if (ret > 0 && pData && pData->connected) g_gameButtons = pData->buttons;
    return ret;
}

bool SafeWrite64(uint64_t addr, uint64_t inVal)
{
    struct proc_rw rw;
    memset(&rw, 0, sizeof(rw));
    rw.address = addr; rw.data = &inVal; rw.length = 8; rw.write_flags = 1;
    return (sys_sdk_proc_rw(&rw) == 0);
}

void PatchSafeGOTFast(void *targetFunc, void *hookFunc)
{
    if (!targetFunc || !hookFunc || procInfo.base_address == 0) return;
    uint64_t target = (uint64_t)targetFunc, hook = (uint64_t)hookFunc;
    const size_t chunkSize = 0x10000;
    uint64_t *chunk = (uint64_t *)malloc(chunkSize);
    if (!chunk) return;

    for (uint64_t offset = 0x0; offset < 0x5500000; offset += chunkSize) {
        uint64_t base = procInfo.base_address + offset;
        struct proc_rw rw;
        memset(&rw, 0, sizeof(rw));
        rw.address = base; rw.data = chunk; rw.length = chunkSize; rw.write_flags = 0;
        if (sys_sdk_proc_rw(&rw) == 0) {
            for (size_t i = 0; i < (chunkSize / 8); i++) {
                if (chunk[i] == target) SafeWrite64(base + i * 8, hook);
            }
        }
    }
    free(chunk);
}

void* my_thread(void* args)
{
    int padLib = sceKernelLoadStartModule("libScePad.sprx", 0, NULL, 0, NULL, NULL);
    if (padLib > 0) {
        sceKernelDlsym(padLib, "scePadReadState", (void**)&orig_scePadReadState);
        sceKernelDlsym(padLib, "scePadRead", (void**)&orig_scePadRead);
    }

    PatchSafeGOTFast((void*)orig_scePadReadState, (void*)Hooked_scePadReadState);
    PatchSafeGOTFast((void*)orig_scePadRead, (void*)Hooked_scePadRead);

    sleep(2);
    ShowPS4Notification("Winesaiya geladen!");

    sleep(35);

    g_gameBase = procInfo.base_address;
    Detour_Construct(&Detour_DrawRect, 0);
    Detour_DetourFunction(&Detour_DrawRect, g_gameBase + 0xDCCAC0, (void*)Hooked_DrawRect);

    ShowPS4Notification("Welcome to Winesaiya!\nDr\xC3\xBC""cke Rechte Pfeiltaste + Viereck zum \xC3\x96""ffnen.");

    while (true) {
        uint32_t btns = g_gameButtons;
        if (btns != 0) {
            if ((btns & ORBIS_PAD_BUTTON_RIGHT) && (btns & ORBIS_PAD_BUTTON_SQUARE)) {
                menuOpen = !menuOpen;
                if (menuOpen) { currentMenu = 0; currentOption = 0; }
                sceKernelUsleep(350000);
            } else if (menuOpen) {
                int total = GetOptionCount(currentMenu);
                if (btns & ORBIS_PAD_BUTTON_DOWN) { currentOption = (currentOption + 1) % total; sceKernelUsleep(200000); }
                else if (btns & ORBIS_PAD_BUTTON_UP) { currentOption = (currentOption - 1 + total) % total; sceKernelUsleep(200000); }
                else if (btns & ORBIS_PAD_BUTTON_CIRCLE) {
                    if (currentMenu != 0) { currentMenu = 0; currentOption = 0; }
                    else menuOpen = false;
                    sceKernelUsleep(250000);
                } else if (btns & ORBIS_PAD_BUTTON_CROSS) {
                    if (currentMenu == 0) { currentMenu = currentOption + 1; currentOption = 0; }
                    else if (currentMenu == 1) {
                        if (currentOption == 0) godmode = !godmode;
                        else if (currentOption == 1) superJump = !superJump;
                        else neverWanted = !neverWanted;
                    } else if (currentMenu == 2) {
                        if (currentOption == 0) ShowPS4Notification("[WINESAIYA] Teleport: Wegpunkt (Test)");
                        else if (currentOption == 1) ShowPS4Notification("[WINESAIYA] Teleport: Mount Chiliad (Test)");
                        else ShowPS4Notification("[WINESAIYA] Teleport: Flughafen LS (Test)");
                    } else if (currentMenu == 3) {
                        if (currentOption == 0) ShowPS4Notification("[WINESAIYA] Geld +10.000.000$ (Test)");
                        else ShowPS4Notification("[WINESAIYA] Set Rank: 999 (Test)");
                    } else if (currentMenu == 4) ShowPS4Notification("[WINESAIYA] Alle Waffen erhalten (Test)");
                    else if (currentMenu == 5) ShowPS4Notification("[WINESAIYA] Zentorno gespawnt (Test)");
                    sceKernelUsleep(250000);
                }
            }
        }
        sceKernelUsleep(16666);
    }
    return NULL;
}

int32_t attr_module_hidden module_start(size_t argc, const void *args)
{
    if (sys_sdk_proc_info(&procInfo) != 0) return -1;
    pthread_t thread;
    pthread_create(&thread, NULL, my_thread, NULL);
    return 0;
}

int32_t attr_module_hidden module_stop(size_t argc, const void *args)
{
    return 0;
}
