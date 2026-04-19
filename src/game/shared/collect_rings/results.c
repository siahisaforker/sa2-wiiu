#include "global.h"
#include "core.h"
#include "flags.h"
#include "bg_triangles.h"
#include "multi_sio.h"
#include "sio32_multi_load.h" // for SIO32ML_BLOCK_SIZE

#include "lib/m4a/m4a.h"

#include "game/globals.h"
#include "game/shared/stage/player.h"
#include "game/shared/stage/stage.h"
#include "game/shared/multiplayer/communication_outcome.h"

#if COLLECT_RINGS_ROM
#include "game/shared/stage/mp_player.h"
#endif

#if (GAME == GAME_SA1)
#include "game/sa1/save.h"
#include "game/sa1/stage/tilemap_table.h"
#include "game/sa1/menus/mp_mode_select_2.h"

#include "data/sa1/sprite_tables.h"
#include "data/sa1/collect_rings.h"

#include "constants/sa1/animations.h"
#include "constants/sa1/songs.h"
#include "constants/sa1/text.h"
#include "constants/sa1/tilemaps.h"
#include "constants/sa1/zones.h"
#elif (GAME == GAME_SA2)
#include "game/sa2/save.h"
#include "game/sa2/title_screen.h"
#include "game/sa2/collect_rings/singlepak_connection.h"
#include "game/sa2/multiplayer/multipak_connection.h"

#include "game/sa2/stage/screen_fade.h"
#include "game/sa2/stage/tilemap_table.h"

#include "data/sa2/collect_rings.h"
#include "data/sa2/sprite_data.h"

#include "constants/sa2/animations.h"
#include "constants/sa2/songs.h"
#include "constants/sa2/text.h"
#include "constants/sa2/tilemaps.h"
#endif

// This is probably a non match but works on both games
#define BASE_ADDR(i)                                                                                                                       \
    ({                                                                                                                                     \
        s32 r8 = (i);                                                                                                                      \
        r8 << 14;                                                                                                                          \
    })

#define INIT_1()                                                                                                                           \
    gDispCnt = DISPCNT_OBJ_1D_MAP;                                                                                                         \
    gBgCntRegs[3] = 0x5E0B;                                                                                                                \
    gBgCntRegs[2] = 0x1C0C;                                                                                                                \
    gBgSprites_Unknown1[2] = 0x7F;                                                                                                         \
    gBgSprites_Unknown2[2][0] = 0;                                                                                                         \
    gBgSprites_Unknown2[2][1] = 0;                                                                                                         \
    gBgSprites_Unknown2[2][2] = 0xFF;                                                                                                      \
    gBgSprites_Unknown2[2][3] = 0x20;                                                                                                      \
    gBgSprites_Unknown1[3] = -1;                                                                                                           \
    gBgSprites_Unknown2[3][0] = 0;                                                                                                         \
    gBgSprites_Unknown2[3][1] = 0;                                                                                                         \
    gBgSprites_Unknown2[3][2] = -1;                                                                                                        \
    gBgSprites_Unknown2[3][3] = 0x40;                                                                                                      \
    DmaFill32(3, 0, VRAM + 0x9FE0, 0x40);                                                                                                  \
    DmaFill32(3, 0, VRAM + 0xFE0 + BASE_ADDR(3), 0x40);

#define INIT_SCREEN(m)                                                                                                                     \
    ({                                                                                                                                     \
        resultsScreen = TASK_DATA(                                                                                                         \
            TaskCreate(Task_MultiplayerSinglePakResultsScreenInit, sizeof(struct MultiplayerSinglePakResultsScreen), 0x2000, 0, NULL));    \
        resultsScreen->mode = (m);                                                                                                         \
        resultsScreen->unk430 = 0;                                                                                                         \
        resultsScreen->actor = 0;                                                                                                          \
    })

