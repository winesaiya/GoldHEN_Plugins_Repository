void CallNative9(uint64_t offset, uint32_t argCount, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7, uint64_t a8)
{
    if (!g_ebootBase) return;
    uint64_t argBuf __attribute__((aligned(16)));
    uint64_t retBuf __attribute__((aligned(16)));
    memset(argBuf, 0, sizeof(argBuf));
    memset(retBuf, 0, sizeof(retBuf));

    *(argBuf + 0) = a0; *(argBuf + 1) = a1; *(argBuf + 2) = a2;
    *(argBuf + 3) = a3; *(argBuf + 4) = a4; *(argBuf + 5) = a5;
    *(argBuf + 6) = a6; *(argBuf + 7) = a7; *(argBuf + 8) = a8;

    uint8_t ctxBuf __attribute__((aligned(16)));
    memset(ctxBuf, 0, sizeof(ctxBuf));
    scrNativeCallContext *ctx = (scrNativeCallContext *)ctxBuf;
    ctx->m_return_data = retBuf;
    ctx->m_argument_count = argCount;
    ctx->m_argument_data = argBuf;
    ctx->m_data_count = 0;

    RageNativeFn_t fn = (RageNativeFn_t)(g_ebootBase + offset);
    fn(ctx);
}

void GTA_DrawRect(float x, float y, float w, float h, int r, int g, int b, int a)
{
    CallNative9(0x9CCAC0, 9, FloatToU32(x), FloatToU32(y), FloatToU32(w), FloatToU32(h), (uint64_t)r, (uint64_t)g, (uint64_t)b, (uint64_t)a, 0);
}

void GTA_DrawText(const char *text, float x, float y, float scale, int font, int r, int g, int b, int a)
{
    (void)text; (void)x; (void)y; (void)scale; (void)font; (void)r; (void)g; (void)b; (void)a;
}
void CallNative9(uint64_t offset, uint32_t argCount, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7, uint64_t a8)
{
    if (!g_ebootBase) return;
    uint64_t *argBuf = (uint64_t *)calloc(16, sizeof(uint64_t));
    uint64_t *retBuf = (uint64_t *)calloc(8, sizeof(uint64_t));
    scrNativeCallContext *ctx = (scrNativeCallContext *)calloc(1, 256);
    if (!argBuf || !retBuf || !ctx) {
        if (argBuf) free(argBuf);
        if (retBuf) free(retBuf);
        if (ctx) free(ctx);
        return;
    }

    *(argBuf + 0) = a0; *(argBuf + 1) = a1; *(argBuf + 2) = a2;
    *(argBuf + 3) = a3; *(argBuf + 4) = a4; *(argBuf + 5) = a5;
    *(argBuf + 6) = a6; *(argBuf + 7) = a7; *(argBuf + 8) = a8;

    ctx->m_return_data = retBuf;
    ctx->m_argument_count = argCount;
    ctx->m_argument_data = argBuf;
    ctx->m_data_count = 0;

    RageNativeFn_t fn = (RageNativeFn_t)(g_ebootBase + offset);
    fn(ctx);

    free(ctx);
    free(retBuf);
    free(argBuf);
}

void GTA_DrawRect(float x, float y, float w, float h, int r, int g, int b, int a)
{
    CallNative9(0x9CCAC0, 9, FloatToU32(x), FloatToU32(y), FloatToU32(w), FloatToU32(h), (uint64_t)r, (uint64_t)g, (uint64_t)b, (uint64_t)a, 0);
}

void GTA_DrawText(const char *text, float x, float y, float scale, int font, int r, int g, int b, int a)
{
    (void)text; (void)x; (void)y; (void)scale; (void)font; (void)r; (void)g; (void)b; (void)a;
}
int64_t Hooked_GetPlayerPed(scrNativeCallContext *ctx)
{
    RageNativeFn_t orig = (RageNativeFn_t)Detour_PlayerPed.StubPtr;
    int64_t res = orig ? orig(ctx) : 0;

    g_inGameReady = true;

    static uint64_t lastDrawTick = 0;
    uint64_t now = sceKernelGetProcessTime();
    if (now - lastDrawTick > 14000) {
        lastDrawTick = now;
        RenderWinesaiyaInGame();
    }
    return res;
}
