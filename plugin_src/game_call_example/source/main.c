#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>

#include "plugin_common.h"
#include <orbis/libkernel.h>
#include <orbis/Pad.h>

attr_public const char *g_pluginName = "WineBetamenü";
attr_public const char *g_pluginDesc = "Custom PS4 Plugin";
attr_public const char *g_pluginAuth = "winesaiya";
attr_public uint32_t g_pluginVersion = 0x00000100;

struct proc_info procInfo;
bool menuOpen = false;
int currentOption = 0;

bool godmode = false;
bool superJump = false;
bool neverWanted = false;

#define TOTAL_OPTIONS 11

volatile uint32_t g_gameButtons = 0;
int (*orig_scePadReadState)(int handle, OrbisPadData *pData) = NULL;
int (*orig_scePadRead)(int handle, OrbisPadData *pData, int num) = NULL;

void ShowNotification(const char *text)
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

void ShowCurrentMenuOption()
{
    char *buf = (char *)calloc(128, 1);
    if (!buf) return;

    switch (currentOption) {
        case 0:
            snprintf(buf, 128, "WineBetamen\xC3\xBC (1/11) Godmode: %s", godmode ? "AN" : "AUS");
            break;
        case 1:
            snprintf(buf, 128, "WineBetamen\xC3\xBC (2/11) Super Sprung: %s", superJump ? "AN" : "AUS");
            break;
        case 2:
            snprintf(buf, 128, "WineBetamen\xC3\xBC (3/11) Keine Fahndung: %s", neverWanted ? "AN" : "AUS");
            break;
        case 3:
            snprintf(buf, 128, "WineBetamen\xC3\xBC (4/11) Teleport: Wegpunkt");
            break;
        case 4:
            snprintf(buf, 128, "WineBetamen\xC3\xBC (5/11) Teleport: Mount Chiliad");
            break;
        case 5:
            snprintf(buf, 128, "WineBetamen\xC3\xBC (6/11) Teleport: Flughafen LS");
            break;
        case 6:
            snprintf(buf, 128, "WineBetamen\xC3\xBC (7/11) Geld +10.000.000$");
            break;
        case 7:
            snprintf(buf, 128, "WineBetamen\xC3\xBC (8/11) Set Rank: 999");
            break;
        case 8:
            snprintf(buf, 128, "WineBetamen\xC3\xBC (9/11) Alle Waffen geben");
            break;
        case 9:
            snprintf(buf, 128, "WineBetamen\xC3\xBC (10/11) Fahrzeug: Zentorno");
            break;
        case 10:
            snprintf(buf, 128, "WineBetamen\xC3\xBC (11/11) Men\xC3\xBC schliessen");
            break;
    }
    ShowNotification(buf);
    free(buf);
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
    if (ret > 0 && pData && pData.connected) {
        g_gameButtons = pData.buttons;
    }
    return ret;
}

bool PatchGameImport(uint64_t baseAddr, size_t searchSize, void *targetFunc, void *hookFunc)
{
    if (!targetFunc || !hookFunc || baseAddr == 0) return false;
    uint64_t target = (uint64_t)targetFunc;
    uint64_t *ptr = (uint64_t *)baseAddr;
    size_t count = searchSize / sizeof(uint64_t);

    for (size_t i = 0; i < count; i++)
    {
        if (ptr[i] == target)
        {
            uint64_t pageStart = ((uint64_t)&ptr[i]) & ~0x3FFFULL;
            mprotect((void *)pageStart, 0x4000, PROT_READ | PROT_WRITE);
            ptr[i] = (uint64_t)hookFunc;
            return true;
        }
    }
    return false;
}

void* my_thread(void* args)
{
    sleep(10);

    int padLib = sceKernelLoadStartModule("libScePad.sprx", 0, NULL, 0, NULL, NULL);
    if (padLib > 0) {
        sceKernelDlsym(padLib, "scePadReadState", (void**)&orig_scePadReadState);
        sceKernelDlsym(padLib, "scePadRead", (void**)&orig_scePadRead);
    }

    bool hookedState = PatchGameImport(procInfo.base_address, 0x4500000, (void*)orig_scePadReadState, (void*)Hooked_scePadReadState);
    bool hookedRead  = PatchGameImport(procInfo.base_address, 0x4500000, (void*)orig_scePadRead, (void*)Hooked_scePadRead);

    char *welcome = (char *)calloc(128, 1);
    if (welcome) {
        snprintf(welcome, 128, "Welcome to WineBetamen\xC3\xBC by Winesaiya (Links + Viereck) [%d/%d]", hookedState, hookedRead);
        ShowNotification(welcome);
        free(welcome);
    }

    while (true)
    {
        uint32_t btns = g_gameButtons;

        if (btns != 0)
        {
            bool leftPressed   = (btns & ORBIS_PAD_BUTTON_LEFT);
            bool squarePressed = (btns & ORBIS_PAD_BUTTON_SQUARE);
            bool upPressed     = (btns & ORBIS_PAD_BUTTON_UP);
            bool downPressed   = (btns & ORBIS_PAD_BUTTON_DOWN);
            bool crossPressed  = (btns & ORBIS_PAD_BUTTON_CROSS);

            if (leftPressed && squarePressed)
            {
                menuOpen = !menuOpen;
                if (menuOpen) {
                    ShowCurrentMenuOption();
                } else {
                    ShowNotification("WineBetamen\xC3\xBC: GESCHLOSSEN");
                }
                sceKernelUsleep(350000);
            }
            else if (menuOpen)
            {
                if (downPressed)
                {
                    currentOption = (currentOption + 1) % TOTAL_OPTIONS;
                    ShowCurrentMenuOption();
                    sceKernelUsleep(250000);
                }
                else if (upPressed)
                {
                    currentOption = (currentOption - 1 + TOTAL_OPTIONS) % TOTAL_OPTIONS;
                    ShowCurrentMenuOption();
                    sceKernelUsleep(250000);
                }
                else if (crossPressed)
                {
                    if (currentOption == 0) {
                        godmode = !godmode;
                        ShowCurrentMenuOption();
                    } else if (currentOption == 1) {
                        superJump = !superJump;
                        ShowCurrentMenuOption();
                    } else if (currentOption == 2) {
                        neverWanted = !neverWanted;
                        ShowCurrentMenuOption();
                    } else if (currentOption == 3) {
                        ShowNotification("Teleportiert zum Wegpunkt! (Test)");
                    } else if (currentOption == 4) {
                        ShowNotification("Teleportiert zum Mount Chiliad! (Test)");
                    } else if (currentOption == 5) {
                        ShowNotification("Teleportiert zum Flughafen LS! (Test)");
                    } else if (currentOption == 6) {
                        ShowNotification("+10.000.000$ hinzugefuegt! (Test)");
                    } else if (currentOption == 7) {
                        ShowNotification("Rank auf 999 gesetzt! (Test)");
                    } else if (currentOption == 8) {
                        ShowNotification("Alle Waffen erhalten! (Test)");
                    } else if (currentOption == 9) {
                        ShowNotification("Zentorno gespawnt! (Test)");
                    } else if (currentOption == 10) {
                        menuOpen = false;
                        ShowNotification("WineBetamen\xC3\xBC: GESCHLOSSEN");
                        sceKernelUsleep(350000);
                        continue;
                    }
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
    if (sys_sdk_proc_info(&procInfo) != 0) {
        return -1;
    }
    pthread_t thread;
    pthread_create(&thread, NULL, my_thread, NULL);
    return 0;
}

int32_t attr_module_hidden module_stop(size_t argc, const void *args)
{
    return 0;
}
