#include "global.h"
#include "gflib.h"
#include "data.h"
#include "event_data.h"
#include "field_player_avatar.h"
#include "fldeff.h"
#include "hm_menu.h"
#include "menu.h"
#include "new_menu_helpers.h"
#include "overworld.h"
#include "party_menu.h"
#include "region_map.h"
#include "strings.h"
#include "task.h"
#include "text_window.h"
#include "constants/moves.h"
#include "constants/party_menu.h"
#include "constants/songs.h"

#define HM_MENU_COUNT (FIELD_MOVE_WATERFALL + 1) // Flash, Cut, Fly, Strength, Surf, Rock Smash, Waterfall

// Matches the fixed constants `WindowFunc_DrawStandardFrame` hardcodes
// internally (src/new_menu_helpers.c:14-15) - any window that wants the
// game's standard bordered-box look must load its frame graphics at this
// exact tile offset/palette bank, since DrawStdWindowFrame ignores whatever
// paletteNum its caller's window template actually has.
#define HM_STD_FRAME_BASE_TILE 0x214
#define HM_STD_FRAME_PALETTE_NUM 14


enum
{
    WIN_HM_HEADER,
    WIN_HM_LIST
};

struct HMMenuState
{
    u8 cursorPos;
    u8 state;
};

static EWRAM_DATA struct HMMenuState *sHMMenu = NULL;
static EWRAM_DATA MainCallback sHMMenuExitCallback = NULL;

static void CB2_InitHMMenu(void);
static void VBlankCB_HMMenu(void);
static void CB2_HMMenu(void);
static void Task_HMMenu(u8 taskId);
static u8 HMMenu_ProcessInput(void);
static void PrintHMMenuHeaderText(const u8 *str);
static void PrintHMMenuList(void);
static void HMMenu_TryUseSelection(u8 taskId, u8 fieldMove);
static void CloseHMMenu(u8 taskId);

static const struct BgTemplate sHMMenuBgTemplates[] = {
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0
    },
};

static const struct WindowTemplate sHMMenuWinTemplates[] = {
    [WIN_HM_HEADER] = {
        .bg = 0,
        .tilemapLeft = 2,
        .tilemapTop = 1,
        .width = 26,
        .height = 2,
        .paletteNum = HM_STD_FRAME_PALETTE_NUM,
        .baseBlock = 1
    },
    [WIN_HM_LIST] = {
        .bg = 0,
        .tilemapLeft = 2,
        .tilemapTop = 5,
        .width = 26,
        .height = 12,
        .paletteNum = HM_STD_FRAME_PALETTE_NUM,
        .baseBlock = 0x080
    },
    DUMMY_WIN_TEMPLATE
};

static const u8 sHMMenuHeaderColor[3] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};
static const u8 sHMMenuCursorColor[3] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_RED, TEXT_COLOR_LIGHT_RED};
static const u8 sText_HMMenuCursor[] = _(">");

// FIELD_MOVE_FLASH..FIELD_MOVE_WATERFALL order
static const u16 sHMMenuMoveIds[HM_MENU_COUNT] = {
    [FIELD_MOVE_FLASH]      = MOVE_FLASH,
    [FIELD_MOVE_CUT]        = MOVE_CUT,
    [FIELD_MOVE_FLY]        = MOVE_FLY,
    [FIELD_MOVE_STRENGTH]   = MOVE_STRENGTH,
    [FIELD_MOVE_SURF]       = MOVE_SURF,
    [FIELD_MOVE_ROCK_SMASH] = MOVE_ROCK_SMASH,
    [FIELD_MOVE_WATERFALL]  = MOVE_WATERFALL,
};

