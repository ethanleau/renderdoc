/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include <unordered_map>
#include "common/formatting.h"
#include "core/core.h"
#include "core/settings.h"
#include "driver/d3d11/d3d11_hooks.h"
#include "driver/d3d12/d3d12_command_queue.h"
#include "driver/d3d12/d3d12_hooks.h"
#include "driver/dxgi/dxgi_wrapped.h"
#include "hooks/hooks.h"

#include "driver/dx/official/d3d11.h"
#include "driver/dx/official/d3d12.h"

#include "official/DXExt/AmdDxExtApi.h"
#include "official/ags/amd_ags.h"
#include "ags_wrapper.h"

RDOC_CONFIG(
    bool, AMD_ags_AllowUnknownExtensions, false,
    "Allow extensions that we may not support. This could crash or cause crashes on replay.");

#if ENABLED(RDOC_X64)

#define BIT_SPECIFIC_DLL(dll32, dll64) dll64

#else

#define BIT_SPECIFIC_DLL(dll32, dll64) dll32

#endif

void FilterDX11(AGSDX11ReturnedParams::ExtensionsSupported &extensionsSupported)
{
  if(AMD_ags_AllowUnknownExtensions())
    return;

  AGSDX11ReturnedParams::ExtensionsSupported ret = {};

  // allow all intrinsics features
  ret.intrinsics16 = extensionsSupported.intrinsics16;
  ret.intrinsics17 = extensionsSupported.intrinsics17;
  ret.intrinsics19 = extensionsSupported.intrinsics19;

  // allow trivial things
  ret.breadcrumbMarkers = extensionsSupported.breadcrumbMarkers;
  ret.appRegistration = extensionsSupported.appRegistration;

  extensionsSupported = ret;
}

void FilterDX12(AGSDX12ReturnedParams::ExtensionsSupported &extensionsSupported)
{
  if(AMD_ags_AllowUnknownExtensions())
    return;

  AGSDX12ReturnedParams::ExtensionsSupported ret = {};

  // allow all intrinsics features
  ret.intrinsics16 = extensionsSupported.intrinsics16;
  ret.intrinsics17 = extensionsSupported.intrinsics17;
  ret.intrinsics19 = extensionsSupported.intrinsics19;

  // allow custom UAV slots
  ret.UAVBindSlot = extensionsSupported.UAVBindSlot;

  // allow trivial things
  ret.userMarkers = extensionsSupported.userMarkers;
  ret.appRegistration = extensionsSupported.appRegistration;

  extensionsSupported = ret;
}

typedef HRESULT(__cdecl *PFN_AmdExtD3DCreateInterface)(IUnknown *, REFIID, void **);

// FidelityFX API descriptors all begin with this common header. Keep these definitions local so the
// compatibility hook doesn't take a dependency on a particular FidelityFX SDK version.
struct FfxApiHeader
{
  uint64_t type;
  FfxApiHeader *pNext;
};

// FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12
struct FfxCreateBackendDX12Desc
{
  FfxApiHeader header;
  ID3D12Device *device;
};

// FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_FOR_HWND_DX12
struct FfxCreateFrameGenerationSwapChainForHwndDX12Desc
{
  FfxApiHeader header;
  IDXGISwapChain4 **swapchain;
  HWND hwnd;
  DXGI_SWAP_CHAIN_DESC1 *desc;
  DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fullscreenDesc;
  IDXGIFactory *dxgiFactory;
  ID3D12CommandQueue *gameQueue;
};

typedef uint32_t(__cdecl *PFN_ffxCreateContext)(void **context, FfxApiHeader *desc,
                                                const void *allocationCallbacks);

// legacy interface before AGS 6.0. This isn't the real signature, see the hook definition for more
// information
typedef AMD_AGS_API AGSReturnCode (*PFN_agsInit)(void **);

using PFN_agsInitialize = decltype(&::agsInitialize);
using PFN_agsDriverExtensionsDX12_CreateDevice = decltype(&::agsDriverExtensionsDX12_CreateDevice);
using PFN_agsDriverExtensionsDX12_DestroyDevice = decltype(&::agsDriverExtensionsDX12_DestroyDevice);
using PFN_agsDriverExtensionsDX11_CreateDevice = decltype(&::agsDriverExtensionsDX11_CreateDevice);
using PFN_agsDriverExtensionsDX11_DestroyDevice = decltype(&::agsDriverExtensionsDX11_DestroyDevice);
using PFN_agsDriverExtensionsDX12_PushMarker = decltype(&::agsDriverExtensionsDX12_PushMarker);
using PFN_agsDriverExtensionsDX12_PopMarker = decltype(&::agsDriverExtensionsDX12_PopMarker);
using PFN_agsDriverExtensionsDX12_SetMarker = decltype(&::agsDriverExtensionsDX12_SetMarker);
using PFN_agsDriverExtensionsDX11_IASetPrimitiveTopology =
    decltype(&::agsDriverExtensionsDX11_IASetPrimitiveTopology);
using PFN_agsDriverExtensionsDX11_BeginUAVOverlap =
    decltype(&::agsDriverExtensionsDX11_BeginUAVOverlap);
using PFN_agsDriverExtensionsDX11_EndUAVOverlap = decltype(&::agsDriverExtensionsDX11_EndUAVOverlap);
using PFN_agsDriverExtensionsDX11_SetDepthBounds =
    decltype(&::agsDriverExtensionsDX11_SetDepthBounds);
