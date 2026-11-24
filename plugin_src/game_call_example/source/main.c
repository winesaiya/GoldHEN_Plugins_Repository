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

uint64_t lastInputTime = 0;

Detour Detour_scePadReadState;
int (*orig_scePadReadState)(int handle, OrbisPadData *pData) = NULL;

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
            snprintf(buf, 128, "(1/11) Godmode: %s", godmode ? "AN" : "AUS");
            break;
        case 1:
            snprintf(buf, 128, "(2/11) Super Sprung: %s", superJump ? "AN" : "AUS");
            break;
        case 2:
            snprintf(buf, 128, "(3/11) Keine Fahndung: %s", neverWanted ? "AN" : "AUS");
            break;
        case 3:
            snprintf(buf, 128, "(4/11) Teleport: Wegpunkt");
            break;
        case 4:
            snprintf(buf, 128, "(5/11) Teleport: Mount Chiliad");
            break;
        case 5:
            snprintf(buf, 128, "(6/11) Teleport: Flughafen LS");
            break;
        case 6:
            snprintf(buf, 128, "(7/11) Geld +10.000.000$");
            break;
        case 7:
            snprintf(buf, 128, "(8/11) Set Rank: 999");
            break;
        case 8:
            snprintf(buf, 128, "(9/11) Alle Waffen geben");
            break;
        case 9:
            snprintf(buf, 128, "(10/11) Fahrzeug: Zentorno spawnen");
            break;
        case 10:
            snprintf(buf, 128, "(11/11) Men\xC3\xBC schliessen");
            break;
    }
    ShowNotification(buf);
    free(buf);
}

int scePadReadState_Hook(int handle, OrbisPadData *pData)
{
    int ret = orig_scePadReadState ? orig_scePadReadState(handle, pData) : -1;

    if (ret == 0 && pData && pData->connected)
    {
        uint64_t now = sceKernelGetProcessTime();
        if (now - lastInputTime > 250000)
        {
            bool leftPressed   = (pData->buttons & ORBIS_PAD_BUTTON_LEFT);
            bool squarePressed = (pData->buttons & ORBIS_PAD_BUTTON_SQUARE);
            bool upPressed     = (pData->buttons & ORBIS_PAD_BUTTON_UP);
            bool downPressed   = (pData->buttons & ORBIS_PAD_BUTTON_DOWN);
            bool crossPressed  = (pData->buttons & ORBIS_PAD_BUTTON_CROSS);

            if (leftPressed && squarePressed)
            {
                lastInputTime = now;
                menuOpen = !menuOpen;
                if (menuOpen) {
                    ShowNotification("=== WINEBETAMEN\xC3\x9C ===");
                    ShowCurrentMenuOption();
                } else {
                    ShowNotification("WineBetamen\xC3\xBC: GESCHLOSSEN");
                }
            }
            else if (menuOpen)
            {
                if (downPressed)
                {
                    lastInputTime = now;
                    currentOption = (currentOption + 1) % TOTAL_OPTIONS;
                    ShowCurrentMenuOption();
                }
                else if (upPressed)
                {
                    lastInputTime = now;
                    currentOption = (currentOption - 1 + TOTAL_OPTIONS) % TOTAL_OPTIONS;
                    ShowCurrentMenuOption();
                }
                else if (crossPressed)
                {
                    lastInputTime = now;
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
                    }
                }
            }
        }
    }
    return ret;
}

void* my_thread(void* args)
{
    sleep(10);
    ShowNotification("Welcome to WineBetamen\xC3\xBC by Winesaiya (Links + Viereck)");
    return NULL;
}

int32_t attr_module_hidden module_start(size_t argc, const void *args)
{
    if (sys_sdk_proc_info(&procInfo) != 0) {
        return -1;
    }

    int padLib = sceKernelLoadStartModule("libScePad.sprx", 0, NULL, 0, NULL, NULL);
    void *fnPadReadState = NULL;
    if (padLib > 0) {
        sceKernelDlsym(padLib, "scePadReadState", &fnPadReadState);
        if (fnPadReadState) {
            Detour_Construct(&Detour_scePadReadState, DetourMode_x64);
            orig_scePadReadState = (int (*)(int, OrbisPadData *))Detour_DetourFunction(&Detour_scePadReadState, (uint64_t)fnPadReadState, scePadReadState_Hook);
        }
    }

    pthread_t thread;
    pthread_create(&thread, NULL, my_thread, NULL);
    return 0;
}

int32_t attr_module_hidden module_stop(size_t argc, const void *args)
{
    Detour_Destroy(&Detour_scePadReadState);
    return 0;
}
