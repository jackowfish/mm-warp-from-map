/*
 * Warp Select
 *
 * Adds a "warp map" mode to the pause menu's world-map page so you can pick a
 * Song of Soaring destination directly from the map instead of playing the song.
 *
 * The game already contains a complete owl-warp selector (the PAUSE_STATE_OWL_WARP_*
 * states that the Song of Soaring drops you into). It draws the map, a cursor over
 * only the *unlocked* owl statues, the location-name panel, a "Warp to ___? Yes/No"
 * confirmation, and on close it spawns the actor that performs the warp. This mod
 * simply switches the open map page into that native selector on a button press, and
 * lets you toggle back out to the normal map. We don't reimplement any of it.
 */
#include "modding.h"
#include "global.h"
#include "recomputils.h"
#include "recompconfig.h"

// Pulled in only for the compile-time PAUSE_*/OWL_WARP_* enum constants. We never
// reference any symbol that lives in the overlay at runtime (see note below).
#include "overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h"

#include "warp_icon_tex.h"

// The pause menu's update/draw entry points are *resident* wrappers in main `code`
// (KaleidoScopeCall_Update / KaleidoScopeCall_Draw). The functions they dispatch to
// live in the relocatable ovl_kaleido_scope overlay, which is only mapped while the
// menu is open, so hooking the overlay functions directly (or reading overlay data
// like sInDungeonScene) resolves against an unloaded overlay and crashes on load.
// We hook only the resident wrappers and touch only resident state (PlayState fields,
// gSaveContext, and resident helpers), so nothing here depends on the overlay layout.

// Captured each frame from the resident update wrapper so the draw wrapper (whose
// return-hook argument registers aren't guaranteed) has a valid PlayState to draw with.
static PlayState* sPlay = NULL;

// True while a selection started from the pause map is in progress. Gates trigger_warp so
// we never interfere with an actual Song of Soaring (where the player consumes ocarinaMode).
static s32 sFromMapWarp = false;

// The game's message font, DMA'd from the player's own ROM on first use so the mod
// ships no game asset. nes_font_static: 16x16 i4 glyphs, 0x80 bytes each.
#define NES_FONT_VROM 0xACC000
#define NES_FONT_SIZE 0x4E00
static u8 sNesFont[NES_FONT_SIZE] __attribute__((aligned(16)));
static s32 sNesFontLoaded = false;

// Which C button toggles warp mode. Index matches the mod.toml "toggle_button" enum.
static u16 get_toggle_btn(void) {
    switch (recomp_get_config_u32("toggle_button")) {
        case 1:  return BTN_CUP;
        case 2:  return BTN_CLEFT;
        case 3:  return BTN_CRIGHT;
        default: return BTN_CDOWN;
    }
}

// True when the player is looking at the overworld map page, idle, with at least one
// owl statue unlocked, and soaring isn't restricted in this area.
static s32 can_enter_warp(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    return (pauseCtx->state == PAUSE_STATE_MAIN) && (pauseCtx->mainState == PAUSE_MAIN_STATE_IDLE) &&
           (pauseCtx->pageIndex == PAUSE_MAP) &&
           (gSaveContext.save.saveInfo.playerData.owlActivationFlags != 0) &&
           (play->interfaceCtx.restrictions.songOfSoaring == 0);
}

// Drop the open map page straight into the native owl-warp selector. We only have to
// reproduce the small amount of state that func_800F4A10's owl-warp branch sets up;
// the loaded map segments and the world-map view registers are already correct because
// the pause menu is open on the map page (both paths share that setup).
static void enter_warp_select(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    u16 owlFlags = gSaveContext.save.saveInfo.playerData.owlActivationFlags;
    s32 i;

    // Where to return to if the player backs out (see KaleidoScope owl-warp cleanup).
    pauseCtx->unk_2C8 = PAUSE_MAP;
    pauseCtx->unk_2CA = pauseCtx->cursorPoint[PAUSE_WORLD_MAP];

    // Show only the unlocked owl statues, and park the cursor on one of them.
    for (i = 0; i < (s32)ARRAY_COUNT(pauseCtx->worldMapPoints); i++) {
        pauseCtx->worldMapPoints[i] = false;
    }
    for (i = OWL_WARP_STONE_TOWER; i >= OWL_WARP_GREAT_BAY_COAST; i--) {
        if ((owlFlags >> i) & 1) {
            pauseCtx->worldMapPoints[i] = true;
            pauseCtx->cursorPoint[PAUSE_WORLD_MAP] = i;
        }
    }
    if ((owlFlags >> OWL_WARP_CLOCK_TOWN) & 1) {
        pauseCtx->cursorPoint[PAUSE_WORLD_MAP] = OWL_WARP_CLOCK_TOWN;
    }

    func_8011552C(play, DO_ACTION_WARP); // change the A-button prompt to "Warp"

    R_PAUSE_OWL_WARP_ALPHA = 120;        // dim overlay used behind the warp map
    pauseCtx->infoPanelOffsetY = 0;
    pauseCtx->namedItem = PAUSE_ITEM_NONE; // force the location-name panel to reload
    pauseCtx->cursorSpecialPos = 0;
    pauseCtx->cursorColorSet = PAUSE_CURSOR_COLOR_SET_BLUE;
    pauseCtx->mainState = PAUSE_MAIN_STATE_IDLE;
    pauseCtx->promptChoice = PAUSE_PROMPT_YES;
    pauseCtx->state = PAUSE_STATE_OWL_WARP_SELECT;
    sFromMapWarp = true;

    Audio_PlaySfx(NA_SE_SY_DECIDE);
}

