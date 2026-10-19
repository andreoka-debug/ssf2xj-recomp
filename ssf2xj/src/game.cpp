// SPDX-License-Identifier: GPL-3.0-only
// Local SSF2X display experiment. No ROM data or translated program is included.
#include "game.h"
#include "machine.h"

namespace cps2recomp {

void RegisterTraining();
void TrainingBeforeFrame(Machine &machine);
void TrainingFrame(Machine &machine);
void TrainingState(Archive &ar);
bool TrainingActive();

void EasySelectGouki(Machine &machine) {
    if (machine.ram[0x8009] != 0) return;
    constexpr uint16_t shortcut = kUp | kPunch3 | kKick3;
    for (int player = 0; player < 2; ++player) {
        const uint32_t base = player ? 0xFF8BDF : 0xFF87DF;
        if (Ram8(base) == 0 && (machine.players[player] & shortcut) == shortcut) {
            SetRam8(player ? 0xFF8BFF : 0xFF87FF, 0x0A);
            SetRam8(player ? 0xFF8C0B : 0xFF880B, 0x01);
        }
    }
}

void RegisterGame() {
    // The SSF2X QSound program's four-byte song entries start here. Entries
    // 0x01-0x3F contain music (0x38 is not a song); later entries are effects.
    // This enables the toolkit's custom-soundtrack feature and playlist.
    TheGame().songTable = 0x9006;
    TheGame().songCount = 0x40;
    RegisterTraining();
    TheGame().features.push_back({"wall_helper", "Display", "Wall helper",
        "Show the original 4:3 playfield limits as two faint dashed lines in wide fights. Useful for wall moves.", 0});
    TheGame().inTraining = TrainingActive;
    TheGame().noRunAhead = TrainingActive;
    // FF8009 was traced through character select, VS, rounds and match transitions in ssf2xj.
    // The opening stays 4:3. Later screens render the CPS-2's full 512-column raster;
    // this does not change the game's camera or collision limits.
    // At the true camera edges, safe wide covers only the columns beyond the stage.
    TheGame().beforeFrame = [](Machine &machine) {
        TrainingBeforeFrame(machine);
        EasySelectGouki(machine);
        const uint8_t state = machine.ram[0x8009];
        const bool fightState = state == 8 || state == 10 || state == 12;
        // Keep the same picture width through the score count and short pre-round state.
        const bool betweenRounds = state == 6 || state == 14 || state == 16;
        const bool fight = fightState || betweenRounds;
        const bool otherScreen = state != 0 && !fight;
        const int shape = FeatureValue("display_shape");
        machine.video.wide.side = shape >= 3 && (fight || otherScreen) ? Video::kMaxSide : 0;
        machine.video.wide.guardStageEdges = shape == 3 && fight && machine.ram[0x8c4f] != 0x0f;
        machine.video.wide.fadeMenuSides = shape >= 3 && otherScreen;
        // Traced in multiple stages: scroll 2 moves from 0 at the left limit to 384 at the right.
        machine.video.wide.maxStageScrollX = 384;
        const bool cammyStage = machine.ram[0x8c4f] == 0x0c;
        const bool deeJayStage = machine.ram[0x8c4f] == 0x0f;
        machine.video.wide.squareBlackEdges = shape == 3 && fight && cammyStage;
        machine.video.wide.fillHoles = shape == 4 && fight;
        machine.video.wide.fillSideHoles = shape == 3 && fight && !cammyStage && !deeJayStage;
        // Dee Jay's row-scrolled sky wraps into stale tiles outside the arcade
        // picture. Extend the painted background while keeping wide objects.
        machine.video.wide.blankSideBackground = false;
        // A Super Combo knockout paints its burst across the full raster in scroll 3, but
        // stage scrolls obscure the extra columns. FF8462/FF8862 are remaining life;
        // FinishPicture also checks for the burst so ordinary KO frames keep their stage.
        machine.video.wide.extendSuperFinish = shape == 3 && state == 10 &&
            (machine.ram[0x8462] == 0 || machine.ram[0x8862] == 0);
        // Cammy's castle leaves stale scroll tiles beyond the original 384 columns.
        // FF8C4F holds the selected stage (0x0C = Cammy); preserve wide objects
        // while mirroring only the castle background in the extra side columns.
        machine.video.wide.mirrorSideBackground = (shape == 4 && fight && cammyStage) ||
            (shape == 3 && fight && deeJayStage);
    };
    TheGame().frame = [](Machine &machine) {
        TrainingFrame(machine);
        const uint8_t state = machine.ram[0x8009];
        if (!FeatureOn("wall_helper") || !machine.video.wide.side ||
            (state != 8 && state != 10 && state != 12)) return;
        auto boxes = Boxes();
        for (int y = 8; y < Video::kHeight - 8; y += 12) {
            boxes.push_back({Video::kMaxSide, y, 1, 6, 80, 220, 255, 95});
            boxes.push_back({Video::kMaxSide + Video::kWidth - 1, y, 1, 6, 80, 220, 255, 95});
        }
        SetBoxes(std::move(boxes));
    };
    TheGame().state = TrainingState;
    TheGame().widestPicture = [] { return FeatureValue("display_shape") >= 3 ? Video::kMaxWidth : Video::kWidth; };
}

} // namespace cps2recomp