using PFN_agsDriverExtensionsDX11_MultiDrawInstancedIndirect =
    decltype(&::agsDriverExtensionsDX11_MultiDrawInstancedIndirect);
using PFN_agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirect =
    decltype(&::agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirect);
using PFN_agsDriverExtensionsDX11_MultiDrawInstancedIndirectCountIndirect =
    decltype(&::agsDriverExtensionsDX11_MultiDrawInstancedIndirectCountIndirect);
using PFN_agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirectCountIndirect =
    decltype(&::agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirectCountIndirect);
using PFN_agsDriverExtensionsDX11_SetMaxAsyncCompileThreadCount =
    decltype(&::agsDriverExtensionsDX11_SetMaxAsyncCompileThreadCount);
using PFN_agsDriverExtensionsDX11_NumPendingAsyncCompileJobs =
    decltype(&::agsDriverExtensionsDX11_NumPendingAsyncCompileJobs);
using PFN_agsDriverExtensionsDX11_SetDiskShaderCacheEnabled =
    decltype(&::agsDriverExtensionsDX11_SetDiskShaderCacheEnabled);
using PFN_agsDriverExtensionsDX11_SetViewBroadcastMasks =
    decltype(&::agsDriverExtensionsDX11_SetViewBroadcastMasks);
using PFN_agsDriverExtensionsDX11_GetMaxClipRects =
    decltype(&::agsDriverExtensionsDX11_GetMaxClipRects);
using PFN_agsDriverExtensionsDX11_SetClipRects = decltype(&::agsDriverExtensionsDX11_SetClipRects);
using PFN_agsDriverExtensionsDX11_CreateBuffer = decltype(&::agsDriverExtensionsDX11_CreateBuffer);
using PFN_agsDriverExtensionsDX11_CreateTexture1D =
    decltype(&::agsDriverExtensionsDX11_CreateTexture1D);
using PFN_agsDriverExtensionsDX11_CreateTexture2D =
    decltype(&::agsDriverExtensionsDX11_CreateTexture2D);
using PFN_agsDriverExtensionsDX11_CreateTexture3D =
    decltype(&::agsDriverExtensionsDX11_CreateTexture3D);
using PFN_agsDriverExtensionsDX11_NotifyResourceEndWrites =
    decltype(&::agsDriverExtensionsDX11_NotifyResourceEndWrites);
using PFN_agsDriverExtensionsDX11_NotifyResourceBeginAllAccess =
    decltype(&::agsDriverExtensionsDX11_NotifyResourceBeginAllAccess);
using PFN_agsDriverExtensionsDX11_NotifyResourceEndAllAccess =
    decltype(&::agsDriverExtensionsDX11_NotifyResourceEndAllAccess);

