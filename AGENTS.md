# HTCapture repository guidance

This repository contains the custom HTCapture/YeeCapture fork of RenderDoc.

- Read `docs/HTCapture.md` before changing YeeCapture branding/build outputs, process injection, child-process hooks, D3D/DXGI/GL wrapping, FidelityFX, Streamline, NVAPI, target control, or the repository-owned capture skills.
- Treat `docs/HTCapture.md` as the stable human-facing entry point. If its content is split later, keep that path as the index rather than replacing it with an unlinked directory.
- In the same change, update `docs/HTCapture.md` whenever a commit adds, removes, or materially changes fork behavior, compatibility policy, versioned diagnostics, validation requirements, or the local commit ledger. Refresh hashes in the ledger after rebasing `HTCapture`.
- Keep `v1.x` clean for upstream RenderDoc code; put local fork changes on `HTCapture` unless the user explicitly requests a different branch.
- Use `.agents/skills/attach-afop` for AFOP and `.agents/skills/attach-game-early` for other launcher-managed games. Do not generalise AFOP-specific FidelityFX/NVIDIA behavior into the generic workflow.
- Do not commit build outputs, captures, logs, crash dumps, generated profiles/state, or ad-hoc diagnostic binaries.
