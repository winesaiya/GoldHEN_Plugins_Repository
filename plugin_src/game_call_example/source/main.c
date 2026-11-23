// Eigenes GoldHEN PS4 Plugin
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "plugin_common.h"
#include <orbis/libkernel.h>

// HIER DEINEN EIGENEN MENÜ-NAMEN EINTRAGEN:
attr_public const char *g_pluginName = "MEIN EIGENES MENU";
attr_public const char *g_pluginDesc = "Custom PS4 Plugin";
attr_public const char *g_pluginAuth = "winesaiya";
attr_public uint32_t g_pluginVersion = 0x00000100;

struct proc_info procInfo;

// Hilfsfunktion für PS4-Benachrichtigung oben links auf dem TV
void sendNotification(const char* text) {
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof(req));
    req.type = NotificationRequest;
    req.unk3 = 0;
    req.useIconImageUri = 1;
    req.targetId = -1;
    strcpy(req.iconUri, "cxml://psnotification/tex_icon_system");
    strncpy(req.message, text, sizeof(req.message) - 1);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

void* my_thread(void* args)
{
    // 15 Sekunden warten, bis das Spiel geladen ist
    sleep(15);
    
    // Zeigt deinen eigenen Namen als Pop-up auf der PS4 an!
    sendNotification("Mein Custom Menu wurde erfolgreich geladen!");

    while (true)
    {
        sceKernelUsleep(1000000); // 1 Sekunde Pause im Hintergrund
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
    scePthreadCreate(&thread, NULL, my_thread, NULL, "custom_menu_thread");
    return 0;
}

int32_t attr_module_hidden module_stop(size_t argc, const void *args)
{
    return 0;
}