class AMDHook : LibraryHook
{
public:
  void RegisterHooks()
  {
    RDCLOG("Registering AMD hooks");

    // these are hooked to prevent AMD extensions from activating and causing later crashes when not
    // replayed correctly
    LibraryHooks::RegisterLibraryHook(BIT_SPECIFIC_DLL("atidxx32.dll", "atidxx64.dll"), NULL);
    AmdCreate11.Register(BIT_SPECIFIC_DLL("atidxx32.dll", "atidxx64.dll"), "AmdDxExtCreate11",
                         AmdCreate11_hook);

    LibraryHooks::RegisterLibraryHook(BIT_SPECIFIC_DLL("amdxc32.dll", "amdxc64.dll"), NULL);
    AmdExtD3DCreateInterface.Register(BIT_SPECIFIC_DLL("amdxc32.dll", "amdxc64.dll"),
                                      "AmdExtD3DCreateInterface", AmdExtD3DCreateInterface_hook);

    const char *ags_dll = BIT_SPECIFIC_DLL("amd_ags_x86.dll", "amd_ags_x64.dll");
    LibraryHooks::RegisterLibraryHook(ags_dll, NULL);

    // Some titles load the FidelityFX API only after FSR frame generation is selected.
    const char *ffx_dll = "amd_fidelityfx_dx12.dll";
    LibraryHooks::RegisterLibraryHook(ffx_dll, FidelityFXLoaded);
    ffxCreateContext.Register(ffx_dll, "ffxCreateContext", ffxCreateContext_hook);

    // FidelityFX can be delay-loaded without passing through an import that RenderDoc patched.
    // Watch briefly for that case and install the same guarded export hook used by the load
    // callback. This thread only observes module state and exits as soon as the DLL appears.
    Threading::ThreadHandle watcher = Threading::CreateThread([]() {
      for(uint32_t i = 0; i < 300; i++)
      {
        if(Process::IsModuleLoaded("amd_fidelityfx_dx12.dll"))
        {
          void *module = Process::LoadModule("amd_fidelityfx_dx12.dll");
          FidelityFXLoaded(module, "amd_fidelityfx_dx12.dll");
          return;
        }

        Threading::Sleep(100);
      }
    });
    Threading::DetachThread(watcher);

    // allowed through without interception:
    // agsDeInitialize, agsSetDisplayMode, agsDriverExtensionsDX11_WriteBreadcrumb
    agsInit.Register(ags_dll, "agsInit", agsInit_hook);
    agsInitialize.Register(ags_dll, "agsInitialize", agsInitialize_hook);
    agsDriverExtensionsDX12_CreateDevice.Register(ags_dll, "agsDriverExtensionsDX12_CreateDevice",
                                                  agsDriverExtensionsDX12_CreateDevice_hook);
    agsDriverExtensionsDX12_DestroyDevice.Register(ags_dll, "agsDriverExtensionsDX12_DestroyDevice",
                                                   agsDriverExtensionsDX12_DestroyDevice_hook);
    agsDriverExtensionsDX11_CreateDevice.Register(ags_dll, "agsDriverExtensionsDX11_CreateDevice",
                                                  agsDriverExtensionsDX11_CreateDevice_hook);
    agsDriverExtensionsDX11_DestroyDevice.Register(ags_dll, "agsDriverExtensionsDX11_DestroyDevice",
                                                   agsDriverExtensionsDX11_DestroyDevice_hook);
    agsDriverExtensionsDX12_CreateDevice.Register(ags_dll, "agsDriverExtensionsDX12_PushMarker",
                                                  agsDriverExtensionsDX12_PushMarker_hook);
    agsDriverExtensionsDX12_CreateDevice.Register(ags_dll, "agsDriverExtensionsDX12_PopMarker",
                                                  agsDriverExtensionsDX12_PopMarker_hook);
    agsDriverExtensionsDX12_CreateDevice.Register(ags_dll, "agsDriverExtensionsDX12_SetMarker",
                                                  agsDriverExtensionsDX12_SetMarker_hook);
    agsDriverExtensionsDX11_IASetPrimitiveTopology.Register(
        ags_dll, "agsDriverExtensionsDX11_IASetPrimitiveTopology",
        agsDriverExtensionsDX11_IASetPrimitiveTopology_hook);
    agsDriverExtensionsDX11_BeginUAVOverlap.Register(ags_dll,
                                                     "agsDriverExtensionsDX11_BeginUAVOverlap",
                                                     agsDriverExtensionsDX11_BeginUAVOverlap_hook);
    agsDriverExtensionsDX11_EndUAVOverlap.Register(ags_dll, "agsDriverExtensionsDX11_EndUAVOverlap",
                                                   agsDriverExtensionsDX11_EndUAVOverlap_hook);
    agsDriverExtensionsDX11_SetDepthBounds.Register(ags_dll,
                                                    "agsDriverExtensionsDX11_SetDepthBounds",
                                                    agsDriverExtensionsDX11_SetDepthBounds_hook);
    agsDriverExtensionsDX11_MultiDrawInstancedIndirect.Register(
        ags_dll, "agsDriverExtensionsDX11_MultiDrawInstancedIndirect",
        agsDriverExtensionsDX11_MultiDrawInstancedIndirect_hook);
    agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirect.Register(
        ags_dll, "agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirect",
        agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirect_hook);
    agsDriverExtensionsDX11_MultiDrawInstancedIndirectCountIndirect.Register(
        ags_dll, "agsDriverExtensionsDX11_MultiDrawInstancedIndirectCountIndirect",
        agsDriverExtensionsDX11_MultiDrawInstancedIndirectCountIndirect_hook);
    agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirectCountIndirect.Register(
        ags_dll, "agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirectCountIndirect",
        agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirectCountIndirect_hook);
    agsDriverExtensionsDX11_SetMaxAsyncCompileThreadCount.Register(
        ags_dll, "agsDriverExtensionsDX11_SetMaxAsyncCompileThreadCount",
        agsDriverExtensionsDX11_SetMaxAsyncCompileThreadCount_hook);
    agsDriverExtensionsDX11_NumPendingAsyncCompileJobs.Register(
        ags_dll, "agsDriverExtensionsDX11_NumPendingAsyncCompileJobs",
        agsDriverExtensionsDX11_NumPendingAsyncCompileJobs_hook);
    agsDriverExtensionsDX11_SetDiskShaderCacheEnabled.Register(
        ags_dll, "agsDriverExtensionsDX11_SetDiskShaderCacheEnabled",
        agsDriverExtensionsDX11_SetDiskShaderCacheEnabled_hook);
    agsDriverExtensionsDX11_SetViewBroadcastMasks.Register(
        ags_dll, "agsDriverExtensionsDX11_SetViewBroadcastMasks",
        agsDriverExtensionsDX11_SetViewBroadcastMasks_hook);
    agsDriverExtensionsDX11_GetMaxClipRects.Register(ags_dll,
                                                     "agsDriverExtensionsDX11_GetMaxClipRects",
                                                     agsDriverExtensionsDX11_GetMaxClipRects_hook);
    agsDriverExtensionsDX11_SetClipRects.Register(ags_dll, "agsDriverExtensionsDX11_SetClipRects",
                                                  agsDriverExtensionsDX11_SetClipRects_hook);
    agsDriverExtensionsDX11_CreateBuffer.Register(ags_dll, "agsDriverExtensionsDX11_CreateBuffer",
                                                  agsDriverExtensionsDX11_CreateBuffer_hook);
    agsDriverExtensionsDX11_CreateTexture1D.Register(ags_dll,
                                                     "agsDriverExtensionsDX11_CreateTexture1D",
                                                     agsDriverExtensionsDX11_CreateTexture1D_hook);
    agsDriverExtensionsDX11_CreateTexture2D.Register(ags_dll,
                                                     "agsDriverExtensionsDX11_CreateTexture2D",
                                                     agsDriverExtensionsDX11_CreateTexture2D_hook);
    agsDriverExtensionsDX11_CreateTexture3D.Register(ags_dll,
                                                     "agsDriverExtensionsDX11_CreateTexture3D",
                                                     agsDriverExtensionsDX11_CreateTexture3D_hook);
    agsDriverExtensionsDX11_NotifyResourceEndWrites.Register(
        ags_dll, "agsDriverExtensionsDX11_NotifyResourceEndWrites",
        agsDriverExtensionsDX11_NotifyResourceEndWrites_hook);
    agsDriverExtensionsDX11_NotifyResourceBeginAllAccess.Register(
        ags_dll, "agsDriverExtensionsDX11_NotifyResourceBeginAllAccess",
        agsDriverExtensionsDX11_NotifyResourceBeginAllAccess_hook);
    agsDriverExtensionsDX11_NotifyResourceEndAllAccess.Register(
        ags_dll, "agsDriverExtensionsDX11_NotifyResourceEndAllAccess",
        agsDriverExtensionsDX11_NotifyResourceEndAllAccess_hook);
  }

private:
  static AMDHook amdhooks;
  static Threading::CriticalSection ffxHookLock;
  static bool ffxInlineHookInstalled;