#define INIT_BG(mBg, id)                                                                                                                   \
    ({                                                                                                                                     \
        Background *bg = &(mBg);                                                                                                           \
        bg->graphics.dest = (void *)BG_SCREEN_ADDR(16);                                                                                    \
        bg->graphics.anim = 0;                                                                                                             \
        bg->layoutVram = (void *)BG_SCREEN_ADDR(30);                                                                                       \
        bg->unk18 = 0;                                                                                                                     \
        bg->unk1A = 0;                                                                                                                     \
        bg->tilemapId = (id);                                                                                                              \
        bg->unk1E = 0;                                                                                                                     \
        bg->unk20 = 0;                                                                                                                     \
        bg->unk22 = 0;                                                                                                                     \
        bg->unk24 = 0;                                                                                                                     \
        bg->targetTilesX = 0x1E;                                                                                                           \
        bg->targetTilesY = 0x19;                                                                                                           \
        bg->paletteOffset = 0;                                                                                                             \
        bg->flags = BACKGROUND_FLAGS_BG_ID(3);                                                                                             \
        DrawBackground(bg);                                                                                                                \
    })

#define RS_SPRITE_INIT(s, _vramAddr, _animId, _variant, _x, _y, _oamFlags, _unk25, _unk10)                                                 \
    s->x = (_x);                                                                                                                           \
    s->y = (_y);                                                                                                                           \
    s->graphics.dest = (_vramAddr);                                                                                                        \
    s->oamFlags = (_oamFlags);                                                                                                             \
    s->graphics.size = 0;                                                                                                                  \
    s->graphics.anim = (_animId);                                                                                                          \
    s->variant = (_variant);                                                                                                               \
    s->animCursor = 0;                                                                                                                     \
    s->qAnimDelay = 0;                                                                                                                     \
    s->prevVariant = -1;                                                                                                                   \
    s->animSpeed = SPRITE_ANIM_SPEED(1.0);                                                                                                 \
    s->palId = (_unk25);                                                                                                                   \
    s->frameFlags = (_unk10);                                                                                                              \
    UpdateSpriteAnimation(s);

// TODO: Is this a "Sprite2" (Sprite with 2 hitboxes)?
typedef struct MultiplayerSinglePakResultsScreen_UNK80 {
    Sprite unk0;
    u8 filler30[8];
} MultiplayerSinglePakResultsScreen_UNK80;

typedef struct MultiplayerSinglePakResultsScreen {
    Background unk0;
    Background unk40;
    struct MultiplayerSinglePakResultsScreen_UNK80 unk80[4];
    Sprite unk160[10];
    Sprite unk340;
    Sprite unk370[3];
    Sprite unk400;
    u16 unk430;
    u16 unk432;
    u32 mode;
    u8 actor;
    u8 filler439[3];
    u32 unk43C;
} MultiplayerSinglePakResultsScreen; /* size 0x440 */

struct MultiplayerSinglePakResultsScreen *InitScreen(s16);

void Task_MultiplayerSinglePakResultsScreenInit(void);
void SA2_LABEL(sub_80823FC)(void);
void SA2_LABEL(Task_8082630)(void);
void SA2_LABEL(sub_808267C)(void);
void SA2_LABEL(sub_8082788)(void);
void SA2_LABEL(sub_8082AA8)(void);

#if (GAME == GAME_SA2)
void Init1(void);
void InitBg(struct MultiplayerSinglePakResultsScreen *);
void sub_8082B80(struct MultiplayerSinglePakResultsScreen *);
void sub_8082C58(struct MultiplayerSinglePakResultsScreen *);
void sub_8082CB4(struct MultiplayerSinglePakResultsScreen *);
void sub_8082BF8(struct MultiplayerSinglePakResultsScreen *);
void sub_8082CEC(Sprite *s, void *vramAddr, u16 animId, u8 variant, s16 x, s16 y, u16 oamFlags, u8 unk25, u32 unk10);
#endif

#if (GAME == GAME_SA1)
void LinkCommunicationError(void)
{
    m4aMPlayAllStop();
    gFlags &= ~FLAGS_EXECUTE_HBLANK_COPY;
    gDispCnt = 0x40;
    gMultiSioEnabled = FALSE;
    MultiSioStop();
    MultiSioInit(0);

    gTilemapsRef = (Tilemap **)gTilemaps;
    gRefSpriteTables = &gSpriteTables;
    gMultiplayerMissingHeartbeats[0] = 0;
    gMultiplayerMissingHeartbeats[1] = 0;
    gMultiplayerMissingHeartbeats[2] = 0;
    gMultiplayerMissingHeartbeats[3] = 0;
    CreateMultipackOutcomeScreen(OUTCOME_CONNECTION_ERROR);
}
#endif

