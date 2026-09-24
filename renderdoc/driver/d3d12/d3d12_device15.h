// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Device15 declarations from microsoft/DirectX-Headers include/directx/d3d12.h.
// Keep this small compatibility header until the bundled SDK is updated.
#pragma once

#ifndef __ID3D12Device15_INTERFACE_DEFINED__
#define __ID3D12Device15_INTERFACE_DEFINED__

typedef enum D3D12_TRIM_NOTIFICATION_FLAGS
{
  D3D12_TRIM_NOTIFICATION_FLAG_NONE = 0,
  D3D12_TRIM_NOTIFICATION_FLAG_PERIODIC_TRIM = 0x1,
  D3D12_TRIM_NOTIFICATION_FLAG_RESTART_PERIODIC_TRIM = 0x2,
  D3D12_TRIM_NOTIFICATION_FLAG_TRIM_TO_BUDGET = 0x4
} D3D12_TRIM_NOTIFICATION_FLAGS;

DEFINE_ENUM_FLAG_OPERATORS(D3D12_TRIM_NOTIFICATION_FLAGS)
typedef struct D3D12_TRIM_NOTIFICATION
{
  void *pContext;
  D3D12_TRIM_NOTIFICATION_FLAGS Flags;
  UINT64 NumBytesToTrim;
} D3D12_TRIM_NOTIFICATION;

typedef void(__stdcall *D3D12_PFN_TRIM_NOTIFICATION_CALLBACK)(
    _In_ const D3D12_TRIM_NOTIFICATION *__MIDL____MIDL_itf_d3d12_0000_00640000);

typedef struct D3D12_REGISTER_TRIM_NOTIFICATION
{
  D3D12_PFN_TRIM_NOTIFICATION_CALLBACK pfnCallback;
  void *pContext;
  DWORD CallbackCookie;
} D3D12_REGISTER_TRIM_NOTIFICATION;

typedef enum D3D12_QUERY_HEAP_FLAGS
{
  D3D12_QUERY_HEAP_FLAG_NONE = 0,
  D3D12_QUERY_HEAP_FLAG_CPU_RESOLVE = 1
} D3D12_QUERY_HEAP_FLAGS;

MIDL_INTERFACE("76cff76f-1e9b-4450-8cdc-34f1af788e5b")
ID3D12Device15 : public ID3D12Device14
{
public:
  virtual HRESULT STDMETHODCALLTYPE RegisterTrimNotificationCallback(
      _Inout_ D3D12_REGISTER_TRIM_NOTIFICATION * pData) = 0;

  virtual HRESULT STDMETHODCALLTYPE UnregisterTrimNotificationCallback(DWORD CallbackCookie) = 0;

  virtual HRESULT STDMETHODCALLTYPE TryCreateShaderResourceView(
      _In_opt_ ID3D12Resource * pResource, _In_opt_ const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc,
      _In_ D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;

  virtual HRESULT STDMETHODCALLTYPE TryCreateUnorderedAccessView(
      _In_opt_ ID3D12Resource * pResource, _In_opt_ ID3D12Resource * pCounterResource,
      _In_opt_ const D3D12_UNORDERED_ACCESS_VIEW_DESC *pDesc,
      _In_ D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;

  virtual HRESULT STDMETHODCALLTYPE TryCreateConstantBufferView(
      _In_opt_ const D3D12_CONSTANT_BUFFER_VIEW_DESC *pDesc,
      _In_ D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;

  virtual HRESULT STDMETHODCALLTYPE TryCreateSampler2(
      _In_ const D3D12_SAMPLER_DESC2 *pDesc, _In_ D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;

  virtual HRESULT STDMETHODCALLTYPE TryCreateRenderTargetView(
      _In_opt_ ID3D12Resource * pResource, _In_opt_ const D3D12_RENDER_TARGET_VIEW_DESC *pDesc,
      _In_ D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;

  virtual HRESULT STDMETHODCALLTYPE TryCreateDepthStencilView(
      _In_opt_ ID3D12Resource * pResource, _In_opt_ const D3D12_DEPTH_STENCIL_VIEW_DESC *pDesc,
      _In_ D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;

  virtual HRESULT STDMETHODCALLTYPE TryCreateSamplerFeedbackUnorderedAccessView(
      _In_opt_ ID3D12Resource * pTargetedResource, _In_opt_ ID3D12Resource * pFeedbackResource,
      _In_ D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;

  virtual HRESULT STDMETHODCALLTYPE CreateQueryHeap1(
      _In_ const D3D12_QUERY_HEAP_DESC *pDesc, _In_ D3D12_QUERY_HEAP_FLAGS Flags, _In_ REFIID riid,
      _COM_Outptr_ void **ppvHeap) = 0;

  virtual HRESULT STDMETHODCALLTYPE ResolveQueryData(
      _In_ ID3D12QueryHeap * pQueryHeap, _In_ D3D12_QUERY_TYPE Type, _In_ UINT StartIndex,
      _In_ UINT NumQueries, _Inout_ void *pResolvedQueryData) = 0;
};

#endif
