# YeeCapture

- Keep `v1.x` upstream-only; make fork changes on `YeeCapture` unless requested otherwise.
- For fork behavior, compatibility, diagnostics, validation, history, or upgrades, read only the relevant section of `docs/YeeCapture.md` and update it when behavior or its commit ledger changes.
- Use `.agents/skills/attach-afop` for AFOP and `.agents/skills/attach-game-early` for other launcher-managed games; keep AFOP-specific compatibility out of the generic workflow.
- Do not commit generated outputs, captures, logs, dumps, profiles/state, or diagnostic binaries.
