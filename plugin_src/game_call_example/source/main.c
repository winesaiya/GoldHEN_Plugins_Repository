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

typedef struct {
    void *address;
    void *address2;
} VideoOutBufferInfo;

int (*orig_sceVideoOutRegisterBuffers)(int handle, int startIndex, void * const *addresses, int bufferNum, const void *attribute) = NULL;
int (*orig_sceVideoOutSubmitFlip)(int handle, int bufferIndex, int flipMode, int64_t flipArg) = NULL;

void *g_fbAddresses;
int g_fbWidth = 1920;
int g_fbHeight = 1080;
int g_fbPitch = 1920;

// Kompakte 5x7 Pixel-Schriftart (A-Z, 0-9, Sonderzeichen)
static const uint8_t font5x7 = {
    {0x00,0x00,0x00,0x00,0x00}, // ' ' (32)
    {0x00,0x00,0x5F,0x00,0x00}, // '!' (33)
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$'
    {0x23,0x13,0x08,0x64,0x62}, // '%'
    {0x36,0x49,0x55,0x22,0x50}, // '&'
    {0x00,0x05,0x03,0x00,0x00}, // '''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x14,0x08,0x3E,0x08,0x14}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x50,0x30,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x60,0x60,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00}, // ';'
    {0x08,0x14,0x22,0x41,0x00}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x00,0x41,0x22,0x14,0x08}, // '>'
    {0x02,0x01,0x51,0x09,0x06}, // '?'
    {0x32,0x49,0x79,0x41,0x3E}, // '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x0C,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // '['
    {0x02,0x04,0x08,0x10,0x20}, // '\'
    {0x00,0x41,0x41,0x7F,0x00}  // ']'
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
        for (int px = x; px < x + w; px++) {
            row[px] = color;
        }
    }
}

void DrawChar(uint32_t *fb, int x, int y, char c, int scale, uint32_t color)
{
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c < 32 || c > 93) c = ' ';
    int idx = c - 32;
    for (int col = 0; col < 5; col++) {
        uint8_t bits = font5x7[idx][col];
        for (int row = 0; row < 7; row++) {
            if (bits & (1 << row)) {
                DrawRect(fb, x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

void DrawText(uint32_t *fb, int x, int y, const char *str, int scale, uint32_t color)
{
    if (!fb || !str) return;
    int cx = x;
    while (*str) {
        DrawChar(fb, cx, y, *str, scale, color);
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

    // Farben (ARGB: 0xAARRGGBB)
    const uint32_t COL_NEON_GREEN = 0xFF39FF14;
    const uint32_t COL_BLACK      = 0xFF0A0A0A;
    const uint32_t COL_DARK_BG    = 0xFF111111;
    const uint32_t COL_WHITE      = 0xFFFFFFFF;

    int boxW = 460;
    int boxX = g_fbWidth - boxW - 70; // Rechts auf dem Bildschirm
    int boxY = 110;

    int headerH = 74;
    int subH    = 38;
    int rowH    = 44;
    int total   = GetOptionCount(currentMenu);
    int bodyH   = total * rowH + 10;
    int totalH  = headerH + subH + bodyH;

    // Neon-gruener Aussenrahmen (Glow-Effekt)
    DrawRect(fb, boxX - 4, boxY - 4, boxW + 8, totalH + 8, COL_NEON_GREEN);

    // Header-Banner (Neon-Gruen mit fetter schwarzer Schrift WINESAIYA)
    DrawRect(fb, boxX, boxY, boxW, headerH, COL_NEON_GREEN);
    DrawText(fb, boxX + 24, boxY + 20, "WINESAIYA", 5, COL_BLACK);

    // Subheader (Schwarz mit neon-gruener Kategorie & Seitenzahl)
    int subY = boxY + headerH;
    DrawRect(fb, boxX, subY, boxW, subH, COL_BLACK);

    const char *catName = "HAUPTMENU";
    if (currentMenu == 1) catName = "SPIELER";
    else if (currentMenu == 2) catName = "TELEPORT";
    else if (currentMenu == 3) catName = "GELD & RANK";
    else if (currentMenu == 4) catName = "WAFFEN";
    else if (currentMenu == 5) catName = "FAHRZEUGE";

    DrawText(fb, boxX + 18, subY + 11, catName, 2, COL_NEON_GREEN);

    char *counter = (char *)calloc(32, 1);
    if (counter) {
        snprintf(counter, 32, "%d/%d", currentOption + 1, total);
        DrawText(fb, boxX + boxW - 65, subY + 11, counter, 2, COL_NEON_GREEN);
        free(counter);
    }

    // Menue-Hintergrund (Tiefschwarz)
    int bodyY = subY + subH;
    DrawRect(fb, boxX, bodyY, boxW, bodyH, COL_DARK_BG);

    // Zeilen & Neon-Gruener Auswahlbalken
    for (int i = 0; i < total; i++)
    {
        int ry = bodyY + 5 + i * rowH;
        bool selected = (i == currentOption);

        if (selected) {
            DrawRect(fb, boxX + 6, ry, boxW - 12, rowH - 4, COL_NEON_GREEN);
        }

        uint32_t textCol = selected ? COL_BLACK : COL_WHITE;
        const char *label = "";
        const char *stateTxt = "";

        if (currentMenu == 0) {
            if (i == 0) { label = "SPIELER"; stateTxt = ">"; }
            else if (i == 1) { label = "TELEPORT"; stateTxt = ">"; }
            else if (i == 2) { label = "GELD & RANK"; stateTxt = ">"; }
            else if (i == 3) { label = "WAFFEN"; stateTxt = ">"; }
            else if (i == 4) { label = "FAHRZEUGE"; stateTxt = ">"; }
        } else if (currentMenu == 1) {
            if (i == 0) { label = "GODMODE"; stateTxt = godmode ? "[AN]" : "[AUS]"; }
            else if (i == 1) { label = "SUPER SPRUNG"; stateTxt = superJump ? "[AN]" : "[AUS]"; }
            else if (i == 2) { label = "KEINE FAHNDUNG"; stateTxt = neverWanted ? "[AN]" : "[AUS]"; }
        } else if (currentMenu == 2) {
            if (i == 0) label = "WEGPUNKT";
            else if (i == 1) label = "MOUNT CHILIAD";
            else if (i == 2) label = "FLUGHAFEN LS";
        } else if (currentMenu == 3) {
            if (i == 0) label = "GELD +10.000.000$";
            else if (i == 1) label = "SET RANK 999";
        } else if (currentMenu == 4) {
            label = "ALLE WAFFEN GEBEN";
        } else if (currentMenu == 5) {
            label = "ZENTORNO SPAWNEN";
        }

        DrawText(fb, boxX + 20, ry + 12, label, 2, textCol);
        if (stateTxt[0] != '\0') {
            uint32_t stateCol = selected ? COL_BLACK : (strstr(stateTxt, "AN") ? COL_NEON_GREEN : COL_WHITE);
            DrawText(fb, boxX + boxW - 95, ry + 12, stateTxt, 2, stateCol);
        }
    }
}

void ShowPS4Notification(const char *text)
{
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof(req));
    req.type = 0;
    req.useIconImageUri = 0;
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
    if (addresses && bufferNum > 0) {
        for (int i = 0; i < bufferNum && (startIndex + i) < 16; i++) {
            g_fbAddresses[startIndex + i] = addresses[i];
        }
    }
