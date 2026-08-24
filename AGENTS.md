# YeeCapture repository guidance

This repository contains the custom YeeCapture fork of RenderDoc.

- Read `docs/YeeCapture.md` before changing YeeCapture branding/build outputs, process injection, child-process hooks, D3D/DXGI/GL wrapping, FidelityFX, Streamline, NVAPI, target control, or the repository-owned capture skills.
- Treat `docs/YeeCapture.md` as the stable human-facing entry point.
- In the same change, update `docs/YeeCapture.md` whenever a commit adds, removes, or materially changes fork behavior, compatibility policy, versioned diagnostics, validation requirements, or the local commit ledger. Refresh hashes in the ledger after rebasing `YeeCapture`.
- Keep `v1.x` clean for upstream RenderDoc code; put local fork changes on `YeeCapture` unless the user explicitly requests a different branch.
- Use `.agents/skills/attach-afop` for AFOP and `.agents/skills/attach-game-early` for other launcher-managed games. Do not generalise AFOP-specific FidelityFX/NVIDIA behavior into the generic workflow.
- Do not commit build outputs, captures, logs, crash dumps, generated profiles/state, or ad-hoc diagnostic binaries.
