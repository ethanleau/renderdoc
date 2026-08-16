/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include "core/core.h"
#include "hooks/hooks.h"
#include "dxgi_wrapped.h"

typedef HRESULT(WINAPI *PFN_CREATE_DXGI_FACTORY)(REFIID, void **);
typedef HRESULT(WINAPI *PFN_CREATE_DXGI_FACTORY2)(UINT, REFIID, void **);
typedef HRESULT(WINAPI *PFN_GET_DEBUG_INTERFACE)(REFIID, void **);
typedef HRESULT(WINAPI *PFN_GET_DEBUG_INTERFACE1)(UINT, REFIID, void **);

MIDL_INTERFACE("9F251514-9D4D-4902-9D60-18988AB7D4B5")
IDXGraphicsAnalysis : public IUnknown
{
  virtual void STDMETHODCALLTYPE BeginCapture() = 0;
  virtual void STDMETHODCALLTYPE EndCapture() = 0;
};

struct RenderDocAnalysis : IDXGraphicsAnalysis
{
  // IUnknown boilerplate
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) { return E_NOINTERFACE; }
  ULONG STDMETHODCALLTYPE AddRef()
  {
    InterlockedIncrement(&m_iRefcount);
    return m_iRefcount;
  }
  ULONG STDMETHODCALLTYPE Release() { return InterlockedDecrement(&m_iRefcount); }
  unsigned int m_iRefcount = 0;

  // IDXGraphicsAnalysis
  void STDMETHODCALLTYPE BeginCapture()
  {
    DeviceOwnedWindow devWnd;
    RenderDoc::Inst().GetActiveWindow(devWnd);

    RenderDoc::Inst().StartFrameCapture(devWnd);
  }

  void STDMETHODCALLTYPE EndCapture()
  {
    DeviceOwnedWindow devWnd;
    RenderDoc::Inst().GetActiveWindow(devWnd);

    RenderDoc::Inst().EndFrameCapture(devWnd);
  }
};

