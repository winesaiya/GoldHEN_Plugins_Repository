#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "plugin_common.h"
#include <orbis/libkernel.h>

attr_public const char *g_pluginName = "Winee Menu Beta";
attr_public const char *g_pluginDesc = "Custom PS4 Plugin";
attr_public const char *g_pluginAuth = "winesaiya";
attr_public uint32_t g_pluginVersion = 0x00000100;

struct proc_info procInfo;
bool menuOpen = false;

#define BTN_L1    0x00000400
#define BTN_RIGHT 0x00000020

typedef struct {
    int32_t type;
    int32_t reqId;
    int32_t priority;
    int32_t msgId;
    int32_t targetId;
    int32_t userId;
    int32_t unk1;
    int32_t unk2;
    int32_t appId;
    int32_t errorNum;
    int32_t unk3;
    unsigned char useIconImageUri;
    char message[1024];
    char iconUri[1024];
    char unk[1024];
} OrbisNotificationRequest;

typedef struct {
    uint32_t buttons;
    uint8_t leftStickX;
    uint8_t leftStickY;
    uint8_t rightStickX;
    uint8_t rightStickY;
    uint8_t analogL2;
    uint8_t analogR2;
    uint8_t padding[2];
    uint8_t orientation[16];
    uint8_t acceleration[12];
    uint8_t angularVelocity[12];
    uint8_t touchData[24];
    bool connected;
    uint64_t timestamp;
    uint8_t ext[16];
    uint8_t count;
    uint8_t unknown[15];
} SimplePadData;

int (*fnPadInit)(void) = NULL;
int (*fnPadOpen)(int userId, int type, int index, void *param) = NULL;
int (*fnPadReadState)(int handle, SimplePadData *data) = NULL;

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

void* my_thread(void* args)
{
    sleep(15);

    int padLib = sceKernelLoadStartModule("libScePad.sprx", 0, NULL, 0, NULL, NULL);
    if (padLib > 0) {
        sceKernelDlsym(padLib, "scePadInit", (void**)&fnPadInit);
        sceKernelDlsym(padLib, "scePadOpen", (void**)&fnPadOpen);
        sceKernelDlsym(padLib, "scePadReadState", (void**)&fnPadReadState);
    }

    int padHandle = -1;
    if (fnPadInit && fnPadOpen) {
        fnPadInit();
        padHandle = fnPadOpen(0xFF, 0, 0, NULL);
    }

    ShowNotification("Winee Menu Beta geladen! (L1 + Rechts)");

    SimplePadData padData;
    memset(&padData, 0, sizeof(padData));

    while (true)
    {
        if (padHandle > 0 && fnPadReadState && fnPadReadState(padHandle, &padData) == 0)
        {
            bool l1Pressed = (padData.buttons & BTN_L1);
            bool rightPressed = (padData.buttons & BTN_RIGHT);

            if (l1Pressed && rightPressed)
            {
                menuOpen = !menuOpen;
                if (menuOpen) {
                    ShowNotification("Winee Menu Beta: GEOEFFNET");
                } else {
                    ShowNotification("Winee Menu Beta: GESCHLOSSEN");
                }
                sceKernelUsleep(300000);
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