// Back out of warp mode to the normal world map, keeping the pause menu open.
static void exit_warp_select(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    s32 i;

    // Restore the normal map: region dots for every visited region.
    for (i = 0; i < (s32)ARRAY_COUNT(pauseCtx->worldMapPoints); i++) {
        pauseCtx->worldMapPoints[i] = false;
    }
    for (i = 0; i < REGION_MAX; i++) {
        if ((gSaveContext.save.saveInfo.regionsVisited >> i) & 1) {
            pauseCtx->worldMapPoints[i] = true;
        }
    }

    func_8011552C(play, DO_ACTION_NONE);
    pauseCtx->cursorPoint[PAUSE_WORLD_MAP] = pauseCtx->unk_2CA;
    pauseCtx->namedItem = PAUSE_ITEM_NONE;
    pauseCtx->cursorColorSet = PAUSE_CURSOR_COLOR_SET_WHITE;
    pauseCtx->mainState = PAUSE_MAIN_STATE_IDLE;
    pauseCtx->state = PAUSE_STATE_MAIN;
    sFromMapWarp = false;

    Audio_PlaySfx(NA_SE_SY_CANCEL);
}

// Owl-warp destinations, indexed by OwlWarpId. Mirrors sOwlWarpEntrances in ovl_En_Test7,
// the actor the Song of Soaring normally spawns to perform the warp.
static const u16 sOwlWarpEntrances[] = {
    ENTRANCE(GREAT_BAY_COAST, 11),         // OWL_WARP_GREAT_BAY_COAST
    ENTRANCE(ZORA_CAPE, 6),                // OWL_WARP_ZORA_CAPE
    ENTRANCE(SNOWHEAD, 3),                 // OWL_WARP_SNOWHEAD
    ENTRANCE(MOUNTAIN_VILLAGE_WINTER, 8),  // OWL_WARP_MOUNTAIN_VILLAGE
    ENTRANCE(SOUTH_CLOCK_TOWN, 9),         // OWL_WARP_CLOCK_TOWN
    ENTRANCE(MILK_ROAD, 4),                // OWL_WARP_MILK_ROAD
    ENTRANCE(WOODFALL, 4),                 // OWL_WARP_WOODFALL
    ENTRANCE(SOUTHERN_SWAMP_POISONED, 10), // OWL_WARP_SOUTHERN_SWAMP
    ENTRANCE(IKANA_CANYON, 4),             // OWL_WARP_IKANA_CANYON
    ENTRANCE(STONE_TOWER, 3),              // OWL_WARP_STONE_TOWER
};

// Actually perform the warp. The native owl-warp confirm only sets msgCtx.ocarinaMode,
// which is normally consumed by the player's ocarina action (it spawns En_Test7 to run the
// feather cutscene and trigger the transition). Because we open the selector from the pause
// map instead of by playing the song, the player is never in that action and nothing reads
// ocarinaMode, so we replicate En_Test7's transition setup here.
static void trigger_warp(PlayState* play, s32 owlWarpId) {
    u16 entrance = sOwlWarpEntrances[owlWarpId];

    if ((entrance == ENTRANCE(SOUTHERN_SWAMP_POISONED, 10)) &&
        CHECK_WEEKEVENTREG(WEEKEVENTREG_CLEARED_WOODFALL_TEMPLE)) {
        entrance = ENTRANCE(SOUTHERN_SWAMP_CLEARED, 10);
    } else if ((entrance == ENTRANCE(MOUNTAIN_VILLAGE_WINTER, 8)) &&
               CHECK_WEEKEVENTREG(WEEKEVENTREG_CLEARED_SNOWHEAD_TEMPLE)) {
        entrance = ENTRANCE(MOUNTAIN_VILLAGE_SPRING, 8);
    }

    play->nextEntrance = entrance;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.ambienceId = AMBIENCE_ID_DISABLED;
}