// Re-runs the same position/context check the party menu field-move list used
// to use (facing a cuttable tree, facing surfable water, in a dark cave, etc).
// No party Pokemon needs to know the move for this check to succeed.
static bool8 (*const sHMMenuSetUpFuncs[HM_MENU_COUNT])(void) = {
    [FIELD_MOVE_FLASH]      = SetUpFieldMove_Flash,
    [FIELD_MOVE_CUT]        = SetUpFieldMove_Cut,
    [FIELD_MOVE_FLY]        = SetUpFieldMove_Fly,
    [FIELD_MOVE_STRENGTH]   = SetUpFieldMove_Strength,
    [FIELD_MOVE_SURF]       = SetUpFieldMove_Surf,
    [FIELD_MOVE_ROCK_SMASH] = SetUpFieldMove_RockSmash,
    [FIELD_MOVE_WATERFALL]  = SetUpFieldMove_Waterfall,
};

static const u8 *const sHMMenuContextFailMessages[HM_MENU_COUNT] = {
    [FIELD_MOVE_FLASH]      = gText_CantUseHere,
    [FIELD_MOVE_CUT]        = gText_NothingToCut,
    [FIELD_MOVE_FLY]        = gText_CantUseHere,
    [FIELD_MOVE_STRENGTH]   = gText_CantUseHere,
    [FIELD_MOVE_SURF]       = gText_CantUseHere,
    [FIELD_MOVE_ROCK_SMASH] = gText_CantUseHere,
    [FIELD_MOVE_WATERFALL]  = gText_CantUseHere,
};

void CB2_HMMenuFromStartMenu(void)
{
    sHMMenu = AllocZeroed(sizeof(struct HMMenuState));
    SetMainCallback2(CB2_HMMenu);
}

static void VBlankCB_HMMenu(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_InitHMMenu(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void CB2_HMMenu(void)
{
    switch (sHMMenu->state)
    {
    case 0:
        SetVBlankCallback(NULL);
        SetHBlankCallback(NULL);
        break;
    case 1:
        DmaClearLarge16(3, (void *)VRAM, VRAM_SIZE, 0x1000);
        DmaClear32(3, (void *)OAM, OAM_SIZE);
        DmaClear16(3, (void *)PLTT, PLTT_SIZE);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0);
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sHMMenuBgTemplates, NELEMS(sHMMenuBgTemplates));
        ChangeBgX(0, 0, 0);
        ChangeBgY(0, 0, 0);
        InitWindows(sHMMenuWinTemplates);
        DeactivateAllTextPrinters();
        ShowBg(0);
        break;
    case 2:
        ResetSpriteData();
        ResetPaletteFade();
        FreeAllSpritePalettes();
        ResetTasks();
        break;
    case 3:
        Menu_LoadStdPal();
        LoadUserWindowGfx(WIN_HM_HEADER, HM_STD_FRAME_BASE_TILE, BG_PLTT_ID(HM_STD_FRAME_PALETTE_NUM));
        LoadUserWindowGfx(WIN_HM_LIST, HM_STD_FRAME_BASE_TILE, BG_PLTT_ID(HM_STD_FRAME_PALETTE_NUM));
        break;
    case 4:
        DrawStdWindowFrame(WIN_HM_HEADER, TRUE);
        DrawStdWindowFrame(WIN_HM_LIST, TRUE);
        break;
    case 5:
        PrintHMMenuHeaderText(gText_MenuUseHM);
        PrintHMMenuList();
        break;
    default:
        SetVBlankCallback(VBlankCB_HMMenu);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0x10, 0, RGB_BLACK);
        CreateTask(Task_HMMenu, 0);
        SetMainCallback2(CB2_InitHMMenu);
        return;
    }
    sHMMenu->state++;
}

static void PrintHMMenuHeaderText(const u8 *str)
{
    FillWindowPixelBuffer(WIN_HM_HEADER, PIXEL_FILL(1));
    AddTextPrinterParameterized3(WIN_HM_HEADER, FONT_NORMAL, 8, 1, sHMMenuHeaderColor, TEXT_SKIP_DRAW, str);
    PutWindowTilemap(WIN_HM_HEADER);
    CopyWindowToVram(WIN_HM_HEADER, COPYWIN_FULL);
}

