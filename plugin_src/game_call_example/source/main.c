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
attr_public const char *g_pluginDesc = "Winesaiya GTA V Menu";
attr_public const char *g_pluginAuth = "winesaiya";
attr_public uint32_t g_pluginVersion = 0x00000100;

struct proc_info procInfo;

bool menuOpen = false;
int currentMenu = 0;   // 0=Hauptmenü, 1=Spieler, 2=Teleport, 3=Geld & Rank, 4=Waffen, 5=Fahrzeuge
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
    req.unk3 = 0;
    req.useIconImageUri = 0;
    req.targetId = -1;
    snprintf(req.message, sizeof(req.message), "%s", text);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

int GetOptionCount(int menu)
{
    switch (menu) {
        case 0: return 5; // Hauptmenü: 5 Kategorien
        case 1: return 3; // Spieler: Godmode, Super Sprung, Keine Fahndung
        case 2: return 3; // Teleport: Wegpunkt, Mount Chiliad, Flughafen LS
        case 3: return 2; // Geld & Rank: +10 Mio, Set Rank 999
        case 4: return 1; // Waffen: Alle Waffen
        case 5: return 1; // Fahrzeuge: Zentorno
        default: return 1;
    }
}

void GetMenuTitle(int menu, char *out, size_t len)
{
    switch (menu) {
        case 0: snprintf(out, len, "HAUPTMEN\xC3\x9C"); break;
        case 1: snprintf(out, len, "SPIELER"); break;
        case 2: snprintf(out, len, "TELEPORT"); break;
        case 3: snprintf(out, len, "GELD & RANK"); break;
        case 4: snprintf(out, len, "WAFFEN"); break;
        case 5: snprintf(out, len, "FAHRZEUGE"); break;
        default: snprintf(out, len, "WINESAIYA"); break;
    }
}

void GetOptionLabel(int menu, int opt, char *out, size_t len)
{
    if (menu == 0) {
        switch (opt) {
            case 0: snprintf(out, len, "Spieler  >"); break;
            case 1: snprintf(out, len, "Teleport  >"); break;
            case 2: snprintf(out, len, "Geld & Rank  >"); break;
            case 3: snprintf(out, len, "Waffen  >"); break;
            case 4: snprintf(out, len, "Fahrzeuge  >"); break;
        }
    } else if (menu == 1) {
        switch (opt) {
            case 0: snprintf(out, len, "Godmode  [%s]", godmode ? "AN" : "AUS"); break;
            case 1: snprintf(out, len, "Super Sprung  [%s]", superJump ? "AN" : "AUS"); break;
            case 2: snprintf(out, len, "Keine Fahndung  [%s]", neverWanted ? "AN" : "AUS"); break;
        }
    } else if (menu == 2) {
        switch (opt) {
            case 0: snprintf(out, len, "Wegpunkt"); break;
            case 1: snprintf(out, len, "Mount Chiliad"); break;
            case 2: snprintf(out, len, "Flughafen LS"); break;
        }
    } else if (menu == 3) {
        switch (opt) {
            case 0: snprintf(out, len, "Geld +10.000.000$"); break;
            case 1: snprintf(out, len, "Set Rank: 999"); break;
        }
    } else if (menu == 4) {
        snprintf(out, len, "Alle Waffen geben");
    } else if (menu == 5) {
        snprintf(out, len, "Zentorno spawnen");
    }
}

void ShowMenuView()
{
    char *msg = (char *)calloc(256, 1);
    char *title = (char *)calloc(64, 1);
    char *item = (char *)calloc(96, 1);
    if (!msg || !title || !item) {
        free(msg); free(title); free(item);
        return;
    }

    int total = GetOptionCount(currentMenu);
    GetMenuTitle(currentMenu, title, 64);
    GetOptionLabel(currentMenu, currentOption, item, 96);

    snprintf(msg, 256, "[WINESAIYA | %s]\n> %s (%d/%d)", title, item, currentOption + 1, total);
    ShowPS4Notification(msg);

    free(msg);
    free(title);
    free(item);
}

int Hooked_scePadReadState(int handle, OrbisPadData *pData)
{
    int ret = orig_scePadReadState ? orig_scePadReadState(handle, pData) : -1;
    if (ret == 0 && pData && pData->connected) {
        g_gameButtons = pData->buttons;
    }
    return ret;
}

int Hooked_scePadRead(int handle, OrbisPadData *pData, int num)
{
    int ret = orig_scePadRead ? orig_scePadRead(handle, pData, num) : -1;
    if (ret > 0 && pData && pData->connected) {
        g_gameButtons = pData->buttons;
    }
    return ret;
}

bool SafeWrite64(uint64_t addr, uint64_t inVal)
{
    struct proc_rw rw;
    memset(&rw, 0, sizeof(rw));
    rw.address = addr;
    rw.data = &inVal;
    rw.length = sizeof(uint64_t);
    rw.write_flags = 1;
    return (sys_sdk_proc_rw(&rw) == 0);
}