// TODO: Maybe rename because this is also called before the 1st round?
void CreateMultiplayerSinglePakResultsScreen(u32 a)
{
    Sprite *s;
    u32 i;
    struct MultiplayerSinglePakResultsScreen *resultsScreen;
#if (GAME == GAME_SA2)
#ifndef COLLECT_RINGS_ROM
    const u8 *tilemaps = gCollectRingsTilemaps;
    const u8 *bgStageTileset = gCollectRingsBgStageTileset;
#endif
#endif

    gMultiplayerMissingHeartbeats[3] = 0;
    gMultiplayerMissingHeartbeats[2] = 0;
    gMultiplayerMissingHeartbeats[1] = 0;
    gMultiplayerMissingHeartbeats[0] = 0;
    gWinRegs[WINREG_WININ] = 0;
    gWinRegs[WINREG_WINOUT] = 0;
    gWinRegs[WINREG_WIN0H] = 0;
    gWinRegs[WINREG_WIN0V] = 0;
    gWinRegs[WINREG_WIN1H] = 0;
    gWinRegs[WINREG_WIN1V] = 0;
    gStageFlags = 0;
    gMultiSioEnabled = TRUE;
    gFlags &= ~0x4000;
    m4aSoundVSyncOn();
    gGameMode = GAME_MODE_MULTI_PLAYER_COLLECT_RINGS;
#if (GAME == GAME_SA1)
    CpuFastCopy(gCollectRingsTilemaps, (void *)BG_VRAM, SIO32ML_BLOCK_SIZE);
    CpuFastCopy(gCollectRingsBgStageTileset, (void *)EWRAM_START + 0x33000, EWRAM_SIZE - 0x33000);
#elif (GAME == GAME_SA2)
#ifndef COLLECT_RINGS_ROM
    CpuFastCopy(bgStageTileset, (void *)BG_VRAM, SIO32ML_BLOCK_SIZE);
    CpuFastCopy(tilemaps, (void *)EWRAM_START + 0x33000, EWRAM_SIZE - 0x33000);
#endif
#endif

    gTilemapsRef = *((Tilemap ***)(EWRAM_START + 0x33000));
#if (GAME == GAME_SA2)
#if COLLECT_RINGS_ROM
    gRefSpriteTables = (const struct SpriteTables *)(EWRAM_START + 0x20000);
    gMultiplayerLanguage = 0;
#else
    gMultiplayerLanguage = LOADED_SAVE->language;
#endif
#endif

// The code is the same they just split it out into functions
#if (GAME == GAME_SA1)
    INIT_1();
    INIT_SCREEN(a);
    resultsScreen->unk43C = LOADED_SAVE->unk4 + gFrameCount;
    INIT_BG(resultsScreen->unk0, 4);

    for (i = 0; i < (s32)ARRAY_COUNT(resultsScreen->unk80); i++) {
        s = &resultsScreen->unk80[i].unk0;

        RS_SPRITE_INIT(s, OBJ_VRAM0 + (i << 0xB), gPlayerCharacterIdleAnims[i], 0, 120, 0x14 + (i * 0x28), SPRITE_OAM_ORDER(16), 0, 0x1000);
    }

    for (i = 0; i < (s32)ARRAY_COUNT(resultsScreen->unk160); i++) {
        void *vram = OBJ_VRAM0 + 0x2000 + (i * 0x80);
        u32 oamFlags = SPRITE_OAM_ORDER(4); // required to be here for match

        s = &resultsScreen->unk160[i];
        RS_SPRITE_INIT(s, vram, 0x380, (i + 3), 0, 0, oamFlags, 0, 0x1000);
    }

    if (resultsScreen->mode != 0) {
        for (i = 0; i < (s32)ARRAY_COUNT(resultsScreen->unk370); i++) {
            s = &resultsScreen->unk370[i];
            // Doesn't match macro and I CBA
            s->graphics.dest = (void *)(OBJ_VRAM0 + 0x2500 + (i * UNK_GFX_SIZE));
            s->x = 0;
            s->y = 0;
            s->oamFlags = SPRITE_OAM_ORDER(4);
            s->graphics.size = 0;
            s->graphics.anim = 0x380;
            s->variant = i;
            s->animCursor = 0;
            s->qAnimDelay = 0;
            s->prevVariant = -1;
            s->animSpeed = SPRITE_ANIM_SPEED(1.0);
            s->palId = 0;
            s->frameFlags = 0x1000;
            UpdateSpriteAnimation(s);
        }
    } else {
        s = &resultsScreen->unk340;
        RS_SPRITE_INIT(s, (void *)OBJ_VRAM0 + 0x2500, 0x380, 13, 0, 0, SPRITE_OAM_ORDER(4), 0, 0x1000);
    }

    if (resultsScreen->mode != 0) {
        m4aSongNumStart(MUS_VS_RESULT);
    } else {
        m4aSongNumStart(MUS_VS_SCORE_OVERVIEW);
    }

    gBldRegs.bldCnt = 0xFF;
    gBldRegs.bldY = 0x10;
#elif (GAME == GAME_SA2)
    Init1();
    resultsScreen = InitScreen(a);
    InitBg(resultsScreen);
    sub_8082B80(resultsScreen);
    sub_8082BF8(resultsScreen);

    if (a != 0) {
        sub_8082C58(resultsScreen);
        m4aSongNumStart(MUS_VS_LOBBY);
    } else {
        sub_8082CB4(resultsScreen);
        m4aSongNumStart(MUS_VS_SELECT_PAK_MODE);
    }

    gBldRegs.bldCnt = 0;
    gBldRegs.bldAlpha = 0;
    gBldRegs.bldY = 0;
#endif
}

