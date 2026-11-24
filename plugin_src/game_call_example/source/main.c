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
int (*orig_sceVideoOutRegisterBuffers)(int handle, int startIndex, void * const *addresses, int bufferNum, const void *attribute) = NULL;
int (*orig_sceVideoOutSubmitFlip)(int handle, int bufferIndex, int flipMode, int64_t flipArg) = NULL;

void **g_fbAddresses = NULL;
int g_fbWidth = 1920;
int g_fbHeight = 1080;
int g_fbPitch = 1920;

static const uint8_t font5x7_flat[] = {
    0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x5F,0x00,0x00, 0x00,0x07,0x00,0x07,0x00, 0x14,0x7F,0x14,0x7F,0x14,
    0x24,0x2A,0x7F,0x2A,0x12, 0x23,0x13,0x08,0x64,0x62, 0x36,0x49,0x55,0x22,0x50, 0x00,0x05,0x03,0x00,0x00,
    0x00,0x1C,0x22,0x41,0x00, 0x00,0x41,0x22,0x1C,0x00, 0x14,0x08,0x3E,0x08,0x14, 0x08,0x08,0x3E,0x08,0x08,
    0x00,0x50,0x30,0x00,0x00, 0x08,0x08,0x08,0x08,0x08, 0x00,0x60,0x60,0x00,0x00, 0x20,0x10,0x08,0x04,0x02,
    0x3E,0x51,0x49,0x45,0x3E, 0x00,0x42,0x7F,0x40,0x00, 0x42,0x61,0x51,0x49,0x46, 0x21,0x41,0x45,0x4B,0x31,
    0x18,0x14,0x12,0x7F,0x10, 0x27,0x45,0x45,0x45,0x39, 0x3C,0x4A,0x49,0x49,0x30, 0x01,0x71,0x09,0x05,0x03,
    0x36,0x49,0x49,0x49,0x36, 0x06,0x49,0x49,0x29,0x1E, 0x00,0x36,0x36,0x00,0x00, 0x00,0x56,0x36,0x00,0x00,
    0x08,0x14,0x22,0x41,0x00, 0x14,0x14,0x14,0x14,0x14, 0x00,0x41,0x22,0x14,0x08, 0x02,0x01,0x51,0x09,0x06,
    0x32,0x49,0x79,0x41,0x3E, 0x7E,0x11,0x11,0x11,0x7E, 0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22,
    0x7F,0x41,0x41,0x22,0x1C, 0x7F,0x49,0x49,0x49,0x41, 0x7F,0x09,0x09,0x09,0x01, 0x3E,0x41,0x49,0x49,0x7A,
    0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00, 0x20,0x40,0x41,0x3F,0x01, 0x7F,0x08,0x14,0x22,0x41,
    0x7F,0x40,0x40,0x40,0x40, 0x7F,0x02,0x0C,0x02,0x7F, 0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E,
    0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, 0x7F,0x09,0x19,0x29,0x46, 0x46,0x49,0x49,0x49,0x31,
    0x01,0x01,0x7F,0x01,0x01, 0x3F,0x40,0x40,0x40,0x3F, 0x1F,0x20,0x40,0x20,0x1F, 0x3F,0x40,0x38,0x40,0x3F,
    0x63,0x14,0x08,0x14,0x63, 0x07,0x08,0x70,0x08,0x07, 0x61,0x51,0x49,0x45,0x43, 0x00,0x7F,0x41,0x41,0x00,
    0x02,0x04,0x08,0x10,0x20, 0x00,0x41,0x41,0x7F,0x00
};

void DrawRect(uint32_t *fb, int x, int y, int w, int h, uint32_t color)
{
    if (!fb) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > g_fbWidth) w = g_fbWidth - x;
    if (y + h > g_fbHeight) h = g_fbHeight - y;
    for (int py = y; py < y + h; py++) {
        uint32_t *row = fb + py * g_fbPitch;
        for (int px = x; px < x + w; px++) row[px] = color;
    }
}