bool PatchSafeGOTFast(void *targetFunc, void *hookFunc)
{
    if (!targetFunc || !hookFunc || procInfo.base_address == 0) return false;
    uint64_t target = (uint64_t)targetFunc;
    uint64_t hook = (uint64_t)hookFunc;
    bool patched = false;

    const size_t chunkSize = 0x10000;
    uint64_t *chunk = (uint64_t *)malloc(chunkSize);
    if (!chunk) return false;

    for (uint64_t offset = 0x0; offset < 0x5500000; offset += chunkSize)
    {
        uint64_t base = procInfo.base_address + offset;
        struct proc_rw rw;
        memset(&rw, 0, sizeof(rw));
        rw.address = base;
        rw.data = chunk;
        rw.length = chunkSize;
        rw.write_flags = 0;

        if (sys_sdk_proc_rw(&rw) == 0)
        {
            size_t count = chunkSize / sizeof(uint64_t);
            for (size_t i = 0; i < count; i++)
            {
                if (chunk[i] == target)
                {
                    if (SafeWrite64(base + i * sizeof(uint64_t), hook))
                    {
                        patched = true;
                    }
                }
            }
        }
    }

    free(chunk);
    return patched;
}

void* my_thread(void* args)
{
    sleep(1);

    int padLib = sceKernelLoadStartModule("libScePad.sprx", 0, NULL, 0, NULL, NULL);
    if (padLib > 0) {
        sceKernelDlsym(padLib, "scePadReadState", (void**)&orig_scePadReadState);
        sceKernelDlsym(padLib, "scePadRead", (void**)&orig_scePadRead);
    }

    PatchSafeGOTFast((void*)orig_scePadReadState, (void*)Hooked_scePadReadState);
    PatchSafeGOTFast((void*)orig_scePadRead, (void*)Hooked_scePadRead);

    // 1. Pop-up direkt beim Laden von GTA V
    ShowPS4Notification("Winesaiya geladen!");

    bool storyHintShown = false;
    int ticks = 0;

    while (true)
    {
        ticks++;

        // 2. Hinweis oben links, sobald der Story-Modus geladen ist (ca. 45 Sek.)
        if (!storyHintShown && ticks >= 2600)
        {
            storyHintShown = true;
            ShowPS4Notification("Welcome to Winesaiya!\nDr\xC3\xBC""cke Rechte Pfeiltaste + Viereck zum \xC3\x96""ffnen.");
        }

        uint32_t btns = g_gameButtons;
        if (btns != 0)
        {
            bool rightPressed  = (btns & ORBIS_PAD_BUTTON_RIGHT);
            bool squarePressed = (btns & ORBIS_PAD_BUTTON_SQUARE);
            bool upPressed     = (btns & ORBIS_PAD_BUTTON_UP);
            bool downPressed   = (btns & ORBIS_PAD_BUTTON_DOWN);
            bool crossPressed  = (btns & ORBIS_PAD_BUTTON_CROSS);
            bool circlePressed = (btns & ORBIS_PAD_BUTTON_CIRCLE);

            if (rightPressed && squarePressed)
            {
                menuOpen = !menuOpen;
                if (menuOpen) {
                    currentMenu = 0;
                    currentOption = 0;
                    ShowMenuView();
                } else {
                    ShowPS4Notification("[WINESAIYA] Geschlossen");
                }
                sceKernelUsleep(350000);
            }
            else if (menuOpen)
            {
                int total = GetOptionCount(currentMenu);

                if (downPressed)
                {
                    currentOption = (currentOption + 1) % total;
                    ShowMenuView();
                    sceKernelUsleep(230000);
                }
                else if (upPressed)
                {
                    currentOption = (currentOption - 1 + total) % total;
                    ShowMenuView();
                    sceKernelUsleep(230000);
                }
                else if (circlePressed)
                {
                    if (currentMenu != 0) {
                        currentMenu = 0;
                        currentOption = 0;
                        ShowMenuView();
                    } else {
                        menuOpen = false;
                        ShowPS4Notification("[WINESAIYA] Geschlossen");
                    }
                    sceKernelUsleep(280000);
                }
                else if (crossPressed)
                {
                    if (currentMenu == 0)
                    {
                        currentMenu = currentOption + 1;
                        currentOption = 0;
                        ShowMenuView();
                    }
                    else if (currentMenu == 1)
                    {
                        if (currentOption == 0) godmode = !godmode;
                        else if (currentOption == 1) superJump = !superJump;
                        else if (currentOption == 2) neverWanted = !neverWanted;
                        ShowMenuView();
                    }
                    else if (currentMenu == 2)
                    {
                        if (currentOption == 0) ShowPS4Notification("[WINESAIYA] Teleport: Wegpunkt (Test)");
                        else if (currentOption == 1) ShowPS4Notification("[WINESAIYA] Teleport: Mount Chiliad (Test)");
