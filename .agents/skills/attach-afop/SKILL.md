---
name: attach-afop
description: Automatically launch or restart Avatar: Frontiers of Pandora through Steam and attach this repository's custom YeeCapture early enough for D3D12 capture. Use when the user asks to attach, hook, or prepare AFOP for manual frame capture; do not use for other games or official RenderDoc builds.
---

# Attach YeeCapture to AFOP

Use the deterministic helper in this skill instead of Global Hook, UI attach, or a hand-written process watcher. AFOP first creates a Steam-owned bootstrap `afop.exe` that lives for roughly half a second; the real game is a later child of `UbisoftGameLauncher`. The helper identifies the real process by its parent and injects immediately, before D3D12 initialises.

This workflow requires the custom Release DLL in this repository. Its AFOP compatibility path keeps ordinary FidelityFX contexts on YeeCapture's wrapped D3D12 device; passing FidelityFX the real device while it receives wrapped game resources crashes in the NVIDIA user-mode driver.

## Attach workflow

1. Tell the user that a currently running AFOP instance will be restarted because injection must happen before D3D12 initialises. Do not request elevation and do not use Global Hook.
2. Run:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\renderdoc\.agents\skills\attach-afop\scripts\Attach-AFOP.ps1"
   ```

3. Treat the attach as successful only when the script prints `AFOP_ATTACH_OK`. The script verifies the per-game Steam Overlay setting, selects the Ubisoft-launched game process, checks that a D3D12 device and frame capturer were created, then confirms the process remains alive for 60 seconds. NVIDIA private-interface queries are diagnostic only and must not determine attach success because the real driver may legitimately return `E_NOINTERFACE`.
4. On success, tell the user that AFOP is ready and leave the game running. Do not trigger a capture unless the user asks for one.

The script saves the current process and target-control ID in `%LOCALAPPDATA%\Temp\YeeCapture\AFOPAttachState.json`.

AFOP's Steam Overlay must remain disabled for app `2840770`; it conflicts with the capture hooks. This machine is already configured that way, and the script fails clearly if the setting changes.

## Manual capture

The user can press `F12` while the game has focus. Steam Overlay is disabled for AFOP, so Steam should not consume the same key for an overlay screenshot. AFOP captures can be several gigabytes; ensure the Documents drive has sufficient free space.

If the user asks the agent to capture the current frame, avoid keyboard-focus ambiguity and run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\renderdoc\.agents\skills\attach-afop\scripts\Capture-AFOP.ps1"
```

This triggers one capture through the verified target-control connection with the built-in `yeecapturecmd targetcapture` command and reports the saved `.rdc` path. It does not depend on helper binaries under `analysis`.

## Failure handling

- The attach helper makes at most two clean launch attempts. Do not loop indefinitely.
- If both attempts fail, report the final error and the log path printed by the helper. Do not fall back to Global Hook: suspended child-process injection was tested and still reproduced the old crash, so earlier loading does not address a missing compatibility patch. Do not disable security features or switch to the official RenderDoc build.
- In a current build, the AFOP log should contain `FFX_DX12_POLICY policy=keep-wrapped-v1` followed by `FFX_DX12_POLICY result=success`. These are compatibility diagnostics rather than attach-success requirements because AFOP may create the FidelityFX context after the attach timeout. If they are absent after the game reaches its menu, rebuild `renderdoc.vcxproj` in x64 Release before retrying.
- Use the custom Release files under `D:\renderdoc\x64\Release`. The AFOP compatibility patches exist only in this build.