#if (GAME == GAME_SA2)
void Init1(void) { INIT_1(); }

void InitBg(struct MultiplayerSinglePakResultsScreen *screen) { INIT_BG(screen->unk0, TM_LEVEL_BG(LEVEL_INDEX(ZONE_1, ACT_2))); }

#if COLLECT_RINGS_ROM
void sub_8082B80(struct MultiplayerSinglePakResultsScreen *resultsScreen)
{
    s16 i;
    for (i = 0; i < 4; i++) {
        u16 val;
        if (i == SIO_MULTI_CNT->id) {
            val = 0;
        } else {
            val = gUnknown_02015B18[i];
        }
        sub_8082CEC(&resultsScreen->unk80[i].unk0, OBJ_VRAM0 + (i * 0x800), val, 0, 0x78, i * 40 + 20, SPRITE_OAM_ORDER(16), i, 0x1000);
    }
}
#endif
#endif

void Task_MultiplayerSinglePakResultsScreenInit(void)
{
    u32 i;
    Sprite *s;
    struct MultiplayerSinglePakResultsScreen *resultsScreen;
    gDispCnt |= 0x1800;
    gMultiplayerConnections = EXTRACT_REGISTERED_CONNECTIONS(gMultiSioStatusFlags);
    LINK_HEARTBEAT();

    gMultiSioSend.pat0.unk0 = 0x4010;
    if (gMultiSioStatusFlags & MULTI_SIO_PARENT) {
        gMultiSioSend.pat0.unk3 = gMultiplayerLanguage;
    }
#if COLLECT_RINGS_ROM
    else if (gMultiSioRecv->pat0.unk0 == 0x4010) {
        gMultiplayerLanguage = gMultiSioRecv->pat0.unk3;
    }
#endif

    resultsScreen = TASK_DATA(gCurTask);
#ifndef NON_MATCHING
    while (0)
        ;
#endif
    gBldRegs.bldCnt = 0;
    gBldRegs.bldY = 0;

    if (++resultsScreen->unk430 > 0xF0) {
#ifndef COLLECT_RINGS_ROM
        gFlags &= ~0x8000;
#endif

        if (resultsScreen->mode) {
            for (i = 0; i < 3; i++) {
                s32 temp;
                s = &resultsScreen->unk370[i];
                s->graphics.dest = (void *)(OBJ_VRAM0 + 0x2500 + (i * 0x180));

                s->x = 0;
                s->y = 0;
                s->oamFlags = SPRITE_OAM_ORDER(4);
                s->graphics.size = 0;

// Non match required for non japan main rom
#if !defined(NON_MATCHING) && !defined(JAPAN)
#if COLLECT_RINGS_ROM
                do
#endif
                {

                    s16 var = SA2_ANIM_MP_SINGLE_PAK_RESULTS_ROUND;
                    asm("" ::"r"(var));
                }
#if COLLECT_RINGS_ROM

                while (0);
#endif
#endif

                switch (gMultiplayerLanguage) {
#ifdef JAPAN
                    case LANG_DEFAULT:
                        s->graphics.anim = SA2_ANIM_MP_SINGLE_PAK_RESULTS_CUMULATIVE;
                        break;
#endif
                    case LANG_JAPANESE:
                        s->graphics.anim = SA2_ANIM_MP_SINGLE_PAK_RESULTS_CUMULATIVE;
                        break;
#ifdef JAPAN
                    case LANG_ENGLISH:
#endif
                    default:
                        s->graphics.anim = SA2_ANIM_MP_SINGLE_PAK_RESULTS_ROUND;
                        break;
                }

                s->variant = i;
                s->animCursor = 0;
                s->qAnimDelay = 0;
                s->prevVariant = -1;
                s->animSpeed = SPRITE_ANIM_SPEED(1.0);
                s->palId = 0;
                s->frameFlags = 0x1000;
                UpdateSpriteAnimation(s);
            }
        }
#ifndef COLLECT_RINGS_ROM
        if (gMultiSioStatusFlags & MULTI_SIO_PARENT) {
            s = &resultsScreen->unk400;
            s->x = DISPLAY_CENTER_X;
            s->y = DISPLAY_CENTER_Y;

            if (resultsScreen->mode) {
                s->graphics.dest = resultsScreen->unk370[2].graphics.dest + 0x180;
            } else {
                s->graphics.dest = resultsScreen->unk340.graphics.dest + 0x180;
            }
            s->oamFlags = SPRITE_OAM_ORDER(0);
            s->graphics.size = 0;

            switch (gMultiplayerLanguage) {
#ifdef JAPAN
                case LANG_DEFAULT:
                    s->graphics.anim = SA2_ANIM_PRESS_START_MSG_JP;
                    break;
#endif
                case LANG_JAPANESE:
                    s->graphics.anim = SA2_ANIM_PRESS_START_MSG_JP;
                    break;
#ifdef JAPAN
                case LANG_ENGLISH:
                    s->graphics.anim = SA2_ANIM_PRESS_START_MSG_EN;
                    break;
#endif
                default:
                    s->graphics.anim = SA2_ANIM_PRESS_START_MSG_EN;
                    break;
            }
            s->variant = 0;
            s->animCursor = 0;
            s->qAnimDelay = 0;
            s->prevVariant = -1;
            s->animSpeed = SPRITE_ANIM_SPEED(1.0);
            s->palId = 0;
            s->frameFlags = 0;
            UpdateSpriteAnimation(s);
        }
#endif
        resultsScreen->unk430 = 0;
        gCurTask->main = SA2_LABEL(sub_80823FC);
        SA2_LABEL(sub_80823FC)();
    } else {
#ifndef NON_MATCHING
        // This is wrong, just here to make asm correct here
        resultsScreen++;
        while (0)
            ;
        resultsScreen--;
#endif

        for (i = 0; i < 4; i++) {
            if (!(gMultiSioStatusFlags & MULTI_SIO_RECV_ID(i + 8))) {
                if (gMultiplayerRanks[i] & 1) {
                    sub_80078D4(3, i * 40, (i + 1) * 40, DISPLAY_WIDTH - resultsScreen->unk430, DISPLAY_HEIGHT - i * 40);
                } else {
                    sub_80078D4(3, i * 40, (i + 1) * 40, resultsScreen->unk430 - DISPLAY_WIDTH, DISPLAY_HEIGHT - i * 40);
                }
            } else {
                if (gMultiplayerRanks[i] & 1) {
                    sub_80078D4(3, gMultiplayerRanks[i] * 40, (gMultiplayerRanks[i] + 1) * 40, DISPLAY_WIDTH - resultsScreen->unk430,
                                (i * 5 - gMultiplayerRanks[i] * 5) * 8);
                } else {
                    sub_80078D4(3, gMultiplayerRanks[i] * 40, (gMultiplayerRanks[i] + 1) * 40, resultsScreen->unk430 - DISPLAY_WIDTH,
                                (i * 5 - gMultiplayerRanks[i] * 5) * 8);
                }
            }
        }
    }
}

