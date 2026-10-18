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

void RegisterGame() {
    RegisterTraining();
    TheGame().inTraining = TrainingActive;
    TheGame().noRunAhead = TrainingActive;
    // Only gameplay states contain a stage that can be drawn beyond the CPS-2's normal 384 columns.
    // FF8009 was traced through character select, VS, rounds and match transitions in ssf2xj.
    // This expands the rendered view; it does not change the game's camera or collision limits.
    // Where a stage has no extra tiles at its edge, the renderer mirrors adjacent art instead of leaving black.
    TheGame().beforeFrame = [](Machine &machine) {
        TrainingBeforeFrame(machine);
        const uint8_t state = machine.ram[0x8009];
        const bool fightState = state == 8 || state == 10 || state == 12;
        // Keep the same picture width through the score count and the short pre-round state.
        // The first VS screen still enters at 4:3; only a match that has already reached gameplay
        // keeps its wide stage until character selection or the match exit begins.
        const bool betweenRounds = (state == 6 || state == 14 || state == 16) && machine.video.wide.side > 0;
        const bool fight = fightState || betweenRounds;
        const int shape = FeatureValue("display_shape");
        machine.video.wide.side = shape >= 3 && fight ? Video::kMaxSide : 0;
        machine.video.wide.guardStageEdges = shape == 3 && fight;
        // Traced in multiple stages: scroll 2 moves from 0 at the left limit to 384 at the right.
        machine.video.wide.maxStageScrollX = 384;
        const bool cammyStage = machine.ram[0x8c4f] == 0x0c;
        const bool deeJayStage = machine.ram[0x8c4f] == 0x0f;
        machine.video.wide.squareBlackEdges = shape == 3 && fight && cammyStage;
        machine.video.wide.fillHoles = shape == 4 && fight;
        machine.video.wide.fillSideHoles = shape == 3 && fight && !cammyStage && !deeJayStage;
        machine.video.wide.blankSideBackground = shape == 3 && fight && deeJayStage;
        // A Super Combo knockout paints its burst across the full raster in scroll 3, but
        // stage scrolls obscure the extra columns. FF8462/FF8862 are remaining life;
        // FinishPicture also checks for the burst so ordinary KO frames keep their stage.
        machine.video.wide.extendSuperFinish = shape == 3 && state == 10 &&
            (machine.ram[0x8462] == 0 || machine.ram[0x8862] == 0);
        // Cammy's castle leaves stale scroll tiles beyond the original 384 columns.
        // FF8C4F holds the selected stage (0x0C = Cammy); preserve wide objects
        // while mirroring only the castle background in the extra side columns.
        machine.video.wide.mirrorSideBackground = shape == 4 && fight && machine.ram[0x8c4f] == 0x0c;
    };
    TheGame().frame = TrainingFrame;
    TheGame().state = TrainingState;
    TheGame().widestPicture = [] { return FeatureValue("display_shape") >= 3 ? Video::kMaxWidth : Video::kWidth; };
}

} // namespace cps2recomp
