/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024-2026 Baldur Karlsson
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

#include "d3d12_device.h"
#include "d3d12_resources.h"

HRESULT WrappedID3D12Device::RegisterTrimNotificationCallback(D3D12_REGISTER_TRIM_NOTIFICATION *pData)
{
  return m_pDevice15 ? m_pDevice15->RegisterTrimNotificationCallback(pData) : E_NOINTERFACE;
}

HRESULT WrappedID3D12Device::UnregisterTrimNotificationCallback(DWORD CallbackCookie)
{
  return m_pDevice15 ? m_pDevice15->UnregisterTrimNotificationCallback(CallbackCookie)
                     : E_NOINTERFACE;
}

HRESULT WrappedID3D12Device::TryCreateSamplerFeedbackUnorderedAccessView(
    ID3D12Resource *pTargetedResource, ID3D12Resource *pFeedbackResource,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
  // Match the existing sampler-feedback capability clamp and avoid creating an untracked descriptor.
  RDCERR("Device15 sampler feedback is not supported");
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::CreateQueryHeap1(const D3D12_QUERY_HEAP_DESC *pDesc,
                                              D3D12_QUERY_HEAP_FLAGS Flags, REFIID riid,
                                              void **ppvHeap)
{
  if(Flags != D3D12_QUERY_HEAP_FLAG_NONE)
  {
    if(ppvHeap)
      *ppvHeap = NULL;
    RDCERR("Device15 CPU-resolve query heaps are not supported");
    return E_NOTIMPL;
  }
  // The flag-free form is equivalent to CreateQueryHeap and uses its existing replay chunk.
  return CreateQueryHeap(pDesc, riid, ppvHeap);
}

HRESULT WrappedID3D12Device::ResolveQueryData(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type,
                                              UINT StartIndex, UINT NumQueries,
                                              void *pResolvedQueryData)
{
  // CPU-resolve heaps cannot be created through this wrapper yet.
  RDCERR("Device15 CPU query resolution is not supported");
  return E_NOTIMPL;
}