void SA2_LABEL(sub_80823FC)(void)
{
    Background *background;
    u32 i;
    s32 val2 = 0;
    u8 val = FALSE;
    struct MultiplayerSinglePakResultsScreen *resultsScreen = TASK_DATA(gCurTask);
    union MultiSioData *packet;

    gMultiplayerConnections = EXTRACT_REGISTERED_CONNECTIONS(gMultiSioStatusFlags);
    packet = &gMultiSioRecv[0];
    if (packet->pat0.unk0 == 0x4012) {
#if COLLECT_RINGS_ROM
        gSelectedCharacter = 0;
#else
        gSelectedCharacter = SIO_MULTI_CNT->id;
#endif
        if (!resultsScreen->mode) {
            background = &resultsScreen->unk40;
            gBgScrollRegs[2][0] = 0;
            gBgScrollRegs[2][1] = 0;
            background->graphics.dest = (void *)BG_SCREEN_ADDR(24);
            background->graphics.anim = 0;
            background->layoutVram = (void *)BG_SCREEN_ADDR(28);
            background->unk18 = 0;
            background->unk1A = 0;
            background->tilemapId = TM_LEVEL_METATILES_0(LEVEL_INDEX(ZONE_1, ACT_BOSS));
            switch (gMultiplayerLanguage) {
                case 0:
                    background->unk1E = 0;
#ifdef JAPAN
                    background->unk20 = 0;
#else
                    background->unk20 = 4;
#endif
                    break;
                case 1:
                    background->unk1E = 0;
                    background->unk20 = 0;
                    break;
                case 2:
                default:
                    background->unk1E = 0;
                    background->unk20 = 4;
                    break;
            }
            background->unk22 = 6;
            background->unk24 = 8;
            background->targetTilesX = 0x12;
            background->targetTilesY = 4;
            background->paletteOffset = 0;
            background->flags = BACKGROUND_FLAGS_BG_ID(2);
            DrawBackground(background);
        }

        gDispCnt |= 0x400;
        resultsScreen->unk430 = 0;
        if (resultsScreen->mode) {
            gBldRegs.bldCnt = 0xFF;
            gCurTask->main = SA2_LABEL(Task_8082630);
            SA2_LABEL(Task_8082630)();
        } else {
            gCurTask->main = SA2_LABEL(sub_8082AA8);
            SA2_LABEL(sub_8082AA8)();
        }
    } else {
        SA2_LABEL(sub_8082788)();

#ifndef COLLECT_RINGS_ROM
        if (gMultiSioStatusFlags & MULTI_SIO_PARENT) {
            DisplaySprite(&resultsScreen->unk400);
        }
#endif

        for (i = 0; i < 4 && CONNECTION_REGISTERED(i); i++) {
            if (!(MULTI_SIO_RECV_ID(i) & gMultiSioStatusFlags)) {
                val = TRUE;
            } else {
                packet = &gMultiSioRecv[i];
                if (i == SIO_MULTI_CNT->id || packet->pat0.unk0 == 0x4010) {
                    val2++;
                }
            }
        }
        packet = &gMultiSioSend;
        packet->pat0.unk0 = 0x4010;
        if (gMultiSioStatusFlags & MULTI_SIO_PARENT) {
            packet->pat0.unk3 = gMultiplayerLanguage;
        }

        if ((gMultiSioStatusFlags & MULTI_SIO_PARENT && gPressedKeys & START_BUTTON && !val && val2 > 1) || resultsScreen->actor) {
            resultsScreen->actor = 1;
            packet->pat0.unk0 = 0x4012;
        }
    }
}

