# Changelog

## v0.1.1-preview — 2026-10-19

- Add optional **3 punches** and **3 kicks** keyboard/gamepad bindings for both players, with labels in all five interface languages.
- Fix E. Honda's widescreen side flicker: align the black edge guards with the bathhouse's slower background layer and disable its intermittent mirrored hole filling.
- Correct Super Combo finish detection to read the signed current-health word, preventing bright stage art from triggering the widescreen finish effect during normal play.
- Add an experimental Windows-to-Windows frame synchronization layer and local host/join helper in the source. This is a local/LAN test transport, without rollback, web-lobby integration, or FBNeo crossplay.
- Add regression tests for synchronization, malformed/disconnected peers, Honda camera movement, preservation of the original 4:3 viewport, and a real Super Combo finish.
- Refresh the Windows installer, including the launcher and required runtime libraries. ROMs and web-project files are excluded.

Validation: 649 Honda movement frames checked against the original viewport, plus a real Super Combo knockout. The synchronization prototype completed 6,000 matching frames in two local processes and matched an offline reference. Internet play and testing on two separate PCs remain outstanding.
