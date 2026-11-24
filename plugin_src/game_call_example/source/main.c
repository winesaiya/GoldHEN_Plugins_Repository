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
attr_public const char *g_pluginDesc = "Winesaiya Menu";
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

void ShowPS4Notification(const char *text)
{
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof(req));
    req.type = 0;
    req.targetId = -1;
    snprintf(req.message, sizeof(req.message), "%s", text);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

int GetOptionCount(int menu)
{
    if (menu == 0) return 5;
    if (menu == 1) return 3;
    if (menu == 2) return 3;
    if (menu == 3) return 2;
    return 1;
}

void ShowFullCategoryBox()
{
    char *box = (char *)calloc(512, 1);
    if (!box) return;

    if (currentMenu == 0) {
        snprintf(box, 512,
            "=== WINESAIYA | HAUPTMEN\xC3\x9C ===\n"
            "%s Spieler >\n"
            "%s Teleport >\n"
            "%s Geld & Rank >\n"
            "%s Waffen >\n"
            "%s Fahrzeuge >",
            (currentOption == 0) ? ">>" : "  ",
            (currentOption == 1) ? ">>" : "  ",
            (currentOption == 2) ? ">>" : "  ",
            (currentOption == 3) ? ">>" : "  ",
            (currentOption == 4) ? ">>" : "  ");
    } else if (currentMenu == 1) {
        snprintf(box, 512,
            "=== WINESAIYA | SPIELER ===\n"
            "%s Godmode [%s]\n"
            "%s Super Sprung [%s]\n"
            "%s Keine Fahndung [%s]",
            (currentOption == 0) ? ">>" : "  ", godmode ? "AN" : "AUS",
            (currentOption == 1) ? ">>" : "  ", superJump ? "AN" : "AUS",
            (currentOption == 2) ? ">>" : "  ", neverWanted ? "AN" : "AUS");
    } else if (currentMenu == 2) {
        snprintf(box, 512,
            "=== WINESAIYA | TELEPORT ===\n"
            "%s Wegpunkt\n"
            "%s Mount Chiliad\n"
            "%s Flughafen LS",
            (currentOption == 0) ? ">>" : "  ",
            (currentOption == 1) ? ">>" : "  ",
            (currentOption == 2) ? ">>" : "  ");
    } else if (currentMenu == 3) {
        snprintf(box, 512,
            "=== WINESAIYA | GELD & RANK ===\n"
            "%s Geld +10.000.000$\n"
            "%s Set Rank: 999",
            (currentOption == 0) ? ">>" : "  ",
            (currentOption == 1) ? ">>" : "  ");
    } else if (currentMenu == 4) {
        snprintf(box, 512,
            "=== WINESAIYA | WAFFEN ===\n"
            ">> Alle Waffen geben");
    } else {
        snprintf(box, 512,
            "=== WINESAIYA | FAHRZEUGE ===\n"
            ">> Zentorno spawnen");
    }

    ShowPS4Notification(box);
    free(box);
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
                if (menuOpen) { currentMenu = 0; currentOption = 0; ShowFullCategoryBox(); }
                else ShowPS4Notification("=== WINESAIYA: GESCHLOSSEN ===");
                sceKernelUsleep(350000);
            } else if (menuOpen) {
                int total = GetOptionCount(currentMenu);
                if (btns & ORBIS_PAD_BUTTON_DOWN) { currentOption = (currentOption + 1) % total; ShowFullCategoryBox(); sceKernelUsleep(220000); }
                else if (btns & ORBIS_PAD_BUTTON_UP) { currentOption = (currentOption - 1 + total) % total; ShowFullCategoryBox(); sceKernelUsleep(220000); }
                else if (btns & ORBIS_PAD_BUTTON_CIRCLE) {
                    if (currentMenu != 0) { currentMenu = 0; currentOption = 0; ShowFullCategoryBox(); }
                    else { menuOpen = false; ShowPS4Notification("=== WINESAIYA: GESCHLOSSEN ==="); }
                    sceKernelUsleep(280000);
                } else if (btns & ORBIS_PAD_BUTTON_CROSS) {
                    if (currentMenu == 0) { currentMenu = currentOption + 1; currentOption = 0; ShowFullCategoryBox(); }
                    else if (currentMenu == 1) {
                        if (currentOption == 0) godmode = !godmode;
                        else if (currentOption == 1) superJump = !superJump;
                        else neverWanted = !neverWanted;
                        ShowFullCategoryBox();
                    } else if (currentMenu == 2) {
                        if (currentOption == 0) ShowPS4Notification("[WINESAIYA] Teleport: Wegpunkt (Test)");
                        else if (currentOption == 1) ShowPS4Notification("[WINESAIYA] Teleport: Mount Chiliad (Test)");
                        else ShowPS4Notification("[WINESAIYA] Teleport: Flughafen LS (Test)");
                    } else if (currentMenu == 3) {
                        if (currentOption == 0) ShowPS4Notification("[WINESAIYA] Geld +10.000.000$ (Test)");
                        else ShowPS4Notification("[WINESAIYA] Set Rank: 999 (Test)");
                    } else if (currentMenu == 4) ShowPS4Notification("[WINESAIYA] Alle Waffen erhalten (Test)");
                    else if (currentMenu == 5) ShowPS4Notification("[WINESAIYA] Zentorno gespawnt (Test)");
                    sceKernelUsleep(280000);
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
