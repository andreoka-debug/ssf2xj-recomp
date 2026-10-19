# Super Street Fighter II X Recomp for PC (prototype)

An experimental PC port of **Super Street Fighter II X (Japan 940223)** built with [cps2recomp](https://github.com/tmcleroy/cps2recomp). This repository contains the port-specific source, toolkit patches, recompilation entry points, and test movies. **It does not contain ROMs, game keys, EEPROM files, or CPS2X Web files.** You need your own `ssf2xj.zip` and `ssf2t.zip` ROM sets to play.

Windows is the current focus. The prototype supports local play, CRT and curvature filters, widescreen fight stages, and a training mode with P2 character selection, health and meter options, input history, damage readouts, hitboxes, frame data, and dummy recording/replay in three slots. Menus and other non-fight screens remain at 4:3. Widescreen renders extra stage area without stretching fighters or changing the gameplay camera; some stage edges still have graphical artifacts. Frame counts are measured in displayed frames and have not been validated at every game speed.

## Download and install

Download `SSF2X-Recomp-Setup.exe` from the [Releases page](https://github.com/andreoka-debug/ssf2xj-recomp/releases). The installer includes the launcher and required runtime libraries, but **no ROMs**. It installs for the current Windows user in `%LOCALAPPDATA%\Programs\SSF2X Recomp`, adds a Start menu shortcut, and opens the launcher when installation finishes. Select your own ROM ZIP files in the launcher. You do not need MSYS2 to run the installed program. Uninstall through **Installed apps**; your local ROMs and preferences are kept.

## Credits and relationship to upstream

Tommy McLeroy's [cps2recomp](https://github.com/tmcleroy/cps2recomp) toolkit provides static recompilation, CPS-2 emulation, audio, the user interface, and shared training components. His [Vampire Savior Recomp](https://github.com/tmcleroy/vampire-savior-recomp) was the reference for the training workflow and features. The hitbox viewer from [pof/ssf2xj](https://github.com/poliva/ssf2xj/blob/master/lua/pof-ssf2x-training2020.lua), based on work by Dammit, MZ, and Felineki, guided the SSF2X collision-table implementation.

This is a complementary port, not an official version of those projects. `ssf2xj/src/` contains the SSF2X-specific integration; the `.patch` files contain the necessary toolkit changes. Source code is released under GPL-3.0, consistent with the [upstream toolkit license](https://github.com/tmcleroy/cps2recomp/blob/main/LICENSE). Capcom and other rights holders retain their rights to the game and its data.

## Build on Windows

Install Git, PowerShell 7 or Windows PowerShell, and [MSYS2 UCRT64](https://www.msys2.org/) with the build dependencies for `cps2recomp` (CMake, Ninja, Python, a C/C++ compiler, Make, and SDL3). Put `ssf2xj.zip` and `ssf2t.zip` in the same folder. From the repository root, run:

```powershell
./build.ps1 -RomPath 'C:\path\to\roms\ssf2xj.zip'
```

The script checks out `cps2recomp` at commit `fdd55ecbd20c9197c85ef526ecebd44879ced82e`, applies the patches, copies the game sources and entry points, then builds the game and launcher. Generated files stay under `_build/`, which Git ignores. The first recompilation may take a while. Use `-MsysRoot` if MSYS2 is installed elsewhere.

Run `play.cmd` without arguments to open the launcher, choose the ROM sets, and configure controls. If both ZIP files are in `roms/`, the main ROM is preselected. To start the game directly:

```powershell
./play.ps1 -RomPath 'C:\path\to\roms\ssf2xj.zip'
```

In the launcher's **Controllers** page, assign **3 punches** (LP + MP + HP) and **3 kicks** (LK + MK + HK) to a keyboard key, gamepad button, or trigger for either player. These optional shortcuts start unassigned and press all three attacks together while held. Existing individual attack bindings remain available.

After building, run `./package-windows.ps1` to create `dist/SSF2X-Recomp-Setup.exe`. It uses the IExpress tool included with Windows and the project's icon. ROMs are never packaged.

## Options and training

An experimental **Windows-to-Windows match synchronization** layer is available for local tests. See [native synchronization](netplay/README.md) for the host/join helper, protocol, tests and current limits. It is not connected to the web lobby yet.

Preferences and the locally generated EEPROM are stored in `userdata/`, which Git ignores. Open the in-game menu with `Esc` or `F1`. Choose English, Portuguese, Spanish, Japanese, or French in the launcher's **Language** field or in **Display → Interface language** in the game; both use the same saved setting. This translates the PC port's interface, while the arcade game's original text stays as it was. Japanese uses a Windows font; no font files are bundled.

In **Display → Wall helper**, enable lines marking the original playable area's boundaries while using widescreen. In **Alternative modes → Training mode**, choose the P2 character, including **Gouki (Akuma)**, and select **Start**. Enable **Hitboxes**, **Frame data**, or **Frame meter** as needed. On the character select screen, hold **up + heavy punch + heavy kick** with the cursor over **Ryu** to select Gouki quickly. In the **Dummy** section, choose a slot, enable **Record** (P1 controls P2), then enable **Replay**. **Start over** restarts the round without deleting recordings.

## Custom music

Choose a folder under **Custom soundtrack** on the launcher's **Game** page and enable **Custom soundtrack** under **Features** (or in the in-game menu). MP3, OGG, FLAC, and WAV files without special names play as a playlist. The game creates `songs.txt` in the folder. To replace a particular track, name an audio file after its song number, for example `07.mp3`. **On, shuffled** randomizes the playlist. Sound effects and voices still come from the game.

To replace only character fight themes, copy the [example `soundtrack.ini`](examples/soundtrack-personagens.ini) into the selected folder, rename it to `soundtrack.ini`, and add files such as `Ryu.mp3`, `Ryu-Perigo.mp3`, `Ken.mp3`, and `Ken-Perigo.mp3`. Each character has a normal theme and a faster danger theme; the game requests different song codes when it changes music during a fight. Normal codes were checked in training matches on all 16 stages, and the danger codes were checked against the game program. Tracks you do not provide continue to use the original music. `songs.txt` is generated for reference; changes to it are overwritten.

## Testing and upstream integration

Using local ROM sets, the port built from scratch on Windows and passed the `cps2recomp` checks for translated-versus-interpreted equivalence and repeatability. An automated test confirmed the REC-to-PLAY transition and identical dummy playback after save/load. Comprehensive manual testing across every stage and move is still outstanding.

To integrate this port into the toolkit, apply the `.patch` files listed by `build.ps1` in that order to the pinned toolkit commit, copy `ssf2xj/` to `games/ssf2xj/`, copy the three native synchronization sources from `netplay/` to `port/`, and copy `entries/entries.txt*` to `work/ssf2xj/`. The entry points are addresses, not ROM bytes. `build.ps1` automates these steps.
