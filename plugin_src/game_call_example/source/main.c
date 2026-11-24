#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "plugin_common.h"
#include <orbis/libkernel.h>

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

#define TOTAL_OPTIONS 10

#define BTN_UP     0x00000010
#define BTN_RIGHT  0x00000020
#define BTN_DOWN   0x00000040
#define BTN_LEFT   0x00000080
#define BTN_CROSS  0x00004000
#define BTN_SQUARE 0x00008000

typedef struct {
    uint32_t buttons;
    uint8_t leftStickX;
    uint8_t leftStickY;
    uint8_t rightStickX;
    uint8_t rightStickY;
    uint8_t analogL2;
    uint8_t analogR2;
    uint8_t padding;
    uint8_t orientation;
    uint8_t acceleration;
    uint8_t angularVelocity;
    uint8_t touchData;
    bool connected;
    uint64_t timestamp;
    uint8_t ext;
    uint8_t count;
    uint8_t unknown;
} SimplePadData;

int (*fnPadInit)(void) = NULL;
int (*fnPadOpen)(int userId, int type, int index, void *param) = NULL;
int (*fnPadReadState)(int handle, SimplePadData *data) = NULL;
int (*fnGetInitialUser)(int *userId) = NULL;

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
    char buf;
    switch (currentOption) {
        case 0:
            snprintf(buf, sizeof(buf), "[1/10] Godmode: %s", godmode ? "AN" : "AUS");
            break;
        case 1:
            snprintf(buf, sizeof(buf), "[2/10] Super Sprung: %s", superJump ? "AN" : "AUS");
            break;
        case 2:
            snprintf(buf, sizeof(buf), "[3/10] Keine Fahndung: %s", neverWanted ? "AN" : "AUS");
            break;
        case 3:
            snprintf(buf, sizeof(buf), "[4/10] Teleport: Wegpunkt");
            break;
        case 4:
            snprintf(buf, sizeof(buf), "[5/10] Teleport: Mount Chiliad");
            break;
        case 5:
            snprintf(buf, sizeof(buf), "[6/10] Teleport: Flughafen LS");
            break;
        case 6:
            snprintf(buf, sizeof(buf), "[7/10] Geld +10.000.000$");
            break;
        case 7:
            snprintf(buf, sizeof(buf), "[8/10] Alle Waffen geben");
            break;
        case 8:
            snprintf(buf, sizeof(buf), "[9/10] Fahrzeug: Zentorno spawnen");
            break;
        case 9:
            snprintf(buf, sizeof(buf), "[10/10] Men\xC3\xBC schliessen");
            break;
    }
    ShowNotification(buf);
}

void* my_thread(void* args)
{
    sleep(55);

    int userId = 0xFF;
    int userLib = sceKernelLoadStartModule("libSceUserService.sprx", 0, NULL, 0, NULL, NULL);
    if (userLib > 0) {
        sceKernelDlsym(userLib, "sceUserServiceGetInitialUser", (void**)&fnGetInitialUser);
        if (fnGetInitialUser) {
            fnGetInitialUser(&userId);
        }
    }

    int padLib = sceKernelLoadStartModule("libScePad.sprx", 0, NULL, 0, NULL, NULL);
    if (padLib > 0) {
        sceKernelDlsym(padLib, "scePadInit", (void**)&fnPadInit);
        sceKernelDlsym(padLib, "scePadOpen", (void**)&fnPadOpen);
        sceKernelDlsym(padLib, "scePadReadState", (void**)&fnPadReadState);
    }

    int padHandle = -1;
    if (fnPadInit && fnPadOpen) {
        fnPadInit();
        padHandle = fnPadOpen(userId, 0, 0, NULL);
    }

    ShowNotification("Welcome to WineBetamen\xC3\xBC");

    SimplePadData padData;
    memset(&padData, 0, sizeof(padData));

    while (true)
    {
        if (padHandle >= 0 && fnPadReadState && fnPadReadState(padHandle, &padData) == 0)
        {
            bool leftPressed   = (padData.buttons & BTN_LEFT);
            bool squarePressed = (padData.buttons & BTN_SQUARE);
            bool upPressed     = (padData.buttons & BTN_UP);
            bool downPressed   = (padData.buttons & BTN_DOWN);
            bool crossPressed  = (padData.buttons & BTN_CROSS);

            if (leftPressed && squarePressed)
            {
                menuOpen = !menuOpen;
                if (menuOpen) {
                    ShowNotification("=== WINEBETAMEN\xC3\x9C ===");
                    sceKernelUsleep(250000);
                    ShowCurrentMenuOption();
                } else {
                    ShowNotification("WineBetamen\xC3\xBC: GESCHLOSSEN");
                }
                sceKernelUsleep(300000);
            }
            else if (menuOpen)
            {
                if (downPressed)
                {
                    currentOption = (currentOption + 1) % TOTAL_OPTIONS;
                    ShowCurrentMenuOption();
                    sceKernelUsleep(220000);
                }
                else if (upPressed)
                {
                    currentOption = (currentOption - 1 + TOTAL_OPTIONS) % TOTAL_OPTIONS;
                    ShowCurrentMenuOption();
                    sceKernelUsleep(220000);
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
                        ShowNotification("Alle Waffen erhalten! (Test)");
                    } else if (currentOption == 8) {
                        ShowNotification("Zentorno gespawnt! (Test)");
                    } else if (currentOption == 9) {
                        menuOpen = false;
                        ShowNotification("WineBetamen\xC3\xBC: GESCHLOSSEN");
                        sceKernelUsleep(300000);
                        continue;
                    }
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