void SA2_LABEL(SA2_LABEL(Task_8082630))(void)
{
    struct MultiplayerSinglePakResultsScreen *resultsScreen = TASK_DATA(gCurTask);
    resultsScreen->unk430 += 0x20;
    SA2_LABEL(sub_8082788)();

    if (resultsScreen->unk430 > Q(16)) {
        resultsScreen->unk430 = Q(16);
        gBldRegs.bldY = 0x10;
        gCurTask->main = SA2_LABEL(sub_808267C);
    }

    gBldRegs.bldY = I(resultsScreen->unk430);
}

void SA2_LABEL(sub_808267C)(void)
{
    union MultiSioData *packet;
    struct MultiplayerSinglePakResultsScreen *resultsScreen = TASK_DATA(gCurTask);

    packet = &gMultiSioRecv[0];

    if (packet->pat3.unk0 == COMM_DATA(0x80)) {
        u32 i;
        gMultiplayerPseudoRandom = packet->pat3.unk8;

        for (i = 0; i < 4; i++) {
            gMultiplayerCharacters[i] = 0;
            gMultiplayerRanks[i] = i;
        }

        gFlags &= ~4;
        if (resultsScreen->mode) {
            TaskDestroy(gCurTask);
            CreateMultiplayerSinglePakResultsScreen(0);
        } else {
            TaskDestroy(gCurTask);
            gBldRegs.bldCnt = 0;
            gBldRegs.bldY = 0;
            SA2_LABEL(sub_8081200)();
#if (GAME == GAME_SA2)
            GameStageStart();
#endif
        }
        return;
    }

    SA2_LABEL(sub_8082788)();
    packet = &gMultiSioSend;
    packet->pat0.unk0 = COMM_DATA(0x51);
    packet->pat0.unk2 = 0;

    if ((gMultiSioStatusFlags & MULTI_SIO_TYPE) == MULTI_SIO_PARENT) {
        u8 i;
        for (i = 0; i < 4; i++) {
            if (CONNECTION_REGISTERED(i)) {
                packet = &gMultiSioRecv[i];
                if (packet->pat0.unk0 != COMM_DATA(0x51)) {
                    return;
                }
            }
        }
        packet = &gMultiSioSend;
        packet->pat3.unk0 = COMM_DATA(0x80);
        packet->pat3.unk8 = resultsScreen->unk43C;
    }
}

