# Game attach profile

Profiles are JSON files. Create the initial file with `New-GameAttachProfile.ps1`; edit only the fields needed after observing a real launch.

```json
{
  "profileVersion": 1,
  "slug": "example-game",
  "displayName": "Example Game",
  "mode": "direct",
  "repoRoot": "D:\\renderdoc",
  "target": {
    "processName": "example",
    "expectedPath": "C:\\Games\\Example\\example.exe"
  },
  "launch": {
    "filePath": "C:\\Program Files (x86)\\Steam\\steam.exe",
    "arguments": "-applaunch 123456",
    "workingDirectory": ""
  },
  "stop": {
    "filePath": "C:\\Program Files (x86)\\Steam\\steam.exe",
    "arguments": "steam://stop/123456",
    "graceSeconds": 15,
    "forceAfterGrace": false
  },
  "selection": {
    "launcherProcessName": "",
    "acceptParentNames": [],
    "rejectParentNames": ["steam"],
    "stableMilliseconds": 1000
  },
  "capture": {
    "template": "C:\\Users\\user\\Documents\\YeeCapture\\example-game",
    "options": [],
    "attachTimeoutSeconds": 45,
    "postAttachStabilitySeconds": 60,
    "requireReadyLog": true,
    "readyLogPatterns": [
      "Adding (D3D11|D3D12|Vulkan|OpenGL) frame capturer"
    ]
  }
}
```

## Selection

- Process names omit `.exe`.
- `expectedPath` should be set when multiple installed copies exist. Empty means any path with the matching process name.
- Stop/restart operations require `expectedPath` whenever an existing same-named process is present. Only processes whose executable path matches it are affected; same-named processes at other paths are left alone.
- `acceptParentNames`: when non-empty, direct mode selects a candidate immediately only if its parent is listed. This is the preferred deterministic rule.
- `rejectParentNames`: candidates with these parents are ignored even if they survive the fallback interval.
- `stableMilliseconds`: fallback used only when `acceptParentNames` is empty.
- `launcherProcessName`: required only for `launcher-child`; it must name a dedicated per-game launcher.

## Capture verification

- `options` accepts YeeCapture CLI capture options beginning with `--opt-`.
- `requireReadyLog` defaults to true in both injection modes. Set it false only when child injection demonstrably shares a launcher log or when validating injection into a non-graphics test process.
- `readyLogPatterns` are regular expressions; any one match satisfies graphics readiness.
- Stability is checked after module/readiness verification and after confirming that target control is listening on RenderDoc's reserved `38920`-`38927` range.

Successful attach state uses schema version 2 and records the process start time, executable path, loaded capture DLL path, and target-control ID. Capture refuses an older or stale state and requires a new attach instead of trusting a reused PID.

## Failure boundary

This profile describes how to reach the correct process. It must not contain instructions for bypassing DRM or anti-cheat. Game-specific graphics compatibility code belongs in a dedicated skill/profile and source patch, not in this generic schema.