static void PrintHMMenuList(void)
{
    u8 i;
    u8 letterHeight = GetFontAttribute(FONT_NORMAL, FONTATTR_MAX_LETTER_HEIGHT);

    FillWindowPixelBuffer(WIN_HM_LIST, PIXEL_FILL(1));
    for (i = 0; i < HM_MENU_COUNT; i++)
    {
        u16 y = (i * (letterHeight - 1)) + 2;

        if (sHMMenu->cursorPos == i)
            AddTextPrinterParameterized3(WIN_HM_LIST, FONT_NORMAL, 4, y, sHMMenuCursorColor, TEXT_SKIP_DRAW, sText_HMMenuCursor);
        AddTextPrinterParameterized3(WIN_HM_LIST, FONT_NORMAL, 16, y, sHMMenuHeaderColor, TEXT_SKIP_DRAW, gMoveNames[sHMMenuMoveIds[i]]);
    }
    PutWindowTilemap(WIN_HM_LIST);
    CopyWindowToVram(WIN_HM_LIST, COPYWIN_FULL);
}

static void Task_HMMenu(u8 taskId)
{
    switch (gTasks[taskId].data[0])
    {
    case 0:
        if (gPaletteFade.active)
            return;
        gTasks[taskId].data[0]++;
        break;
    case 1:
        switch (HMMenu_ProcessInput())
        {
        case 1: // moved cursor
            PrintHMMenuHeaderText(gText_MenuUseHM);
            PrintHMMenuList();
            break;
        case 2: // confirm
            HMMenu_TryUseSelection(taskId, sHMMenu->cursorPos);
            break;
        case 3: // cancel
            sHMMenuExitCallback = CB2_ReturnToFieldWithOpenMenu;
            BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 0x10, RGB_BLACK);
            gTasks[taskId].data[0]++;
            break;
        }
        break;
    case 2:
        if (gPaletteFade.active)
            return;
        CloseHMMenu(taskId);
        break;
    }
}

static u8 HMMenu_ProcessInput(void)
{
    if (JOY_NEW(DPAD_UP))
    {
        sHMMenu->cursorPos = (sHMMenu->cursorPos == 0) ? HM_MENU_COUNT - 1 : sHMMenu->cursorPos - 1;
        PlaySE(SE_SELECT);
        return 1;
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        sHMMenu->cursorPos = (sHMMenu->cursorPos == HM_MENU_COUNT - 1) ? 0 : sHMMenu->cursorPos + 1;
        PlaySE(SE_SELECT);
        return 1;
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        return 2;
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        return 3;
    }
    return 0;
}

static void HMMenu_TryUseSelection(u8 taskId, u8 fieldMove)
{
    if (!FlagGet(FLAG_BADGE01_GET + fieldMove))
    {
        PrintHMMenuHeaderText(gText_HMMenuNeedBadge);
        return;
    }
    if (!CanUseHMFieldMove(fieldMove))
    {
        PrintHMMenuHeaderText(gText_HMItemNotInBag);
        return;
    }
    if (sHMMenuSetUpFuncs[fieldMove]() != TRUE)
    {
        PrintHMMenuHeaderText(sHMMenuContextFailMessages[fieldMove]);
        return;
    }

    // SetUpFieldMove_X already queued gFieldCallback2/gPostMenuFieldCallback
    // (or, for Fly, nothing - it just opens the destination map screen).
    sHMMenuExitCallback = (fieldMove == FIELD_MOVE_FLY) ? CB2_OpenFlyMap : CB2_ReturnToField;
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 0x10, RGB_BLACK);
    gTasks[taskId].data[0]++;
}

static void CloseHMMenu(u8 taskId)
{
    FreeAllWindowBuffers();
    FREE_AND_SET_NULL(sHMMenu);
    DestroyTask(taskId);
    SetMainCallback2(sHMMenuExitCallback);
}
