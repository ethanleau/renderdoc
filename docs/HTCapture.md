# HTCapture 本地改动与升级维护

本文是 `ethanleau/renderdoc` 中 HTCapture/YeeCapture 自定义分支的稳定文档入口，记录相对官方 RenderDoc 的功能差异、兼容性策略、诊断标记、验证结果和升级流程。

## 文档布局

目前继续保留单文件路径 `docs/HTCapture.md`。现有内容仍能在一份文档内清晰维护，单独创建目录只会增加跳转层级。如果以后需要拆分 AFOP、图形中间件兼容和升级手册，应保留本路径作为索引页，再把详细内容放入 `docs/HTCapture/`。

仓库根目录的 `AGENTS.md` 只负责让代码代理发现本文件并遵守维护规则；它不是面向开发者的第二份文档，也不复制这里的实现说明。

## 当前功能差异

当前 `HTCapture` 分支相对于干净的 `v1.x` 基线包含以下功能：

1. 增加 YeeCapture Windows 品牌化构建目标和集中构建配置。
2. 增加 YeeCapture OpenGL 支持。
3. 使用挂起进程和 `SetThreadContext` 完成注入，并通过 `ShellExecuteExW` hook 跟踪启动的子进程（例如 HTGame）。
4. 增加 FidelityFX/FSR3 兼容路径：
   - 扫描 delay-IAT，使延迟加载的图形 API 也能被发现；
   - 监视晚加载的 `amd_fidelityfx_dx12.dll`；
   - 仅在 `ffxCreateContext` 的 15 字节入口机器码完全匹配时安装 x64 inline hook；
   - 将 Frame Generation queue 临时解包，并将 FFX 返回的 swapchain 重新包装为 RenderDoc swapchain；
   - 为 FFX 返回的借用式 swapchain 保留兼容引用，避免调用方释放后继续使用造成崩溃。
5. hook NVIDIA Streamline 的 `sl.interposer.dll` D3D12/DXGI 导出，使通过 interposer 创建的 device 和 factory 仍进入 RenderDoc 包装路径。
6. 对普通 FidelityFX D3D12 backend context 保持 wrapped device，避免 native device 与 wrapped resources 混用导致 NVIDIA 用户态驱动崩溃。
7. 允许 Streamline 启动阶段所需的只读 NVAPI GPU/驱动发现查询，并记录每个 NVAPI ID 的首个路由结果；未开放全部 vendor extension。
8. 增加 `yeecapturecmd targetcapture`，并提供 AFOP 专用及可配置的 launcher-managed game early-attach skills，不再依赖 `analysis` 下的临时工具。

## 当前提交记录

下表记录功能提交；纯文档修订未列入。`HTCapture` 经过 rebase 后提交哈希会改变，应按提交主题重新核对并更新本表。

| 提交 | 主题 | 主要范围 |
| --- | --- | --- |
| `06f1b5840` | `feat: add YeeCapture Windows capture build` | Windows 品牌化目标、输出名、资源、打包和安装配置 |
| `27aba241f` | `feat: add YeeCapture OpenGL support` | OpenGL hook、driver/replay 和 Windows/Linux 平台适配 |
| `559f72383` | `Add suspended injection and ShellExecute child capture` | 早期注入和 launcher child 跟踪 |
| `c6c9f5aec` | `Support FidelityFX frame generation capture` | FFX context hook、queue/swapchain 包装和生命周期兼容 |
| `40eacff51` | `Support NVIDIA Streamline interposer hooks` | `sl.interposer.dll` 的 D3D12/DXGI 导出 hook |
| `e0ca749df` | `Fix FidelityFX DX12 backend device wrapping` | wrapped backend device 策略和 `FFX_DX12_POLICY` 日志 |
| `2545a1463` | `Add NVIDIA discovery compatibility and diagnostics` | 只读 NVAPI discovery 白名单、`NVAPI_QUERY` 和私有 D3D12 interface 日志 |
| `ac7b6290a` | `Add early game attach and target capture tools` | 正式 target-control capture 命令及两个 early-attach skills |

## 兼容性策略与诊断

### FidelityFX D3D12 device policy

普通 FFX backend descriptor 中的 D3D12 device 必须保持 wrapped，使 FidelityFX 收到的 device 与游戏传入的 wrapped resources 属于同一对象体系。只有 Frame Generation swapchain descriptor 的 queue 会在调用期间解包，返回的 swapchain 随后重新包装。

日志使用带版本的标记：

```text
FFX_DX12_POLICY policy=keep-wrapped-v1 ...
FFX_DX12_POLICY result=success|failure ...
```

如果该策略引起新的兼容问题，日志中的 `descriptors`、`backendAction`、`queueAction`、返回码和恢复后的指针可以确认是否经过这条路径。不要在没有同时核对 resource 包装状态的情况下恢复“始终把 backend device 解包”为 native device 的行为。

### NVIDIA discovery policy