  static bool InstallFidelityFXInlineHook(void *target)
  {
    SCOPED_LOCK(ffxHookLock);

    if(ffxInlineHookInstalled)
      return true;

    if(target == NULL)
      return false;

#if ENABLED(RDOC_WIN32) && ENABLED(RDOC_X64)
    // amd_fidelityfx_dx12.dll 1.x starts ffxCreateContext with four position-independent
    // instructions totalling 15 bytes. Verify every byte before installing the hook so an SDK
    // update can only disable this compatibility path, never patch an unknown prologue.
    static const byte expectedPrologue[15] = {
        0x48, 0x89, 0x5c, 0x24, 0x18, 0x48, 0x89, 0x6c,
        0x24, 0x20, 0x57, 0x48, 0x83, 0xec, 0x20,
    };

    if(memcmp(target, expectedPrologue, sizeof(expectedPrologue)) != 0)
    {
      RDCERR("FidelityFX ffxCreateContext has an unsupported prologue; inline hook disabled");
      return false;
    }

    const size_t patchSize = sizeof(expectedPrologue);
    const size_t absoluteJumpSize = 14;
    byte *trampoline = (byte *)VirtualAlloc(NULL, patchSize + absoluteJumpSize,
                                            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if(trampoline == NULL)
    {
      RDCERR("Couldn't allocate FidelityFX ffxCreateContext trampoline");
      return false;
    }

    memcpy(trampoline, target, patchSize);

    byte jumpBack[absoluteJumpSize] = {0xff, 0x25, 0, 0, 0, 0};
    *(void **)(jumpBack + 6) = (byte *)target + patchSize;
    memcpy(trampoline + patchSize, jumpBack, absoluteJumpSize);

    byte hookJump[absoluteJumpSize] = {0xff, 0x25, 0, 0, 0, 0};
    *(void **)(hookJump + 6) = (void *)&ffxCreateContext_hook;

    DWORD oldProtect = 0;
    if(!VirtualProtect(target, patchSize, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
      RDCERR("Couldn't make FidelityFX ffxCreateContext writable");
      VirtualFree(trampoline, 0, MEM_RELEASE);
      return false;
    }

    amdhooks.ffxCreateContext.SetFuncPtr(trampoline);
    memcpy(target, hookJump, absoluteJumpSize);
    *((byte *)target + absoluteJumpSize) = 0x90;
    FlushInstructionCache(GetCurrentProcess(), target, patchSize);

    DWORD dummy = 0;
    VirtualProtect(target, patchSize, oldProtect, &dummy);

    ffxInlineHookInstalled = true;
    RDCLOG("Installed guarded FidelityFX ffxCreateContext inline hook target=%p trampoline=%p",
           target, trampoline);
    return true;
#else
    return false;
#endif
  }

  static void FidelityFXLoaded(void *module, const char *)
  {
    void *target = Process::GetFunctionAddress(module, "ffxCreateContext");
    InstallFidelityFXInlineHook(target);
  }

  HookedFunction<PFN_ffxCreateContext> ffxCreateContext;

  static uint32_t __cdecl ffxCreateContext_hook(void **context, FfxApiHeader *desc,
                                                const void *allocationCallbacks)
  {
    FfxCreateFrameGenerationSwapChainForHwndDX12Desc *swapchainDesc = NULL;
    FfxCreateBackendDX12Desc *backendDesc = NULL;

    size_t count = 0;
    for(FfxApiHeader *header = desc; header && count < 16; header = header->pNext, count++)
    {
      if(header->type == 0x30006)
        swapchainDesc = (FfxCreateFrameGenerationSwapChainForHwndDX12Desc *)header;
      else if(header->type == 0x2)
        backendDesc = (FfxCreateBackendDX12Desc *)header;
    }

    ID3D12CommandQueue *wrappedQueue = swapchainDesc ? swapchainDesc->gameQueue : NULL;
    ID3D12Device *wrappedBackendDevice = backendDesc ? backendDesc->device : NULL;
    ID3DDevice *presentationDevice = GetD3D12DeviceIfAlloc(wrappedQueue);
    ID3DDevice *backendDevice = GetD3D12DeviceIfAlloc(wrappedBackendDevice);

    if(swapchainDesc && presentationDevice)
      swapchainDesc->gameQueue =
          (ID3D12CommandQueue *)presentationDevice->GetRealIUnknown();
    if(backendDesc && backendDevice)
      backendDesc->device = (ID3D12Device *)backendDevice->GetRealIUnknown();

    PFN_ffxCreateContext real = amdhooks.ffxCreateContext();
    if(real == NULL)
    {
      RDCERR("FidelityFX ffxCreateContext hook has no real function pointer");
      return 1;
    }

    uint32_t ret = real(context, desc, allocationCallbacks);

    if(swapchainDesc)
      swapchainDesc->gameQueue = wrappedQueue;
    if(backendDesc)
      backendDesc->device = wrappedBackendDevice;

    if(ret == 0 && swapchainDesc && swapchainDesc->swapchain && *swapchainDesc->swapchain &&
       presentationDevice)
    {
      IDXGISwapChain4 *realSwapchain = *swapchainDesc->swapchain;
      WrappedIDXGISwapChain4 *wrappedSwapchain =
          new WrappedIDXGISwapChain4(realSwapchain, swapchainDesc->hwnd, presentationDevice);

      // The FFX frame-generation context treats the swapchain returned through this descriptor as
      // a borrowed interface. Its caller releases the returned pointer after storing it, but FFX
      // continues to issue ResizeBuffers/Present calls through that pointer. A normal RenderDoc
      // swapchain starts with one caller-owned reference, so that release would destroy the wrapper
      // and leave FFX with a dangling pointer. Keep one compatibility reference for the lifetime of
      // the process; there is only one frame-generation swapchain and no destruction notification
      // is available here to release it safely.
      wrappedSwapchain->AddRef();
      *swapchainDesc->swapchain = (IDXGISwapChain4 *)wrappedSwapchain;
      RDCLOG("Wrapped FidelityFX frame-generation swapchain");
    }

    return ret;
  }

  HookedFunction<PFN_agsInit> agsInit;
  HookedFunction<PFN_agsInitialize> agsInitialize;

  HookedFunction<PFN_agsDriverExtensionsDX12_CreateDevice> agsDriverExtensionsDX12_CreateDevice;
  HookedFunction<PFN_agsDriverExtensionsDX12_DestroyDevice> agsDriverExtensionsDX12_DestroyDevice;
  HookedFunction<PFN_agsDriverExtensionsDX11_CreateDevice> agsDriverExtensionsDX11_CreateDevice;
  HookedFunction<PFN_agsDriverExtensionsDX11_DestroyDevice> agsDriverExtensionsDX11_DestroyDevice;

  // this hook is a little special. agsInit() has different signatures - older versions are
  // agsInit( AGSContext** context, AGSGPUInfo* info );
  // and newer versions are:
  // agsInit( AGSContext** context, const AGSConfiguration* config, AGSGPUInfo* gpuInfo );
  //
  // Rather than fixing a hook or making the hook conditional depending on DLL version (which may
  // not be reliably detectable), we hook only enough to write the first parameter which is the same
  // and return a failure code.
  // Fortunately since this is the cdecl ABI, unused parameters don't affect anything because it's
  // caller saved.
  static AGSReturnCode __cdecl agsInit_hook(void **context)
  {
    if(context)
      *context = NULL;

    RDCLOG("Blocked attempt to initialise old version of AGS. Please update to AGS 6.0 or newer!");

    // the meaning of 1 has changed over time but all non-zero codes are errors so we can safely
    // return this
    return AGSReturnCode(1);
  }

  static AGSReturnCode __cdecl agsInitialize_hook(int agsVersion, const AGSConfiguration *config,
                                                  AGSContext **context, AGSGPUInfo *gpuInfo)
  {
    RDCLOG("Initialising AGS, version %d.%d.%d", agsVersion >> 22, (agsVersion >> 12) & 0x003FF,
           agsVersion & 0xfff);

    // for now don't do anything with the version. If we need to in future we can block based on the
    // version or add any compatibility shims needed.

    return amdhooks.agsInitialize()(agsVersion, config, context, gpuInfo);
  }

  static AGSReturnCode __cdecl agsDriverExtensionsDX12_CreateDevice_hook(
      AGSContext *context, const AGSDX12DeviceCreationParams *creationParams,
      const AGSDX12ExtensionParams *extensionParams, AGSDX12ReturnedParams *returnedParams)
  {
    AGSReturnCode ret = AGS_SUCCESS;
    ID3D12Device *dev = NULL;
    CreateD3D12_Internal(
        [&](IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void **ppDevice) {
          AGSDX12DeviceCreationParams params;
          params.pAdapter = (IDXGIAdapter *)pAdapter;
          params.FeatureLevel = MinimumFeatureLevel;
          params.iid = creationParams->iid;
          ret = amdhooks.agsDriverExtensionsDX12_CreateDevice()(context, &params, extensionParams,
                                                                returnedParams);

          if(ret != AGS_SUCCESS)
            return E_FAIL;

          FilterDX12(returnedParams->extensionsSupported);

          // AGS effectively owns the created refcount which will be paired with a DestroyDevice
          // call. However we also want our wrapper device to own the created refcount (and it will
          // release it on shutdown).
          // To solve this we add an extra refcount for the created real device. When returning from
          // this function the real device will have two refs - one that will be owned by our
          // wrapper, and one by ags. In destroydevice we'll let ags release its, and we'll also
          // de-refcount our device.
          returnedParams->pDevice->AddRef();

          if(ppDevice)
            *ppDevice = returnedParams->pDevice;

          return S_OK;
        },
        NULL, creationParams->pAdapter, creationParams->FeatureLevel, __uuidof(ID3D12Device),
        (void **)&dev);
    returnedParams->pDevice = dev;

    if(returnedParams->pDevice && extensionParams)
    {
      IAGSD3DDevice *agsDev = NULL;
      returnedParams->pDevice->QueryInterface(__uuidof(IAGSD3DDevice), (void **)&agsDev);

      if(extensionParams->uavSlot == 0)
        agsDev->SetShaderExtUAV(AGS_DX12_SHADER_INSTRINSICS_SPACE_ID, 0);
      else
        agsDev->SetShaderExtUAV(0, extensionParams->uavSlot);
    }

    return ret;
  }

  static AGSReturnCode __cdecl agsDriverExtensionsDX12_DestroyDevice_hook(
      AGSContext *context, ID3D12Device *device, unsigned int *deviceReferences)
  {
    IAGSD3DDevice *agsDev = NULL;
    HRESULT hr = device->QueryInterface(__uuidof(IAGSD3DDevice), (void **)&agsDev);

    if(SUCCEEDED(hr))
    {
      // destroy AGS which releases its ref on the real device
      unsigned int dummy = 0;
      amdhooks.agsDriverExtensionsDX12_DestroyDevice()(context, (ID3D12Device *)agsDev->GetReal(),
                                                       &dummy);

      // release the wrapped device to match, since the application should not be releasing the
      // implicit ref on it because it thinks AGS owns it. If there are no other refs on it by the
      // application (say the application just did agsCreateDevice and then agsDestroyDevice) this
      // will destroy it, and release the wrapper's refcount on the real device.
      ULONG refs = device->Release();
      if(deviceReferences)
        *deviceReferences = refs;

      return AGS_SUCCESS;
    }
    return AGS_INVALID_ARGS;
  }

  static AGSReturnCode __cdecl agsDriverExtensionsDX11_CreateDevice_hook(
      AGSContext *context, const AGSDX11DeviceCreationParams *creationParams,
      const AGSDX11ExtensionParams *extensionParams, AGSDX11ReturnedParams *returnedParams)
  {
    AGSReturnCode ret = AGS_SUCCESS;
    CreateD3D11_Internal(
        [&](IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
            CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
            CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain,
            ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel,
            ID3D11DeviceContext **ppImmediateContext) {
          AGSDX11DeviceCreationParams params;
          params.pAdapter = pAdapter;
          params.DriverType = DriverType;
          params.Software = Software;
          params.Flags = Flags;
          params.pFeatureLevels = pFeatureLevels;
          params.FeatureLevels = FeatureLevels;
          params.SDKVersion = SDKVersion;
          params.pSwapChainDesc = pSwapChainDesc;
          ret = amdhooks.agsDriverExtensionsDX11_CreateDevice()(context, &params, extensionParams,
                                                                returnedParams);

          if(ret != AGS_SUCCESS)
            return E_FAIL;

          FilterDX11(returnedParams->extensionsSupported);

          // See above in DX12 create device for the logic for this AddRef. The only difference here
          // is that AGS owns the ref on the device and immediate context, so we do both.
          returnedParams->pDevice->AddRef();
          returnedParams->pImmediateContext->AddRef();

          if(ppDevice)
            *ppDevice = returnedParams->pDevice;
          if(ppImmediateContext)
            *ppImmediateContext = returnedParams->pImmediateContext;
          if(ppSwapChain)
            *ppSwapChain = returnedParams->pSwapChain;

          return S_OK;
        },
        creationParams->pAdapter, creationParams->DriverType, creationParams->Software,
        creationParams->Flags, creationParams->pFeatureLevels, creationParams->FeatureLevels,
        creationParams->SDKVersion, creationParams->pSwapChainDesc, &returnedParams->pSwapChain,
        &returnedParams->pDevice, &returnedParams->featureLevel, &returnedParams->pImmediateContext);

    if(returnedParams->pDevice && extensionParams)
    {
      IAGSD3DDevice *agsDev = NULL;
      returnedParams->pDevice->QueryInterface(__uuidof(IAGSD3DDevice), (void **)&agsDev);

      agsDev->SetShaderExtUAV(0, extensionParams->uavSlot);
    }

    return ret;
  }

  static AGSReturnCode __cdecl agsDriverExtensionsDX11_DestroyDevice_hook(
      AGSContext *context, ID3D11Device *device, unsigned int *deviceReferences,
      ID3D11DeviceContext *immediateContext, unsigned int *immediateContextReferences)
  {
    IAGSD3DDevice *agsDev = NULL;
    HRESULT hr = device->QueryInterface(__uuidof(IAGSD3DDevice), (void **)&agsDev);

    if(SUCCEEDED(hr))
    {
      // Again see above in DX12 for how we manage lifecycles
      ID3D11Device *realDev = (ID3D11Device *)agsDev->GetReal();

      // obtain the real immediate context without changing its refcount
      ID3D11DeviceContext *realCtx = NULL;
      realDev->GetImmediateContext(&realCtx);
      realCtx->Release();

      unsigned int dummy = 0;
      amdhooks.agsDriverExtensionsDX11_DestroyDevice()(context, realDev, &dummy, realCtx, &dummy);

      ULONG refs = immediateContext->Release();
      if(immediateContextReferences)
        *immediateContextReferences = refs;

      refs = device->Release();
      if(deviceReferences)
        *deviceReferences = refs;

      return AGS_SUCCESS;
    }
    return AGS_INVALID_ARGS;
  }

  // AGS calls these functions internally, so we allow them on the *real* device, but don't allow
  // application access.
  HookedFunction<PFNAmdDxExtCreate11> AmdCreate11;
  HookedFunction<PFN_AmdExtD3DCreateInterface> AmdExtD3DCreateInterface;

  static HRESULT __cdecl AmdCreate11_hook(ID3D11Device *pDevice, IAmdDxExt **ppExt)
  {
    if(GetD3D11DeviceIfAlloc(pDevice))
    {
      RDCLOG("Attempt to create AMD extension interface via AmdDxExtCreate11 was blocked.");

      if(ppExt)
        *ppExt = NULL;

      return E_FAIL;
    }

    return amdhooks.AmdCreate11()(pDevice, ppExt);
  }

  static HRESULT __cdecl AmdExtD3DCreateInterface_hook(IUnknown *pDevice, REFIID iid, void **ppvObject)
  {
    if(GetD3D11DeviceIfAlloc(pDevice) || GetD3D12DeviceIfAlloc(pDevice))
    {
      RDCLOG("Attempt to create AMD extension interface via AmdExtD3DCreateInterface was blocked.");

      if(ppvObject)
        *ppvObject = NULL;

      return E_FAIL;
    }

    return amdhooks.AmdExtD3DCreateInterface()(pDevice, iid, ppvObject);
  }

  // remaining hooks all return AGS_EXTENSION_NOT_SUPPORTED and don't forward, to disable that
  // functionality

  HookedFunction<PFN_agsDriverExtensionsDX12_PushMarker> agsDriverExtensionsDX12_PushMarker;
  HookedFunction<PFN_agsDriverExtensionsDX12_PopMarker> agsDriverExtensionsDX12_PopMarker;
  HookedFunction<PFN_agsDriverExtensionsDX12_SetMarker> agsDriverExtensionsDX12_SetMarker;
  HookedFunction<PFN_agsDriverExtensionsDX11_IASetPrimitiveTopology>
      agsDriverExtensionsDX11_IASetPrimitiveTopology;
  HookedFunction<PFN_agsDriverExtensionsDX11_BeginUAVOverlap> agsDriverExtensionsDX11_BeginUAVOverlap;
  HookedFunction<PFN_agsDriverExtensionsDX11_EndUAVOverlap> agsDriverExtensionsDX11_EndUAVOverlap;
  HookedFunction<PFN_agsDriverExtensionsDX11_SetDepthBounds> agsDriverExtensionsDX11_SetDepthBounds;
  HookedFunction<PFN_agsDriverExtensionsDX11_MultiDrawInstancedIndirect>
      agsDriverExtensionsDX11_MultiDrawInstancedIndirect;
  HookedFunction<PFN_agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirect>
      agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirect;
  HookedFunction<PFN_agsDriverExtensionsDX11_MultiDrawInstancedIndirectCountIndirect>
      agsDriverExtensionsDX11_MultiDrawInstancedIndirectCountIndirect;
  HookedFunction<PFN_agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirectCountIndirect>
      agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirectCountIndirect;
  HookedFunction<PFN_agsDriverExtensionsDX11_SetMaxAsyncCompileThreadCount>
      agsDriverExtensionsDX11_SetMaxAsyncCompileThreadCount;
  HookedFunction<PFN_agsDriverExtensionsDX11_NumPendingAsyncCompileJobs>
      agsDriverExtensionsDX11_NumPendingAsyncCompileJobs;
  HookedFunction<PFN_agsDriverExtensionsDX11_SetDiskShaderCacheEnabled>
      agsDriverExtensionsDX11_SetDiskShaderCacheEnabled;
  HookedFunction<PFN_agsDriverExtensionsDX11_SetViewBroadcastMasks>
      agsDriverExtensionsDX11_SetViewBroadcastMasks;
  HookedFunction<PFN_agsDriverExtensionsDX11_GetMaxClipRects> agsDriverExtensionsDX11_GetMaxClipRects;
  HookedFunction<PFN_agsDriverExtensionsDX11_SetClipRects> agsDriverExtensionsDX11_SetClipRects;
  HookedFunction<PFN_agsDriverExtensionsDX11_CreateBuffer> agsDriverExtensionsDX11_CreateBuffer;
  HookedFunction<PFN_agsDriverExtensionsDX11_CreateTexture1D> agsDriverExtensionsDX11_CreateTexture1D;
  HookedFunction<PFN_agsDriverExtensionsDX11_CreateTexture2D> agsDriverExtensionsDX11_CreateTexture2D;
  HookedFunction<PFN_agsDriverExtensionsDX11_CreateTexture3D> agsDriverExtensionsDX11_CreateTexture3D;
  HookedFunction<PFN_agsDriverExtensionsDX11_NotifyResourceEndWrites>
      agsDriverExtensionsDX11_NotifyResourceEndWrites;
  HookedFunction<PFN_agsDriverExtensionsDX11_NotifyResourceBeginAllAccess>
      agsDriverExtensionsDX11_NotifyResourceBeginAllAccess;
  HookedFunction<PFN_agsDriverExtensionsDX11_NotifyResourceEndAllAccess>
      agsDriverExtensionsDX11_NotifyResourceEndAllAccess;

  static AGSReturnCode __cdecl agsDriverExtensionsDX12_PushMarker_hook(
      AGSContext *context, ID3D12GraphicsCommandList *commandList, const char *data)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX12_PopMarker_hook(
      AGSContext *context, ID3D12GraphicsCommandList *commandList)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX12_SetMarker_hook(
      AGSContext *context, ID3D12GraphicsCommandList *commandList, const char *data)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_IASetPrimitiveTopology_hook(
      AGSContext *context, enum D3D_PRIMITIVE_TOPOLOGY topology)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_BeginUAVOverlap_hook(
      AGSContext *context, ID3D11DeviceContext *dxContext)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_EndUAVOverlap_hook(AGSContext *context,
                                                                          ID3D11DeviceContext *dxContext)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_SetDepthBounds_hook(
      AGSContext *context, ID3D11DeviceContext *dxContext, bool enabled, float minDepth,
      float maxDepth)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_MultiDrawInstancedIndirect_hook(
      AGSContext *context, ID3D11DeviceContext *dxContext, unsigned int drawCount,
      ID3D11Buffer *pBufferForArgs, unsigned int alignedByteOffsetForArgs,
      unsigned int byteStrideForArgs)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirect_hook(
      AGSContext *context, ID3D11DeviceContext *dxContext, unsigned int drawCount,
      ID3D11Buffer *pBufferForArgs, unsigned int alignedByteOffsetForArgs,
      unsigned int byteStrideForArgs)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_MultiDrawInstancedIndirectCountIndirect_hook(
      AGSContext *context, ID3D11DeviceContext *dxContext, ID3D11Buffer *pBufferForDrawCount,
      unsigned int alignedByteOffsetForDrawCount, ID3D11Buffer *pBufferForArgs,
      unsigned int alignedByteOffsetForArgs, unsigned int byteStrideForArgs)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirectCountIndirect_hook(
      AGSContext *context, ID3D11DeviceContext *dxContext, ID3D11Buffer *pBufferForDrawCount,
      unsigned int alignedByteOffsetForDrawCount, ID3D11Buffer *pBufferForArgs,
      unsigned int alignedByteOffsetForArgs, unsigned int byteStrideForArgs)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_SetMaxAsyncCompileThreadCount_hook(
      AGSContext *context, unsigned int numberOfThreads)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_NumPendingAsyncCompileJobs_hook(
      AGSContext *context, unsigned int *numberOfJobs)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_SetDiskShaderCacheEnabled_hook(
      AGSContext *context, int enable)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_SetViewBroadcastMasks_hook(
      AGSContext *context, unsigned long long vpMask, unsigned long long rtSliceMask,
      int vpMaskPerRtSliceEnabled)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_GetMaxClipRects_hook(AGSContext *context,
                                                                            unsigned int *maxRectCount)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_SetClipRects_hook(AGSContext *context,
                                                                         unsigned int clipRectCount,
                                                                         const AGSClipRect *clipRects)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_CreateBuffer_hook(
      AGSContext *context, const D3D11_BUFFER_DESC *desc, const D3D11_SUBRESOURCE_DATA *initialData,
      ID3D11Buffer **buffer, AGSAfrTransferType transferType, AGSAfrTransferEngine transferEngine)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_CreateTexture1D_hook(
      AGSContext *context, const D3D11_TEXTURE1D_DESC *desc,
      const D3D11_SUBRESOURCE_DATA *initialData, ID3D11Texture1D **texture1D,
      AGSAfrTransferType transferType, AGSAfrTransferEngine transferEngine)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_CreateTexture2D_hook(
      AGSContext *context, const D3D11_TEXTURE2D_DESC *desc,
      const D3D11_SUBRESOURCE_DATA *initialData, ID3D11Texture2D **texture2D,
      AGSAfrTransferType transferType, AGSAfrTransferEngine transferEngine)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_CreateTexture3D_hook(
      AGSContext *context, const D3D11_TEXTURE3D_DESC *desc,
      const D3D11_SUBRESOURCE_DATA *initialData, ID3D11Texture3D **texture3D,
      AGSAfrTransferType transferType, AGSAfrTransferEngine transferEngine)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_NotifyResourceEndWrites_hook(
      AGSContext *context, ID3D11Resource *resource, const D3D11_RECT *transferRegions,
      const unsigned int *subresourceArray, unsigned int numSubresources)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_NotifyResourceBeginAllAccess_hook(
      AGSContext *context, ID3D11Resource *resource)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
  static AGSReturnCode __cdecl agsDriverExtensionsDX11_NotifyResourceEndAllAccess_hook(
      AGSContext *context, ID3D11Resource *resource)
  {
    return AGS_EXTENSION_NOT_SUPPORTED;
  }
};

AMDHook AMDHook::amdhooks;
Threading::CriticalSection AMDHook::ffxHookLock;
bool AMDHook::ffxInlineHookInstalled = false;
