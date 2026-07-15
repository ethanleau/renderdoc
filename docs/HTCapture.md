# HTCapture 本地改动与升级维护

本文记录 `ethanleau/renderdoc` 中用于 HTCapture/YeeCapture 的本地改动，以及后续同步 RenderDoc 官方代码时的维护流程。

## 当前本地改动

当前 HTCapture 分支相对于基础 RenderDoc `v1.x` 包含以下提交：

1. 增加 YeeCapture Windows 构建目标和相关构建配置。
2. 增加 YeeCapture OpenGL 支持。
3. 使用挂起进程和 `SetThreadContext` 完成注入，并通过 `ShellExecuteExW` hook 跟踪启动的子进程（例如 HTGame）。
4. 增加 FidelityFX/FSR3 兼容路径：
   - 扫描 delay-IAT，使延迟加载的图形 API 也能被发现；
   - 监视晚加载的 `amd_fidelityfx_dx12.dll`；
   - 仅在 `ffxCreateContext` 的 15 字节入口机器码完全匹配时安装 x64 inline hook；
   - 调用 FFX 前解包 RenderDoc 的 D3D12 queue/device，返回后恢复原始 descriptor；
   - 将 FFX Frame Generation swapchain 包装为 RenderDoc swapchain；
   - 为 FFX 返回的借用式 swapchain 保留兼容引用，避免调用方释放后继续使用造成崩溃。

## 已验证行为

HTGame 使用 `r.FidelityFX.FSR3.UseRHI:1` 和 `FSR3SwapchainProvider` 时，能够成功创建 FSR3 context、调用 `ResizeBuffers/Present`，并避免此前的 swapchain use-after-free 崩溃。

FFX hook 对未知 SDK 入口会自动停用，不应对不匹配的函数序言进行 patch。升级 FidelityFX SDK 后如果出现入口不匹配日志，应重新确认函数序言和 descriptor 布局，而不是直接去掉保护。

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
git branch -m v1.x HTCcapture
git switch -c v1.x origin/v1.x
git push -u origin HTCcapture
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
git switch HTCcapture
git rebase v1.x
```

解决冲突并完成编译、HTGame 启动和 FSR3 `ResizeBuffers/Present` 验证后，再更新远程 HTCapture 分支：

```powershell
git push --force-with-lease origin HTCcapture
```

`HTCapture` 使用 rebase 后需要强制更新远程，因此应使用 `--force-with-lease`，不要使用无保护的 `--force`。每次验证通过后建议创建一个带版本号的 tag，方便回退到已知可用版本。

## 提交和验证建议

- 保持“一个功能一个提交”，不要把官方同步和本地功能修改混在同一个提交中。
- 修改 hook 或 ABI 相关代码后，至少重新编译 `yeecapture.dll`，并检查 DebugView 日志。
- 如果 `amd_fidelityfx_dx12.dll` 更新，重点检查入口机器码、FFX descriptor type、queue/device 解包和 swapchain 生命周期。
- 不要提交构建输出、临时日志或只由换行符造成的无意义文件变化。