void SA2_LABEL(sub_8082788)(void)
{
    u32 i;

    Sprite *s;
    struct MultiplayerSinglePakResultsScreen *resultsScreen;

    LINK_HEARTBEAT();
    resultsScreen = TASK_DATA(gCurTask);

    for (i = 0; i < 4; i++) {
        if (!(gMultiSioStatusFlags & MULTI_SIO_RECV_ID(i + 8))) {
            sub_80078D4(3, i * 40, (i + 1) * 40, 0, DISPLAY_HEIGHT - i * 40);
        } else {
            sub_80078D4(3, gMultiplayerRanks[i] * 40, (gMultiplayerRanks[i] + 1) * 40, 0, i * 40 - gMultiplayerRanks[i] * 40);
            if (resultsScreen->mode) {
                u16 temp;

                s = &resultsScreen->unk80[i].unk0;
                s->x = DISPLAY_CENTER_X;
                s->y = gMultiplayerRanks[i] * 40 + 20;
                UpdateSpriteAnimation(s);
                DisplaySprite(s);

                s = &resultsScreen->unk370[gMultiplayerCharacters[i]];
                s->x = 52;
                s->y = gMultiplayerRanks[i] * 40 + 20;
                DisplaySprite(s);

                // TODO: Fix type
                temp = Base10DigitsToHexNibbles(gMultiplayerCharRings[i]);
                s = &resultsScreen->unk160[((temp) >> 8) & 0xF];

                if (s != &resultsScreen->unk160[0]) {
                    s->x = 160;
                    s->y = gMultiplayerRanks[i] * 40 + 20;
                    DisplaySprite(s);
                }

                s = &resultsScreen->unk160[((temp) >> 4) & 0xF];

                if (s != &resultsScreen->unk160[0] || (temp > 0xFF)) {
                    s->x = 171;
                    s->y = gMultiplayerRanks[i] * 40 + 20;
                    DisplaySprite(s);
                }

                s = &resultsScreen->unk160[(temp)&0xF];
                s->x = 182;
                s->y = gMultiplayerRanks[i] * 40 + 20;
                DisplaySprite(s);
            } else {
                u16 temp;
                s = &resultsScreen->unk80[i].unk0;
                s->x = DISPLAY_CENTER_X;
                s->y = i * 40 + 20;
                UpdateSpriteAnimation(s);
                DisplaySprite(s);

                if (gMPRingCollectWins[i] > 99) {
                    temp = 99;
                } else {
                    temp = Base10DigitsToHexNibbles(gMPRingCollectWins[i]);
                }

                s = &resultsScreen->unk160[((temp) >> 4)];

                if (s != &resultsScreen->unk160[0]) {
                    s->x = 160;
                    s->y = i * 40 + 20;
                    DisplaySprite(s);
                }
                s = &resultsScreen->unk160[temp & 0xF];
                s->x = 171;
                s->y = i * 40 + 20;
                DisplaySprite(s);

                s = &resultsScreen->unk340;
                s->x = 197;
                s->y = i * 40 + 20;
                DisplaySprite(s);
            }
        }
    }
}

