// SPDX-License-Identifier: GPL-3.0-only
// Local SSF2X training session. Game-specific memory addresses are for ssf2xj (Japan 940223).
#include "fight.h"
#include "game.h"
#include "machine.h"
#include "state.h"

#include <algorithm>
#include <vector>

namespace cps2recomp {
namespace {

constexpr uint32_t kMode = 0xFF8009;
constexpr uint32_t kClock = 0xFF8DCE;
constexpr uint32_t kPlayer[2] = {0xFF8400, 0xFF8800};
constexpr uint16_t kFullHealth = 0x90;
constexpr int kPictureWidth = 384, kPictureHeight = 224, kWideSide = 64;

bool g_boot = false;
int32_t g_bootFrames = 0;
int32_t g_dummyFrames = 0;
bool g_wasFighting = false;
uint16_t g_lastHealth[2] = {kFullHealth, kFullHealth};
int32_t g_healDelay[2] = {0, 0};
int32_t g_afterBootFrames = 0;
int32_t g_selectCycle = 0;
uint16_t g_selectPulse = 0;
bool g_selectDone = false;
fight::InputHistory g_inputs;
fight::Combos g_combos;
fight::Dummy g_dummy;
fight::Frames g_frames;
std::vector<uint8_t> g_roundStart;

// Box tables and animation IDs from the SSF2X CPS-2 player objects. Read-only:
// the hit tests still run entirely in the game's own code.
bool GameByte(uint32_t address, uint8_t &value) {
    if (address >= 0xFF0000 && address <= 0xFFFFFF) { value = Ram8(address); return true; }
    if (address < ProgramSize()) { value = *Program(address); return true; }
    return false;
}
bool GameWord(uint32_t address, uint16_t &value) {
    uint8_t hi, lo;
    if (!GameByte(address, hi) || !GameByte(address + 1, lo)) return false;
    value = static_cast<uint16_t>((hi << 8) | lo);
    return true;
}
void AddPlayerBoxes(uint32_t player, int side, std::vector<HudBox> &boxes, bool &attack, bool projectile = false) {
    if (Ram16(player) <= 0x0100) return;
    const uint32_t animation = Ram32(player + 0x1A), table = Ram32(player + 0x34);
    const int x = static_cast<int16_t>(Ram16(player + 0x06)) - Ram16(0xFF8ED4) + side;
    const int y = kPictureHeight - (static_cast<int16_t>(Ram16(player + 0x0A)) - 15) + Ram16(0xFF8ED8);
    if (x < -128 || x > kPictureWidth + 128 + 2 * side || y < -128 || y > kPictureHeight + 128) return;
    static constexpr struct { uint8_t offset, id, stride, r, g, b; bool active; } kinds[] = {
        {0x08, 0x0D, 4, 50, 220, 90, false}, // push
        {0x00, 0x08, 4, 65, 145, 255, false},
        {0x02, 0x09, 4, 65, 145, 255, false},
        {0x04, 0x0A, 4, 65, 145, 255, false},
        {0x06, 0x0C, 16, 255, 60, 60, true},
    };
    for (const auto &kind : kinds) {
        if (kind.active && !projectile && Ram8(player + 0x03) == 0) continue;
        uint8_t id, hv, vv, hw, hh;
        uint16_t relative;
        if (!GameByte(animation + kind.id, id) || !id ||
            !GameWord(table + kind.offset, relative)) continue;
        const uint32_t address = table + static_cast<int16_t>(relative) + id * kind.stride;
        if (!GameByte(address, hv) || !GameByte(address + 1, vv) ||
            !GameByte(address + 2, hw) || !GameByte(address + 3, hh)) continue;
        if (!hw || !hh || hw > 96 || hh > 96) continue;
        int horizontal = static_cast<int8_t>(hv);
        if (kind.active) {
            uint8_t alternate;
            if (GameByte(address + 5, alternate) && alternate >= 0x80) horizontal = -static_cast<int>(alternate);
            attack = true;
        }
        const int cx = x + horizontal * (Ram8(player + 0x12) == 1 ? -1 : 1);
        const int cy = y - static_cast<int8_t>(vv);
        boxes.push_back({cx - hw, cy - hh, hw * 2, hh * 2,
                         static_cast<uint8_t>(projectile ? (kind.active ? 255 : 50) : kind.r),
                         static_cast<uint8_t>(projectile ? (kind.active ? 140 : 230) : kind.g),
                         static_cast<uint8_t>(projectile ? (kind.active ? 20 : 230) : kind.b), 48});
    }
}

bool Fighting() {
    return Ram8(kMode) == 0x0A;
}

void Start(Machine &machine) {
    SetFeature("training_mode", true);
    SetFeature("training_health", true);
    SetFeature("training_no_dizzy", true);
    SetFeature("training_input_display", true);
    SetFeature("training_combo_readout", true);
    g_boot = true;
    g_bootFrames = g_dummyFrames = 0;
    g_wasFighting = false;
    g_lastHealth[0] = g_lastHealth[1] = kFullHealth;
    g_healDelay[0] = g_healDelay[1] = 0;
    g_afterBootFrames = g_selectCycle = 0;
    g_selectPulse = 0;
    g_selectDone = false;
    g_inputs.Clear();
    g_combos.Clear();
    g_frames.Reset();
    g_dummy.Afresh();
    g_roundStart.clear();
    SetHud({});
    SetBoxes({});
    machine.Reset();
    SetFastForward(true);
}

void StartOver(Machine &machine) {
    if (!FeatureOn("training_mode") || g_roundStart.empty()) {
        return;
    }
    fight::Dummy recorded = g_dummy;
    Archive saved(g_roundStart, false);
    machine.Serialize(saved);
    g_dummy = std::move(recorded);
    g_inputs.Clear();
    g_combos.Clear();
    g_frames.Reset();
    g_dummy.Afresh();
    g_dummyFrames = 0;
    g_lastHealth[0] = g_lastHealth[1] = kFullHealth;
    g_healDelay[0] = g_healDelay[1] = 0;
    SetHud({});
    SetBoxes({});
}

void Boot(Machine &machine) {
    // Replays only the coin/Start steps of the verified two-player startup. The player takes
    // control on the character-select screen; the game itself performs all transitions.
    const int frame = g_bootFrames++;
    machine.players[0] = machine.players[1] = 0;
    if (frame >= 1200 && frame < 1204) {
        machine.players[0] = machine.players[1] = kCoin;
    } else if ((frame >= 1230 && frame < 1234) || (frame >= 1500 && frame < 1504)) {
        machine.players[0] = machine.players[1] = kStart;
    }
    if (g_bootFrames >= 1900) {
        g_boot = false;
        SetFastForward(false);
    }
}

void SelectDummyCharacter(Machine &machine) {
    const int choice = FeatureValue("training_character") - 1;
    if (choice < 0 || choice > 15 || g_selectDone || ++g_afterBootFrames < 200 || Ram8(kMode) != 0) {
        return;
    }
    // Character-select portrait grid, traced from P2 directional inputs on ssf2xj.
    constexpr uint8_t grid[2][8] = {
        {0, 1, 2, 3, 13, 14, 10, 9},
        {4, 5, 6, 7, 12, 15, 11, 8},
    };
    if (g_selectCycle == 0) {
        const int current = Ram8(0xFF8BDF);
        if (current == choice) {
            g_selectPulse = kPunch1;
        } else {
            int row = -1, column = -1, targetRow = -1, targetColumn = -1;
            for (int r = 0; r < 2; ++r) {
                for (int c = 0; c < 8; ++c) {
                    if (grid[r][c] == current) { row = r; column = c; }
                    if (grid[r][c] == choice) { targetRow = r; targetColumn = c; }
                }
            }
            if (row < 0 || targetRow < 0) {
                return;
            }
            if (row != targetRow) {
                g_selectPulse = row == 0 ? kDown : kUp;
            } else {
                const int right = (targetColumn - column + 8) % 8;
                const int left = (column - targetColumn + 8) % 8;
                g_selectPulse = right <= left ? kRight : kLeft;
            }
        }
    }
    if (g_selectCycle < 4) {
        machine.players[1] = g_selectPulse;
    } else {
        machine.players[1] = 0;
        if (g_selectPulse == kPunch1) {
            g_selectDone = true;
        }
    }
    g_selectCycle = (g_selectCycle + 1) % 20;
}

void BeforeFrame(Machine &machine) {
    if (g_boot) {
        Boot(machine);
        return;
    }
    if (!FeatureOn("training_mode")) {
        return;
    }
    SelectDummyCharacter(machine);
    const int stage = FeatureValue("training_stage") - 1;
    if (stage >= 0 && Ram8(kMode) == 0x04) {
        // SSF2X's stage choice and its copy used when the match starts.
        SetRam8(0xFF8C4F, static_cast<uint8_t>(stage));
        SetRam8(0xFF8C51, 0);
        SetRam8(0xFFE18B, static_cast<uint8_t>(stage));
    }
    if (!Fighting()) {
        return;
    }
    // The game uses a BCD clock and two health words per fighter. Keep them full before
    // the game can decide a timeout or knockout, without changing non-training matches.
    SetRam16(kClock, 0x9928);
    if (FeatureOn("training_health")) {
        for (int n = 0; n < 2; ++n) {
            const uint32_t player = kPlayer[n];
            const uint8_t action = Ram8(player + 0x51);
            const bool reacting = action == 0x08 || action == 0x0E || action == 0x14;
            if (reacting) {
                g_healDelay[n] = 10;
            } else if (g_healDelay[n] > 0) {
                --g_healDelay[n];
            }
            const uint16_t life = Ram16(player + 0x7A);
            // Leave the damage and hit reaction intact. Refill only after recovery, except
            // for a near-knockout where the game could end the training round.
            if ((life < kFullHealth && !reacting && g_healDelay[n] == 0) || life <= 1) {
                SetRam16(player + 0x78, kFullHealth);
                SetRam16(player + 0x7A, kFullHealth);
                SetRam16(player + 0x20A, kFullHealth);
                if (life <= 1) {
                    SetRam8(0xFF82F2, 0);
                }
            }
        }
    }
    if (FeatureOn("training_no_dizzy")) {
        for (uint32_t player : kPlayer) {
            SetRam16(player + 0xAA, 0);
        }
    }
    if (FeatureOn("training_full_meter")) {
        SetRam8(0xFF8702, 0x30);
        SetRam8(0xFF8B02, 0x30);
    }
    switch (FeatureValue("training_dummy")) {
    case 1: machine.players[1] = 0; break;
    case 2: machine.players[1] = kDown; break;
    case 3: machine.players[1] = (++g_dummyFrames % 90 < 5) ? kUp : 0; break;
    default: break; // manual player 2
    }
    fight::DummyView view;
    view.fighting = true;
    view.doing = g_frames.Now(1);
    view.onRight = Ram16(kPlayer[1] + 0x54) >= Ram16(kPlayer[0] + 0x54);
    view.sinceHit = g_combos.Since(1);
    g_dummy.BeforeFrame(machine.players[0], machine.players[1], view);
}

void AfterFrame(Machine &machine) {
    if (!FeatureOn("training_mode") || g_boot) {
        g_wasFighting = false;
        SetHud({});
        SetBoxes({});
        return;
    }
    const bool fighting = Fighting();
    if (fighting && !g_wasFighting) {
        g_roundStart.clear();
        Archive saved(g_roundStart, true);
        machine.Serialize(saved);
        g_inputs.Clear();
        g_combos.Clear();
        g_frames.Reset();
        g_dummy.Afresh();
        g_lastHealth[0] = g_lastHealth[1] = kFullHealth;
        g_healDelay[0] = g_healDelay[1] = 0;
    }
    g_wasFighting = fighting;
    if (!fighting) {
        SetHud({});
        SetBoxes({});
        return;
    }
    std::vector<HudLine> lines;
    std::vector<HudBox> boxes;
    bool active[2] = {false, false};
    if (FeatureOn("training_hitboxes") || FeatureOn("training_frame_data") || FeatureOn("training_frame_meter")) {
        const int side = FeatureValue("display_shape") >= 3 ? kWideSide : 0;
        for (int n = 0; n < 2; ++n) {
            std::vector<HudBox> found;
            AddPlayerBoxes(kPlayer[n] + 0x4E, side, found, active[n]);
            if (FeatureOn("training_hitboxes")) boxes.insert(boxes.end(), found.begin(), found.end());
        }
        if (FeatureOn("training_hitboxes")) {
            for (int n = 0; n < 8; ++n) {
                const uint32_t projectile = 0xFF97A2 + n * 0xC0;
                if (Ram16(projectile) == 0x0101) {
                    bool unused = false;
                    AddPlayerBoxes(projectile, side, boxes, unused, true);
                }
            }
        }
    }
    for (int n = 0; n < 2; ++n) {
        const uint8_t state = Ram8(kPlayer[n] + 0x51);
        fight::StepView view;
        view.control = state == 0x08 || state == 0x0E || state == 0x14 ? fight::StepView::Reeling : fight::StepView::Own;
        // These action states are observable at the end of the game's frame. A box becoming
        // active is the most reliable marker for the active frames of a move.
        view.canAct = state == 0 || state == 2;
        view.attacking = !view.canAct;
        view.attackOut = active[n];
        g_frames.Step(n, view);
    }
    g_combos.Frame();
    for (int n = 0; n < 2; ++n) {
        const uint16_t health = Ram16(kPlayer[n] + 0x7A);
        if (FeatureOn("training_health") && health < g_lastHealth[n]) {
            const uint8_t doing = Ram8(kPlayer[n] + 0x51);
            const bool inCombo = g_combos.Since(n) < 45 && (doing == 0x0E || doing == 0x14);
            g_combos.Hit(n, g_lastHealth[n] - health, 0, inCombo);
            g_frames.Contact(n, false);
        }
        g_lastHealth[n] = health;
        if (FeatureOn("training_combo_readout")) {
            g_combos.Hud(n, lines, 0.17f, false);
        }
    }
    if (FeatureOn("training_input_display")) {
        const bool p1Left = Ram16(kPlayer[0] + 0x54) > Ram16(kPlayer[1] + 0x54);
        g_inputs.Push(0, machine.players[0], p1Left);
        g_inputs.Push(1, machine.players[1], !p1Left);
        g_inputs.Hud(0, lines, 0.30f);
        g_inputs.Hud(1, lines, 0.30f);
    }
    g_frames.Hud(FeatureOn("training_frame_data"), FeatureOn("training_frame_meter"), lines, boxes,
                 0.70f, 193, kPictureWidth, FeatureValue("display_shape") >= 3 ? kWideSide : 0);
    g_dummy.Hud(lines);
    SetBoxes(std::move(boxes));
    SetHud(std::move(lines));
}

void State(Archive &ar) {
    ar.Mark(0x53534654); // SSFT
    ar.Pod(g_boot);
    ar.Pod(g_bootFrames);
    ar.Pod(g_dummyFrames);
    ar.Pod(g_wasFighting);
    ar.Pod(g_lastHealth);
    ar.Pod(g_healDelay);
    ar.Pod(g_afterBootFrames);
    ar.Pod(g_selectCycle);
    ar.Pod(g_selectPulse);
    ar.Pod(g_selectDone);
    ar.Pod(g_inputs);
    ar.Pod(g_combos);
    g_dummy.Serialize(ar);
    g_frames.Serialize(ar);
    ar.Vec(g_roundStart);
}

} // namespace

void RegisterTraining() {
    Game &game = TheGame();
    game.pages.push_back({"Alternative modes", "", "Modes beyond the original arcade game.", true});
    game.pages.push_back({"Training mode", "Alternative modes", "Practise against a second player or a basic dummy."});
    game.commands.push_back({"training_start", "Training mode", "Start",
                             "Reset the board, add two players and open character select for practice.", Start});
    game.commands.push_back({"training_restart", "Training mode", "Start over",
                             "Return to the beginning of this training round (Backspace or the gamepad's left shoulder).",
                             StartOver, kKeyRestart});
    game.features.push_back({"training_mode", "Training mode", "Training session",
                             "Keeps the clock at 99; Start opens character select. Reset ends the session.", kFeatureSession});
    game.features.push_back({"training_health", "Training mode", "Full health",
                             "Restores both fighters' health every frame so a training round does not end by knockout.",
                             kFeatureEnhanced});
    game.features.push_back({"training_no_dizzy", "Training mode", "No dizzy",
                             "Clears both fighters' dizzy counters during training.", kFeatureEnhanced});
    game.features.push_back({"training_full_meter", "Training mode", "Full super meter",
                             "Keeps both super meters full during training.", 0});
    game.features.push_back({"training_input_display", "Training mode", "Input display",
                             "Shows both players' recent directions, buttons and held frames.", kFeatureEnhanced});
    game.features.push_back({"training_combo_readout", "Training mode", "Damage readout",
                             "Shows the damage of each hit and counts hits that land while the opponent reels.",
                             kFeatureEnhanced});
    game.features.push_back({"training_hitboxes", "Training mode", "Hitboxes",
                             "Show the game's blue vulnerability, red attack and green push boxes.", kFeatureEnhanced});
    game.features.push_back({"training_frame_data", "Training mode", "Frame data",
                             "Show startup, active and recovery counts measured during training.", kFeatureEnhanced});
    game.features.push_back({"training_frame_meter", "Training mode", "Frame meter",
                             "Show recent startup, active, recovery and hit-reaction frames for both players.", kFeatureEnhanced});
    game.features.push_back({"training_stage", "Training mode", "Stage",
                             "Choose the stage for the next training match; Original choice lets the game decide.",
                             0, {"Original choice", "Ryu", "Honda", "Blanka", "Guile", "Ken", "Chun-Li",
                                 "Zangief", "Dhalsim", "Dictator", "Sagat", "Boxer", "Claw", "Cammy",
                                 "T. Hawk", "Fei-Long", "Dee Jay"}});
    game.features.push_back({"training_dummy", "Training mode", "Player 2",
                             "Choose manual control, still, crouching or repeated jumps.", 0,
                             {"Manual", "Stand", "Crouch", "Jump"}});
    game.features.push_back({"training_character", "Training mode", "P2 character",
                             "Choose the training opponent automatically on the character-select screen.", 0,
                             {"Choose on screen", "Ryu", "Honda", "Blanka", "Guile", "Ken", "Chun-Li",
                              "Zangief", "Dhalsim", "Dictator", "Sagat", "Boxer", "Claw", "Cammy",
                              "T. Hawk", "Fei-Long", "Dee Jay"}});
    game.features.push_back({"dummy_record", "Dummy", "Record",
                             "P1 controls P2 while its inputs are recorded, up to 20 seconds in the selected slot.",
                             kFeatureExclusive});
    game.features.push_back({"dummy_replay", "Dummy", "Replay",
                             "P2 repeats the selected recording with a short pause between loops.",
                             kFeatureExclusive});
    game.features.push_back({"dummy_replay_any", "Dummy", "Replay any slot",
                             "P2 repeats one of the recorded slots, chosen at random each loop.",
                             kFeatureExclusive});
    game.features.push_back({"dummy_slot", "Dummy", "Recording slot",
                             "Choose which recording to make or replay.", 0, {"1", "2", "3"}});
    TrainingAids({"training_health", "training_no_dizzy", "training_full_meter",
                  "training_input_display", "training_combo_readout", "training_stage", "training_dummy",
                  "training_character", "training_hitboxes", "training_frame_data", "training_frame_meter",
                  "dummy_record", "dummy_replay", "dummy_replay_any", "dummy_slot"});
}

void TrainingBeforeFrame(Machine &machine) { BeforeFrame(machine); }
void TrainingFrame(Machine &machine) { AfterFrame(machine); }
void TrainingState(Archive &ar) { State(ar); }
bool TrainingActive() { return FeatureOn("training_mode"); }

} // namespace cps2recomp
