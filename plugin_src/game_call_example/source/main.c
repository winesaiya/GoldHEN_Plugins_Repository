#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "plugin_common.h"
#include <orbis/libkernel.h>

attr_public const char *g_pluginName = "Winee Menu Beta";
attr_public const char *g_pluginDesc = "Custom PS4 Plugin";
attr_public const char *g_pluginAuth = "winesaiya";
attr_public uint32_t g_pluginVersion = 0x00000100;

struct proc_info procInfo;
bool menuOpen = false;

// Controller-Struktur und Tasten-Codes direkt definiert (kein Makefile-Edit nötig!)
#define BTN_L1    0x00000400
#define BTN_RIGHT 0x00000020

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

void* my_thread(void* args)
{
    sleep(15);

    // Controller-Bibliothek dynamisch laden
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

    SimplePadData padData;
    memset(&padData, 0, sizeof(padData));

    while (true)
    {
        if (padHandle > 0 && fnPadReadState && fnPadReadState(padHandle, &padData) == 0)
        {
            // Prüft ob L1 UND Pfeil Rechts gleichzeitig gedrückt werden
            bool l1Pressed = (padData.buttons & BTN_L1);
            bool rightPressed = (padData.buttons & BTN_RIGHT);

            if (l1Pressed && rightPressed)
            {
                menuOpen = !menuOpen;
                if (menuOpen) {
                    final_printf("[Winee Menu Beta] GEÖFFNET (L1 + Rechts)\n");
                } else {
                    final_printf("[Winee Menu Beta] GESCHLOSSEN\n");
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
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
    if (sys_sdk_proc_info(&procInfo) != 0) {
        return -1;
    }
    ScePthread thread;
    scePthreadCreate(&thread, NULL, my_thread, NULL, "winee_menu_thread");
    return 0;
}

int32_t attr_module_hidden module_stop(size_t argc, const void *args)
{
    return 0;
}





