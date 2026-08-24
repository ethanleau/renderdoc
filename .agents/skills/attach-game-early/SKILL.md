---
name: attach-game-early
description: Diagnose launcher-managed Windows games and attach this repository's custom YeeCapture to the real game process before graphics initialization. Use when ordinary process attach selects a bootstrap process, arrives too late, or misses a launcher-created child; do not use to bypass anti-cheat or for games with a dedicated skill such as AFOP.
---

# Attach launcher-managed games early

Use a per-game JSON profile and the deterministic scripts in this skill. This workflow solves launch-chain and injection-timing failures; it does not make an unsupported graphics middleware or driver interface compatible.

If a dedicated game skill exists, use it instead. In particular, use `$attach-afop` for Avatar: Frontiers of Pandora because that game also requires source-level FidelityFX/NVIDIA compatibility patches.

## First-time diagnosis

1. Collect the target executable name/path and the normal launcher command. Do not ask the user to operate the launcher when these values can be discovered locally.
2. Create a profile with `scripts/New-GameAttachProfile.ps1`. Store it under `%LOCALAPPDATA%\YeeCapture\AttachProfiles` unless the user requests a repository-owned profile.
3. Run `scripts/Observe-GameLaunch.ps1 -ProfilePath <profile> -StopAfterObserve`. It records every matching process, its parent, path, and lifetime without injecting.
4. Read the report and update `selection.acceptParentNames` and `selection.rejectParentNames`. A short launcher-owned candidate is normally a bootstrap; the later game-specific-parent candidate is normally the real process. Read [profile-schema.md](references/profile-schema.md) when creating or changing a profile.
5. Run `scripts/Attach-GameEarly.ps1 -ProfilePath <profile>`. Treat the operation as successful only when it prints `GAME_ATTACH_OK`.

Do not infer success from injection output alone. The attach script verifies that the capture DLL is loaded, target control is listening on RenderDoc's reserved port range, the requested graphics-ready log marker is present when configured, and the process survives the configured stability interval. The generated profile requires a graphics-ready marker in both injection modes unless a demonstrated per-game logging limitation justifies opting out.

## Injection modes

- Prefer `direct`. It selects the real process from its path and parent, then injects immediately.
- Use `launcher-child` only when a dedicated per-game launcher creates the target and direct attach is demonstrably too late. This mode hooks `CreateProcess` in that dedicated launcher so the child is injected while its primary thread is suspended.
- Never use `launcher-child` on shared launchers such as Steam, Epic Games Launcher, Ubisoft Connect/`upc`, EA Desktop, GOG Galaxy, or Battle.net. The script rejects these names because it would inject unrelated launcher and web-helper children.
- Do not enable Global Hook, request elevation, disable Secure Boot, or change anti-cheat/security settings automatically. Stop and report if the game is protected against injection.

If an attached process reaches the graphics-ready state and then crashes, one clean retry is enough. Capture the game log and crash module; changing injection timing repeatedly will not fix a wrapped/native graphics-object incompatibility.

## Capture

Leave the game running after `GAME_ATTACH_OK`. The user can focus it and press `F12`, or the agent can trigger one frame without focus ambiguity:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\renderdoc\.agents\skills\attach-game-early\scripts\Capture-AttachedGame.ps1"
```

The most recent successful attach is used by default. The helper uses the built-in `yeecapturecmd targetcapture` command and does not depend on binaries under `analysis`. Its default capture timeout is 120 seconds; pass `-TimeoutSeconds` only when the game needs a different bounded wait. `-PreflightOnly` calls `targetcapture --connect-only`, verifies the formal target-control protocol, and never captures a frame. Captures can be several gigabytes; report the saved `.rdc` path and size.

Before triggering a capture, the helper revalidates the saved process start time, executable path, loaded capture DLL, profile identity, and target-control listener. An older state must be replaced by running attach again.
