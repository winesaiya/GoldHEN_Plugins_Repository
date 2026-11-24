// Eigenes GoldHEN PS4 Plugin
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

void* my_thread(void* args)
{
    sleep(15);

    sys_sdk_notification("Winee Menu Beta wurde geladen!");

    while (true)
    {
        sceKernelUsleep(1000000);
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