void SA2_LABEL(sub_8082AA8)(void)
{
    struct MultiplayerSinglePakResultsScreen *resultsScreen = TASK_DATA(gCurTask);

    SA2_LABEL(sub_8082788)();
    if (resultsScreen->unk430++ > 0x1E) {
        resultsScreen->unk430 = 0;
        m4aMPlayFadeOut(&gMPlayInfo_BGM, 8);
        m4aMPlayFadeOut(&gMPlayInfo_SE1, 8);
        m4aMPlayFadeOut(&gMPlayInfo_SE2, 8);
        m4aMPlayFadeOut(&gMPlayInfo_SE3, 8);
        gBldRegs.bldCnt = 0xFF;
        gCurTask->main = SA2_LABEL(Task_8082630);
    }
}

struct MultiplayerSinglePakResultsScreen *InitScreen(s16 mode)
{
    struct MultiplayerSinglePakResultsScreen *resultsScreen;
    INIT_SCREEN(mode);
    resultsScreen->unk43C = gFrameCount;

    return resultsScreen;
}

#ifndef COLLECT_RINGS_ROM
void sub_8082B80(struct MultiplayerSinglePakResultsScreen *resultsScreen)
{
    s16 i;

    for (i = 0; i < (s32)ARRAY_COUNT(resultsScreen->unk80); i++) {
        u16 anim = gPlayerCharacterIdleAnims[i];
        sub_8082CEC(&resultsScreen->unk80[i].unk0, OBJ_VRAM0 + (i * 0x800), anim, 0, 0x78, (i * 40) + 20, SPRITE_OAM_ORDER(16), i, 0x1000);
    }
}
#endif

void sub_8082BF8(struct MultiplayerSinglePakResultsScreen *resultsScreen)
{
    s16 i;

    for (i = 0; i < (s32)ARRAY_COUNT(resultsScreen->unk160); i++) {
        sub_8082CEC(&resultsScreen->unk160[i], OBJ_VRAM0 + (i * 4 + 0x100) * 0x20, SA2_ANIM_DIGITS, i, 0, 0, SPRITE_OAM_ORDER(4), 0,
                    0x1000);
    }
}

void sub_8082C58(struct MultiplayerSinglePakResultsScreen *resultsScreen)
{
    s16 i;

    for (i = 0; i < (s32)ARRAY_COUNT(resultsScreen->unk370); i++) {
        sub_8082CEC(&resultsScreen->unk370[i], OBJ_VRAM0 + 0x2500 + i * 0xC0, SA2_ANIM_MP_SINGLE_PAK_RESULTS_CUMULATIVE, i, 0, 0,
                    SPRITE_OAM_ORDER(4), 0, 0x1000);
    }
}

void sub_8082CB4(struct MultiplayerSinglePakResultsScreen *resultsScreen)
{
    sub_8082CEC(&resultsScreen->unk340,
                OBJ_VRAM0 +
#if COLLECT_RINGS_ROM
                    0x2C80
#else
                    0x2F00
#endif
                ,
                SA2_ANIM_MP_SINGLE_PAK_RESULTS_CUMULATIVE, 3, 0, 0, SPRITE_OAM_ORDER(4), 0, 0x1000);
}

void sub_8082CEC(Sprite *s, void *vramAddr, u16 animId, u8 variant, s16 x, s16 y, u16 oamFlags, u8 unk25, u32 unk10)
{
    RS_SPRITE_INIT(s, vramAddr, animId, variant, x, y, oamFlags, unk25, unk10);
}