// Runs before the pause menu's per-frame update. KaleidoScopeCall_Update is the
// resident wrapper (always mapped), unlike the overlay's KaleidoScope_Update.
RECOMP_HOOK("KaleidoScopeCall_Update") void warp_select_update(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = CONTROLLER1(&play->state);
    u16 toggle = get_toggle_btn();

    sPlay = play;

    // Fetch the game font from the player's ROM the first time the menu updates.
    if (!sNesFontLoaded) {
        DmaMgr_SendRequest0(sNesFont, NES_FONT_VROM, NES_FONT_SIZE);
        sNesFontLoaded = true;
    }

    // If we opened the selector from the map and the native confirm has set an owl-warp
    // ocarina mode, perform the warp ourselves (the player can't, see trigger_warp).
    if (sFromMapWarp) {
        u16 ocarinaMode = play->msgCtx.ocarinaMode;

        if ((ocarinaMode >= OCARINA_MODE_WARP_TO_GREAT_BAY_COAST) &&
            (ocarinaMode <= OCARINA_MODE_WARP_TO_STONE_TOWER)) {
            trigger_warp(play, ocarinaMode - OCARINA_MODE_WARP_TO_GREAT_BAY_COAST);
            play->msgCtx.ocarinaMode = OCARINA_MODE_END;
            sFromMapWarp = false;
            return;
        }

        // Left the selector without warping (backed out or closed the menu): stop watching
        // so a later real Song of Soaring is never affected.
        if ((pauseCtx->state == PAUSE_STATE_MAIN) || (pauseCtx->state == PAUSE_STATE_OFF)) {
            sFromMapWarp = false;
        }
    }

    if (can_enter_warp(play) && CHECK_BTN_ALL(input->press.button, toggle)) {
        enter_warp_select(play);
        return;
    }

    // While selecting, the toggle button or B backs out to the normal map. Start still
    // closes the whole menu and A still confirms a destination (both handled natively),
    // so we consume only the buttons we act on here.
    if (pauseCtx->state == PAUSE_STATE_OWL_WARP_SELECT &&
        CHECK_BTN_ANY(input->press.button, toggle | BTN_B)) {
        exit_warp_select(play);
        input->press.button &= ~(toggle | BTN_B);
    }
}

// On-screen position: top-left of the map page, just left of the centered "MAP" header.
#define WARP_PROMPT_SCR_X 51
#define WARP_PROMPT_SCR_Y 43
// Icon footprint (the source texture is authored at 2x this in each axis).
#define WARP_ICON_SCR_W 32
#define WARP_ICON_SCR_H 16
// The label starts here, in the game font at its native 16px cell size.
#define WARP_LABEL_SCR_X (WARP_PROMPT_SCR_X + 28)
// Outline offset in quarter-pixel rect units (1 screen pixel).
#define WARP_OUTLINE_OFS 4

// sNESFontWidths from z_message_nes.c, indexed by (char - 0x20). Advance per glyph.
static const u8 sFontWidths[96] = {
    8, 8, 6, 9, 9, 14, 12, 3, 7, 7, 7, 9, 4, 6, 4, 9,
    10, 5, 9, 9, 10, 9, 9, 9, 9, 9, 6, 6, 9, 11, 9, 11,
    13, 12, 9, 11, 11, 8, 8, 12, 10, 4, 8, 10, 8, 13, 11, 13,
    9, 13, 10, 10, 9, 10, 11, 15, 11, 10, 10, 7, 10, 7, 10, 9,
    5, 8, 9, 8, 9, 9, 6, 9, 8, 4, 6, 8, 4, 12, 9, 9,
    9, 9, 7, 8, 7, 8, 9, 12, 8, 9, 8, 7, 5, 7, 10, 6,
};

// Rect coordinates below are in quarter pixels (the gSPTextureRectangle unit).

// Draw one glyph from the loaded font as a sizePx-tall rect at (x4, y4).
static void draw_glyph(GraphicsContext* gfxCtx, char c, s32 x4, s32 y4, s32 sizePx) {
    s32 dxdy = (16 << 10) / sizePx;

    OPEN_DISPS(gfxCtx);

    gDPLoadTextureBlock_4b(POLY_OPA_DISP++, sNesFont + ((c - 0x20) * 0x80), G_IM_FMT_I, 16, 16, 0,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                           G_TX_NOLOD);
    gSPTextureRectangle(POLY_OPA_DISP++, x4, y4, x4 + (sizePx << 2), y4 + (sizePx << 2), G_TX_RENDERTILE, 0, 0, dxdy,
                        dxdy);

    CLOSE_DISPS(gfxCtx);
}