struct DummyDXGIInfoQueue : public IDXGIInfoQueue
{
public:
  // IUnknown boilerplate
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) { return E_NOINTERFACE; }
  ULONG STDMETHODCALLTYPE AddRef()
  {
    InterlockedIncrement(&m_iRefcount);
    return m_iRefcount;
  }
  ULONG STDMETHODCALLTYPE Release() { return InterlockedDecrement(&m_iRefcount); }
  unsigned int m_iRefcount = 0;
  // IDXGIInfoQueue
  virtual HRESULT STDMETHODCALLTYPE SetMessageCountLimit(DXGI_DEBUG_ID Producer,
                                                         UINT64 MessageCountLimit)
  {
    return S_OK;
  }

  virtual void STDMETHODCALLTYPE ClearStoredMessages(DXGI_DEBUG_ID Producer) { return; }
  virtual HRESULT STDMETHODCALLTYPE GetMessage(DXGI_DEBUG_ID Producer, UINT64 MessageIndex,
                                               _Out_writes_bytes_opt_(*pMessageByteLength)
                                                   DXGI_INFO_QUEUE_MESSAGE *pMessage,
                                               _Inout_ SIZE_T *pMessageByteLength)
  {
    return S_OK;
  }

  virtual UINT64 STDMETHODCALLTYPE GetNumStoredMessagesAllowedByRetrievalFilters(DXGI_DEBUG_ID Producer)
  {
    return 0;
  }

  virtual UINT64 STDMETHODCALLTYPE GetNumStoredMessages(DXGI_DEBUG_ID Producer) { return 0; }
  virtual UINT64 STDMETHODCALLTYPE GetNumMessagesDiscardedByMessageCountLimit(DXGI_DEBUG_ID Producer)
  {
    return 0;
  }

  virtual UINT64 STDMETHODCALLTYPE GetMessageCountLimit(DXGI_DEBUG_ID Producer) { return 0; }
  virtual UINT64 STDMETHODCALLTYPE GetNumMessagesAllowedByStorageFilter(DXGI_DEBUG_ID Producer)
  {
    return 0;
  }

  virtual UINT64 STDMETHODCALLTYPE GetNumMessagesDeniedByStorageFilter(DXGI_DEBUG_ID Producer)
  {
    return 0;
  }

  virtual HRESULT STDMETHODCALLTYPE AddStorageFilterEntries(DXGI_DEBUG_ID Producer,
                                                            DXGI_INFO_QUEUE_FILTER *pFilter)
  {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE GetStorageFilter(DXGI_DEBUG_ID Producer,
                                                     _Out_writes_bytes_opt_(*pFilterByteLength)
                                                         DXGI_INFO_QUEUE_FILTER *pFilter,
                                                     _Inout_ SIZE_T *pFilterByteLength)
  {
    return S_OK;
  }

  virtual void STDMETHODCALLTYPE ClearStorageFilter(DXGI_DEBUG_ID Producer) { return; }
  virtual HRESULT STDMETHODCALLTYPE PushEmptyStorageFilter(DXGI_DEBUG_ID Producer) { return S_OK; }
  virtual HRESULT STDMETHODCALLTYPE PushDenyAllStorageFilter(DXGI_DEBUG_ID Producer)
  {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE PushCopyOfStorageFilter(DXGI_DEBUG_ID Producer) { return S_OK; }
  virtual HRESULT STDMETHODCALLTYPE PushStorageFilter(DXGI_DEBUG_ID Producer,
                                                      DXGI_INFO_QUEUE_FILTER *pFilter)
  {
    return S_OK;
  }

  virtual void STDMETHODCALLTYPE PopStorageFilter(DXGI_DEBUG_ID Producer) { return; }
  virtual UINT STDMETHODCALLTYPE GetStorageFilterStackSize(DXGI_DEBUG_ID Producer) { return 0; }
  virtual HRESULT STDMETHODCALLTYPE AddRetrievalFilterEntries(DXGI_DEBUG_ID Producer,
                                                              DXGI_INFO_QUEUE_FILTER *pFilter)
  {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE GetRetrievalFilter(DXGI_DEBUG_ID Producer,
                                                       _Out_writes_bytes_opt_(*pFilterByteLength)
                                                           DXGI_INFO_QUEUE_FILTER *pFilter,
                                                       _Inout_ SIZE_T *pFilterByteLength)
  {
    return S_OK;
  }

  virtual void STDMETHODCALLTYPE ClearRetrievalFilter(DXGI_DEBUG_ID Producer) { return; }
  virtual HRESULT STDMETHODCALLTYPE PushEmptyRetrievalFilter(DXGI_DEBUG_ID Producer)
  {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE PushDenyAllRetrievalFilter(DXGI_DEBUG_ID Producer)
  {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE PushCopyOfRetrievalFilter(DXGI_DEBUG_ID Producer)
  {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE PushRetrievalFilter(DXGI_DEBUG_ID Producer,
                                                        DXGI_INFO_QUEUE_FILTER *pFilter)
  {
    return S_OK;
  }

  virtual void STDMETHODCALLTYPE PopRetrievalFilter(DXGI_DEBUG_ID Producer) { return; }
  virtual UINT STDMETHODCALLTYPE GetRetrievalFilterStackSize(DXGI_DEBUG_ID Producer) { return 0; }
  virtual HRESULT STDMETHODCALLTYPE AddMessage(DXGI_DEBUG_ID Producer,
                                               DXGI_INFO_QUEUE_MESSAGE_CATEGORY Category,
                                               DXGI_INFO_QUEUE_MESSAGE_SEVERITY Severity,
                                               DXGI_INFO_QUEUE_MESSAGE_ID ID, LPCSTR pDescription)
  {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE AddApplicationMessage(DXGI_INFO_QUEUE_MESSAGE_SEVERITY Severity,
                                                          LPCSTR pDescription)
  {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE SetBreakOnCategory(DXGI_DEBUG_ID Producer,
                                                       DXGI_INFO_QUEUE_MESSAGE_CATEGORY Category,
                                                       BOOL bEnable)
  {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE SetBreakOnSeverity(DXGI_DEBUG_ID Producer,
                                                       DXGI_INFO_QUEUE_MESSAGE_SEVERITY Severity,
                                                       BOOL bEnable)
  {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE SetBreakOnID(DXGI_DEBUG_ID Producer,
                                                 DXGI_INFO_QUEUE_MESSAGE_ID ID, BOOL bEnable)
  {
    return S_OK;
  }

  virtual BOOL STDMETHODCALLTYPE GetBreakOnCategory(DXGI_DEBUG_ID Producer,
                                                    DXGI_INFO_QUEUE_MESSAGE_CATEGORY Category)
  {
    return FALSE;
  }

  virtual BOOL STDMETHODCALLTYPE GetBreakOnSeverity(DXGI_DEBUG_ID Producer,
                                                    DXGI_INFO_QUEUE_MESSAGE_SEVERITY Severity)
  {
    return FALSE;
  }

  virtual BOOL STDMETHODCALLTYPE GetBreakOnID(DXGI_DEBUG_ID Producer, DXGI_INFO_QUEUE_MESSAGE_ID ID)
  {
    return FALSE;
  }

  virtual void STDMETHODCALLTYPE SetMuteDebugOutput(DXGI_DEBUG_ID Producer, BOOL bMute) { return; }
  virtual BOOL STDMETHODCALLTYPE GetMuteDebugOutput(DXGI_DEBUG_ID Producer) { return FALSE; }
};

class DXGIHook : LibraryHook
{
public:
  void RegisterHooks()
  {
    RDCLOG("Registering DXGI hooks");

    LibraryHooks::RegisterLibraryHook("dxgi.dll", NULL);
    LibraryHooks::RegisterLibraryHook("sl.interposer.dll", NULL);

    CreateDXGIFactory.Register("dxgi.dll", "CreateDXGIFactory", CreateDXGIFactory_hook);
    CreateDXGIFactory1.Register("dxgi.dll", "CreateDXGIFactory1", CreateDXGIFactory1_hook);
    CreateDXGIFactory2.Register("dxgi.dll", "CreateDXGIFactory2", CreateDXGIFactory2_hook);
    StreamlineCreateDXGIFactory.Register("sl.interposer.dll", "CreateDXGIFactory",
                                         StreamlineCreateDXGIFactory_hook);
    StreamlineCreateDXGIFactory1.Register("sl.interposer.dll", "CreateDXGIFactory1",
                                          StreamlineCreateDXGIFactory1_hook);
    StreamlineCreateDXGIFactory2.Register("sl.interposer.dll", "CreateDXGIFactory2",
                                          StreamlineCreateDXGIFactory2_hook);
    GetDebugInterface.Register("dxgi.dll", "DXGIGetDebugInterface", DXGIGetDebugInterface_hook);
    GetDebugInterface1.Register("dxgi.dll", "DXGIGetDebugInterface1", DXGIGetDebugInterface1_hook);

    m_RecurseSlot = Threading::AllocateTLSSlot();
    Threading::SetTLSValue(m_RecurseSlot, NULL);
  }

private:
  static DXGIHook dxgihooks;

  RenderDocAnalysis m_RenderDocAnalysis;
  DummyDXGIInfoQueue m_DummyInfoQueue;

  HookedFunction<PFN_CREATE_DXGI_FACTORY> CreateDXGIFactory;
  HookedFunction<PFN_CREATE_DXGI_FACTORY> CreateDXGIFactory1;
  HookedFunction<PFN_CREATE_DXGI_FACTORY2> CreateDXGIFactory2;
  HookedFunction<PFN_CREATE_DXGI_FACTORY> StreamlineCreateDXGIFactory;
  HookedFunction<PFN_CREATE_DXGI_FACTORY> StreamlineCreateDXGIFactory1;
  HookedFunction<PFN_CREATE_DXGI_FACTORY2> StreamlineCreateDXGIFactory2;
  HookedFunction<PFN_GET_DEBUG_INTERFACE> GetDebugInterface;
  HookedFunction<PFN_GET_DEBUG_INTERFACE1> GetDebugInterface1;

  uint64_t m_RecurseSlot = 0;

  void EndRecurse() { Threading::SetTLSValue(m_RecurseSlot, NULL); }
  bool CheckRecurse()
  {
    if(Threading::GetTLSValue(m_RecurseSlot) == NULL)
    {
      Threading::SetTLSValue(m_RecurseSlot, (void *)1);
      return false;
    }

    return true;
  }

  HRESULT CreateFactory_Internal(PFN_CREATE_DXGI_FACTORY real, const char *name, REFIID riid,
                                 void **ppFactory)
  {
    if(CheckRecurse())
      return real(riid, ppFactory);

    if(ppFactory)
      *ppFactory = NULL;

    HRESULT ret = real(riid, ppFactory);

    if(SUCCEEDED(ret))
      RefCountDXGIObject::HandleWrap(name, riid, ppFactory);

    EndRecurse();
    return ret;
  }

  HRESULT CreateFactory2_Internal(PFN_CREATE_DXGI_FACTORY2 real, const char *name, UINT Flags,
                                  REFIID riid, void **ppFactory)
  {
    if(CheckRecurse())
      return real(Flags, riid, ppFactory);

    if(ppFactory)
      *ppFactory = NULL;

    HRESULT ret = real(Flags, riid, ppFactory);

    if(SUCCEEDED(ret))
      RefCountDXGIObject::HandleWrap(name, riid, ppFactory);

    EndRecurse();
    return ret;
  }

  static HRESULT WINAPI CreateDXGIFactory_hook(__in REFIID riid, __out void **ppFactory)
  {
    return dxgihooks.CreateFactory_Internal(dxgihooks.CreateDXGIFactory(), "CreateDXGIFactory",
                                            riid, ppFactory);
  }

  static HRESULT WINAPI CreateDXGIFactory1_hook(__in REFIID riid, __out void **ppFactory)
  {
    return dxgihooks.CreateFactory_Internal(dxgihooks.CreateDXGIFactory1(), "CreateDXGIFactory1",
                                            riid, ppFactory);
  }

  static HRESULT WINAPI CreateDXGIFactory2_hook(UINT Flags, REFIID riid, void **ppFactory)
  {
    return dxgihooks.CreateFactory2_Internal(dxgihooks.CreateDXGIFactory2(), "CreateDXGIFactory2",
                                             Flags, riid, ppFactory);
  }

  static HRESULT WINAPI StreamlineCreateDXGIFactory_hook(__in REFIID riid, __out void **ppFactory)
  {
    return dxgihooks.CreateFactory_Internal(dxgihooks.StreamlineCreateDXGIFactory(),
                                            "Streamline::CreateDXGIFactory", riid, ppFactory);
  }

  static HRESULT WINAPI StreamlineCreateDXGIFactory1_hook(__in REFIID riid, __out void **ppFactory)
  {
    return dxgihooks.CreateFactory_Internal(dxgihooks.StreamlineCreateDXGIFactory1(),
                                            "Streamline::CreateDXGIFactory1", riid, ppFactory);
  }

  static HRESULT WINAPI StreamlineCreateDXGIFactory2_hook(UINT Flags, REFIID riid, void **ppFactory)
  {
    return dxgihooks.CreateFactory2_Internal(dxgihooks.StreamlineCreateDXGIFactory2(),
                                             "Streamline::CreateDXGIFactory2", Flags, riid,
                                             ppFactory);
  }

  static HRESULT WINAPI DXGIGetDebugInterface_hook(REFIID riid, void **ppDebug)
  {
    if(ppDebug)
      *ppDebug = NULL;

    if(riid == __uuidof(IDXGraphicsAnalysis))
    {
      dxgihooks.m_RenderDocAnalysis.AddRef();
      if(ppDebug)
        *ppDebug = &dxgihooks.m_RenderDocAnalysis;
      return S_OK;
    }
    if(riid == __uuidof(IDXGIInfoQueue))
    {
      RDCWARN(
          "Returning a dummy IDXGIInfoQueue that does nothing. RenderDoc takes control of the "
          "debug layer.");
      dxgihooks.m_DummyInfoQueue.AddRef();
      if(ppDebug)
        *ppDebug = &dxgihooks.m_DummyInfoQueue;
      return S_OK;
    }

    // IDXGIDebug and IDXGIDebug1 can come through here, but we don't need to wrap them.

    if(dxgihooks.GetDebugInterface())
      return dxgihooks.GetDebugInterface()(riid, ppDebug);
    else
      return E_NOINTERFACE;
  }

  static HRESULT WINAPI DXGIGetDebugInterface1_hook(UINT Flags, REFIID riid, void **ppDebug)
  {
    if(ppDebug)
      *ppDebug = NULL;

    if(riid == __uuidof(IDXGraphicsAnalysis))
    {
      dxgihooks.m_RenderDocAnalysis.AddRef();
      if(ppDebug)
        *ppDebug = &dxgihooks.m_RenderDocAnalysis;
      return S_OK;
    }
    if(riid == __uuidof(IDXGIInfoQueue))
    {
      RDCWARN(
          "Returning a dummy IDXGIInfoQueue that does nothing. RenderDoc takes control of the "
          "debug layer.");
      dxgihooks.m_DummyInfoQueue.AddRef();
      if(ppDebug)
        *ppDebug = &dxgihooks.m_DummyInfoQueue;
      return S_OK;
    }

    // IDXGIDebug and IDXGIDebug1 can come through here, but we don't need to wrap them.

    if(dxgihooks.GetDebugInterface1())
      return dxgihooks.GetDebugInterface1()(Flags, riid, ppDebug);
    else
      return E_NOINTERFACE;
  }
};

DXGIHook DXGIHook::dxgihooks;