Streamline 启动阶段使用的一组 GPU、驱动、显示器和时钟只读查询直接转发给 NVIDIA driver。这些函数不接收 D3D object，因此可以在不开放全部 NVAPI vendor extension 的情况下使用。其他 NVAPI 查询仍按原有 wrapped、vendor-enabled 或 blocked 策略处理。

每个 ID 的第一次查询会记录：

```text
NVAPI_QUERY id=0x........ name=... route=wrapped|whitelist|vendor-enabled|blocked|driver-missing
```

Streamline 观察到的私有 D3D12 device interface `10b90151-4435-4004-9fad-19361488899a` 仍按普通未知 interface 语义转发给真实 device，只额外记录一次 HRESULT。真实 driver 返回 `E_NOINTERFACE` 是允许结果，不能单独作为 attach 失败条件。

### Target-control capture 与 early attach

正式命令为：

```text
yeecapturecmd targetcapture --target=<ID> [--frame=<N>] [--timeout=<seconds>] [--connect-only]
```

- 默认立即触发一帧，并等待 `NewCapture` 消息；默认超时 120 秒。
- `--frame` 在指定 frame 排队截帧。
- `--connect-only` 只验证 target-control 连接，不截帧。
- 连接失败返回 2；断开或等待超时返回 3。

`.agents/skills/attach-afop` 处理 AFOP 的 Steam bootstrap 和 Ubisoft launcher child，并验证 D3D12 capture-ready 日志及进程稳定性。`.agents/skills/attach-game-early` 使用 per-game JSON profile 处理其他 launcher-managed 游戏。AFOP 需要专用的 FidelityFX/NVIDIA 兼容策略，不能改用泛化 skill。

## 已验证行为

HTGame 使用 `r.FidelityFX.FSR3.UseRHI:1` 和 `FSR3SwapchainProvider` 时，能够成功创建 FSR3 context、调用 `ResizeBuffers/Present`，并避免此前的 swapchain use-after-free 崩溃。

FFX hook 对未知 SDK 入口会自动停用，不应对不匹配的函数序言进行 patch。升级 FidelityFX SDK 后如果出现入口不匹配日志，应重新确认函数序言和 descriptor 布局，而不是直接去掉保护。

2026-08-24 对最近三个提交完成了以下提交前验证：

- x64 Release `renderdoccmd.vcxproj` 构建成功，生成 `x64/Release/yeecapturecmd.exe`；
- `targetcapture` 已注册到命令列表，无效目标连接返回 2，互斥参数被拒绝；
- 两个 skill 的 PowerShell 脚本均通过语法解析，skill metadata/frontmatter 检查通过；
- AFOP `-PreflightOnly` 通过；当时 AFOP 未运行，因此没有在该次提交前验证中触发真实帧捕获。

## 分支结构

建议保持以下结构：

```text
v1.x       只包含官方 RenderDoc 代码
HTCapture  基于 v1.x 的本地 HTCapture/YeeCapture 改动
```

`origin` 指向 `https://github.com/ethanleau/renderdoc`，建议另外添加官方远程仓库：

```powershell
git remote add upstream https://github.com/baldurk/renderdoc.git
git fetch upstream
```

## 一次性分支迁移

如果当前自定义代码仍在 `v1.x`，先保留它为 HTCapture，再从基础提交创建干净的 `v1.x`：

```powershell
git branch -m v1.x HTCapture
git switch -c v1.x origin/v1.x
git push -u origin HTCapture
```

远程已经存在 `origin/v1.x`，因此初次拆分时只需要推送新创建的 `HTCapture`。以后 `v1.x` 同步了新的官方提交，再单独推送更新后的 `v1.x`。

## 后续同步官方升级

先更新干净的基础分支：

```powershell
git fetch upstream
git switch v1.x
git merge --ff-only upstream/v1.x
git push origin v1.x
```

然后把本地改动重放到新的官方基础上：

```powershell
git switch HTCapture
git rebase v1.x
```

解决冲突并完成编译、HTGame 启动和 FSR3 `ResizeBuffers/Present` 验证后，再更新远程 HTCapture 分支：

```powershell
git push --force-with-lease origin HTCapture
```

`HTCapture` 使用 rebase 后需要强制更新远程，因此应使用 `--force-with-lease`，不要使用无保护的 `--force`。每次验证通过后建议创建一个带版本号的 tag，方便回退到已知可用版本。

## 提交和验证建议

- 保持“一个功能一个提交”，不要把官方同步和本地功能修改混在同一个提交中。
- 任何改变 YeeCapture 行为、兼容路径、日志标记或维护流程的提交，都应同步更新本文的功能总览、提交记录或验证记录。
- 修改 hook 或 ABI 相关代码后，至少重新编译 `yeecapture.dll`，并检查对应的版本化诊断日志。
- 修改 `renderdoccmd` 后，至少重新编译 x64 Release 命令行目标，并验证成功路径或明确的失败返回码。
- 如果 `amd_fidelityfx_dx12.dll` 更新，重点检查入口机器码、FFX descriptor type、queue/device 包装策略和 swapchain 生命周期。
- 不要提交构建输出、捕获文件、临时日志或只由换行符造成的无意义文件变化。