// Draw one pass of the label and the C-in-circle glyph. For the outline pass the
// glyphs are drawn at the eight 1px offsets; the fill pass draws them once, centered.
static void draw_text_pass(GraphicsContext* gfxCtx, s32 outline) {
    static const char label[] = "Warp Map";
    s32 dx, dy, i;

    for (dy = -WARP_OUTLINE_OFS; dy <= WARP_OUTLINE_OFS; dy += WARP_OUTLINE_OFS) {
        for (dx = -WARP_OUTLINE_OFS; dx <= WARP_OUTLINE_OFS; dx += WARP_OUTLINE_OFS) {
            s32 x4 = (WARP_LABEL_SCR_X << 2) + dx;

            if (outline == ((dx == 0) && (dy == 0))) {
                continue;
            }
            for (i = 0; label[i] != '\0'; i++) {
                if (label[i] == ' ') {
                    x4 += 6 << 2; // the table's 8px space reads too wide; tighten it
                    continue;
                }
                draw_glyph(gfxCtx, label[i], x4, (WARP_PROMPT_SCR_Y << 2) + dy, 16);
                x4 += sFontWidths[label[i] - 0x20] << 2;
            }
            // "C" inside the button circle, at half size.
            draw_glyph(gfxCtx, 'C', ((WARP_PROMPT_SCR_X << 2) + (15 << 1)) - (4 << 2) + dx,
                       ((WARP_PROMPT_SCR_Y + 4) << 2) + dy, 8);
        }
    }
}

// Draw the icon's coverage layer (circle + arrow) for the current pass.
static void draw_icon_layer(GraphicsContext* gfxCtx, const unsigned char* tex) {
    s32 dxdy = (WARP_ICON_TEX_WIDTH << 10) / WARP_ICON_SCR_W;

    OPEN_DISPS(gfxCtx);

    gDPLoadTextureBlock(POLY_OPA_DISP++, tex, G_IM_FMT_IA, G_IM_SIZ_8b, WARP_ICON_TEX_WIDTH, WARP_ICON_TEX_HEIGHT, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    gSPTextureRectangle(POLY_OPA_DISP++, WARP_PROMPT_SCR_X << 2, WARP_PROMPT_SCR_Y << 2,
                        (WARP_PROMPT_SCR_X + WARP_ICON_SCR_W) << 2, (WARP_PROMPT_SCR_Y + WARP_ICON_SCR_H) << 2,
                        G_TX_RENDERTILE, 0, 0, dxdy, dxdy);

    CLOSE_DISPS(gfxCtx);
}

// Draw the "C-<dir>: Warp Map" prompt over the world map page, in the game's own
// item-name style: everything navy at 1px offsets first, then the white fill on top.
// Color comes from the primitive color only; the textures supply alpha coverage.
static void draw_prompt(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    u32 dir = recomp_get_config_u32("toggle_button");

    if (dir > 3) {
        dir = 0;
    }
    if (!sNesFontLoaded) {
        return;
    }

    OPEN_DISPS(gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCycleType(POLY_OPA_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineLERP(POLY_OPA_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0,
                      PRIMITIVE, 0);
    gDPSetTextureFilter(POLY_OPA_DISP++, G_TF_BILERP);

    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 8, 24, 72, play->pauseCtx.alpha);
    draw_icon_layer(gfxCtx, gWarpIconOutlineTex[dir]);
    draw_text_pass(gfxCtx, true);

    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 225, 240, 255, play->pauseCtx.alpha);
    draw_icon_layer(gfxCtx, gWarpIconFillTex[dir]);
    draw_text_pass(gfxCtx, false);

    gDPPipeSync(POLY_OPA_DISP++);

    CLOSE_DISPS(gfxCtx);
}

// Runs after the resident draw wrapper finishes, so our prompt sits on top of the
// fully-drawn pause menu. KaleidoScopeCall_Draw is resident; we use the PlayState
// captured in the update hook rather than this return hook's arguments.
RECOMP_HOOK_RETURN("KaleidoScopeCall_Draw") void warp_select_draw(void) {
    if (sPlay == NULL) {
        return;
    }
    if (recomp_get_config_u32("show_prompt") != 0) { // 0 = Shown
        return;
    }
    if (can_enter_warp(sPlay)) {
        draw_prompt(sPlay);
    }
}