void DrawText(uint32_t *fb, int x, int y, const char *str, int scale, uint32_t color)
{
    if (!fb || !str) return;
    int cx = x;
    while (*str) {
        char c = *str;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 32 || c > 93) c = ' ';
        int idx = (c - 32) * 5;
        for (int col = 0; col < 5; col++) {
            uint8_t bits = font5x7_flat[idx + col];
            for (int row = 0; row < 7; row++) {
                if (bits & (1 << row)) DrawRect(fb, cx + col * scale, y + row * scale, scale, scale, color);
            }
        }
        cx += 6 * scale;
        str++;
    }
}

int GetOptionCount(int menu)
{
    if (menu == 0) return 5;
    if (menu == 1) return 3;
    if (menu == 2) return 3;
    if (menu == 3) return 2;
    return 1;
}

void RenderWinesaiyaMenu(uint32_t *fb)
{
    if (!menuOpen || !fb) return;
    const uint32_t COL_GREEN = 0xFF39FF14;
    const uint32_t COL_BLACK = 0xFF0A0A0A;
    const uint32_t COL_BG    = 0xFF111111;
    const uint32_t COL_WHITE = 0xFFFFFFFF;

    int boxW = 460, boxX = g_fbWidth - boxW - 70, boxY = 110;
    int headerH = 74, subH = 38, rowH = 44;
    int total = GetOptionCount(currentMenu);
    int bodyH = total * rowH + 10;
    int totalH = headerH + subH + bodyH;

    DrawRect(fb, boxX - 4, boxY - 4, boxW + 8, totalH + 8, COL_GREEN);
    DrawRect(fb, boxX, boxY, boxW, headerH, COL_GREEN);
    DrawText(fb, boxX + 24, boxY + 20, "WINESAIYA", 5, COL_BLACK);

    int subY = boxY + headerH;
    DrawRect(fb, boxX, subY, boxW, subH, COL_BLACK);
    const char *catName = (currentMenu == 1) ? "SPIELER" : (currentMenu == 2) ? "TELEPORT" : (currentMenu == 3) ? "GELD & RANK" : (currentMenu == 4) ? "WAFFEN" : (currentMenu == 5) ? "FAHRZEUGE" : "HAUPTMENU";
    DrawText(fb, boxX + 18, subY + 11, catName, 2, COL_GREEN);

    char *cnt = (char *)calloc(32, 1);
    if (cnt) {
        snprintf(cnt, 32, "%d/%d", currentOption + 1, total);
        DrawText(fb, boxX + boxW - 65, subY + 11, cnt, 2, COL_GREEN);
        free(cnt);
    }

    int bodyY = subY + subH;
    DrawRect(fb, boxX, bodyY, boxW, bodyH, COL_BG);

    for (int i = 0; i < total; i++) {
        int ry = bodyY + 5 + i * rowH;
        bool sel = (i == currentOption);
        if (sel) DrawRect(fb, boxX + 6, ry, boxW - 12, rowH - 4, COL_GREEN);

        uint32_t tCol = sel ? COL_BLACK : COL_WHITE;
        const char *lbl = "", *st = "";
        if (currentMenu == 0) {
            if (i == 0) { lbl = "SPIELER"; st = ">"; }
            else if (i == 1) { lbl = "TELEPORT"; st = ">"; }
            else if (i == 2) { lbl = "GELD & RANK"; st = ">"; }
            else if (i == 3) { lbl = "WAFFEN"; st = ">"; }
            else { lbl = "FAHRZEUGE"; st = ">"; }
        } else if (currentMenu == 1) {
            if (i == 0) { lbl = "GODMODE"; st = godmode ? "[AN]" : "[AUS]"; }
            else if (i == 1) { lbl = "SUPER SPRUNG"; st = superJump ? "[AN]" : "[AUS]"; }
            else { lbl = "KEINE FAHNDUNG"; st = neverWanted ? "[AN]" : "[AUS]"; }
        } else if (currentMenu == 2) {
            lbl = (i == 0) ? "WEGPUNKT" : (i == 1) ? "MOUNT CHILIAD" : "FLUGHAFEN LS";
        } else if (currentMenu == 3) {
            lbl = (i == 0) ? "GELD +10.000.000$" : "SET RANK 999";
        } else if (currentMenu == 4) lbl = "ALLE WAFFEN GEBEN";
        else lbl = "ZENTORNO SPAWNEN";

        DrawText(fb, boxX + 20, ry + 12, lbl, 2, tCol);
        if (st[0] != '\0') {
            uint32_t sCol = sel ? COL_BLACK : (strstr(st, "AN") ? COL_GREEN : COL_WHITE);
            DrawText(fb, boxX + boxW - 95, ry + 12, st, 2, sCol);
        }
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

int Hooked_sceVideoOutRegisterBuffers(int handle, int startIndex, void * const *addresses, int bufferNum, const void *attribute)
{
    if (g_fbAddresses && addresses && bufferNum > 0) {
        for (int i = 0; i < bufferNum && (startIndex + i) < 16; i++) g_fbAddresses[startIndex + i] = addresses[i];
    }
    return orig_sceVideoOutRegisterBuffers ? orig_sceVideoOutRegisterBuffers(handle, startIndex, addresses, bufferNum, attribute) : -1;
}

int Hooked_sceVideoOutSubmitFlip(int handle, int bufferIndex, int flipMode, int64_t flipArg)
{
    if (menuOpen && g_fbAddresses && bufferIndex >= 0 && bufferIndex < 16 && g_fbAddresses[bufferIndex]) {
        RenderWinesaiyaMenu((uint32_t *)g_fbAddresses[bufferIndex]);
    }
    return orig_sceVideoOutSubmitFlip ? orig_sceVideoOutSubmitFlip(handle, bufferIndex, flipMode, flipArg) : -1;
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
    int vidLib = sceKernelLoadStartModule("libSceVideoOut.sprx", 0, NULL, 0, NULL, NULL);
    if (vidLib > 0) {
        sceKernelDlsym(vidLib, "sceVideoOutRegisterBuffers", (void**)&orig_sceVideoOutRegisterBuffers);
        sceKernelDlsym(vidLib, "sceVideoOutSubmitFlip", (void**)&orig_sceVideoOutSubmitFlip);
    }

    PatchSafeGOTFast((void*)orig_scePadReadState, (void*)Hooked_scePadReadState);
    PatchSafeGOTFast((void*)orig_scePadRead, (void*)Hooked_scePadRead);
    PatchSafeGOTFast((void*)orig_sceVideoOutRegisterBuffers, (void*)Hooked_sceVideoOutRegisterBuffers);
    PatchSafeGOTFast((void*)orig_sceVideoOutSubmitFlip, (void*)Hooked_sceVideoOutSubmitFlip);

    sleep(2);
    ShowPS4Notification("Winesaiya geladen!");

    bool storyHintShown = false;
    int ticks = 0;

    while (true) {
        ticks++;
        if (!storyHintShown && ticks >= 2500) {
            storyHintShown = true;
            ShowPS4Notification("Welcome to Winesaiya!\nDr\xC3\xBC""cke Rechte Pfeiltaste + Viereck zum \xC3\x96""ffnen.");
        }

        uint32_t btns = g_gameButtons;
        if (btns != 0) {
            if ((btns & ORBIS_PAD_BUTTON_RIGHT) && (btns & ORBIS_PAD_BUTTON_SQUARE)) {
                menuOpen = !menuOpen;
                if (menuOpen) {
                    currentMenu = 0; currentOption = 0;
                    if (!g_fbAddresses || !g_fbAddresses[0]) ShowPS4Notification("[WINESAIYA] Ge\xC3\xB6""ffnet (Kein FB)");
                }
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
    g_fbAddresses = (void **)calloc(16, sizeof(void *));
    pthread_t thread;
    pthread_create(&thread, NULL, my_thread, NULL);
    return 0;
}

int32_t attr_module_hidden module_stop(size_t argc, const void *args)
{
    return 0;
}
