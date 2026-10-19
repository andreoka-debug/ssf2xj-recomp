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
    // Only the fight stage uses the CPS-2's full 512-column raster. Menus,
    // score screens and the next-opponent screen keep their original 4:3 view.
    // At the true camera edges, safe wide covers only the columns beyond the stage.
    TheGame().beforeFrame = [](Machine &machine) {
        TrainingBeforeFrame(machine);
        EasySelectGouki(machine);
        const uint8_t state = machine.ram[0x8009];
        const bool fightState = state == 8 || state == 10 || state == 12;
        const int shape = FeatureValue("display_shape");
        machine.video.wide.side = shape >= 3 && fightState ? Video::kMaxSide : 0;
        machine.video.wide.guardStageEdges = shape == 3 && fightState && machine.ram[0x8c4f] != 0x0f;
        machine.video.wide.fadeMenuSides = false;
        const bool cammyStage = machine.ram[0x8c4f] == 0x0c;
        const bool deeJayStage = machine.ram[0x8c4f] == 0x0f;
        const bool hondaStage = machine.ram[0x8c4f] == 0x01;
        // The bathhouse's scroll-3 art moves more slowly than its scroll-2
        // floor (traced bounds: 265..502 versus 0..384). Guarding it with the
        // floor exposes unfinished side strips before the wall art reaches them.
        machine.video.wide.stageEdgeScroll = hondaStage ? 2 : 1;
        machine.video.wide.stageEdgeOriginX = hondaStage ? 265 : 0;
        machine.video.wide.maxStageScrollX = hondaStage ? 237 : 384;
        machine.video.wide.stageEdgeStep = hondaStage ? 8 : 1;
        machine.video.wide.squareBlackEdges = shape == 3 && fightState && cammyStage;
        // Honda's animated bathhouse leaves changing transparent runs in the
        // extra columns. Per-line hole mirroring switches on/off as those runs
        // cross its width/side thresholds, making patches of the sides flash.
        machine.video.wide.fillHoles = shape == 4 && fightState && !hondaStage;
        machine.video.wide.fillSideHoles = shape == 3 && fightState && !cammyStage && !deeJayStage && !hondaStage;
        // Keep the original stage pixels in the extra columns; edge effects
        // such as mirroring or blurring look worse on Dee Jay's moving backdrop.
        machine.video.wide.blankSideBackground = false;
        // A Super Combo knockout paints its burst across the full raster in scroll 3, but
        // stage scrolls obscure the extra columns. Current life is the signed
        // word at player + 0x78 (0xFFFF on KO); +0x7A is the delayed damage
        // readout and can still hold the pre-hit value during the burst.
        // The old byte at +0x62 is normally zero even while a fighter is healthy.
        // FinishPicture also checks for the burst so ordinary KO frames keep their stage.
        machine.video.wide.extendSuperFinish = shape == 3 && state == 10 &&
            (static_cast<int16_t>(Ram16(0xFF8478)) <= 0 || static_cast<int16_t>(Ram16(0xFF8878)) <= 0);
        // Cammy's castle leaves stale scroll tiles beyond the original 384 columns.
        // FF8C4F holds the selected stage (0x0C = Cammy); preserve wide objects
        // while mirroring only the castle background in the extra side columns.
        machine.video.wide.mirrorSideBackground = shape == 4 && fightState && cammyStage;
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
