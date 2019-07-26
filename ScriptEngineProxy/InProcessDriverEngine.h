// InProcessDriverEngine.h : Declaration of the InProcessDriverEngine

#pragma once
#include "resource.h"       // main symbols

#include <ExDispid.h>
#include <MsHTML.h>
#include <mshtmldiagnostics.h>
#include "messages.h"
#include "ScriptEngineProxy_i.h"

#define BROWSER_EVENTS_ID 250

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif

using namespace ATL;


// InProcessDriverEngine

class ATL_NO_VTABLE InProcessDriverEngine :
  public CComObjectRootEx<CComSingleThreadModel>,
  public CComCoClass<InProcessDriverEngine, &CLSID_InProcessDriverEngine>,
  public CWindowImpl<InProcessDriverEngine>,
  public IDispEventImpl<BROWSER_EVENTS_ID, InProcessDriverEngine, &DIID_DWebBrowserEvents2, &LIBID_SHDocVw, 1, 1>,
  public IObjectWithSiteImpl<InProcessDriverEngine>,
  public IOleWindow,
  public IDiagnosticsScriptEngineSite,
  public IInProcessDriverEngine
{
public:
	InProcessDriverEngine()
	{
		this->is_navigating_ = false;
		this->current_command_ = "";
		this->command_data_ = "";
		this->response_ = "";
	}

  DECLARE_NO_REGISTRY()
  DECLARE_NOT_AGGREGATABLE(InProcessDriverEngine)

  BEGIN_COM_MAP(InProcessDriverEngine)
	  COM_INTERFACE_ENTRY(IInProcessDriverEngine)
	  COM_INTERFACE_ENTRY(IObjectWithSite)
	  COM_INTERFACE_ENTRY(IOleWindow)
	  COM_INTERFACE_ENTRY(IDiagnosticsScriptEngineSite)
  END_COM_MAP()

  BEGIN_MSG_MAP(InProcessDriverEngine)
    MESSAGE_HANDLER(WM_COPYDATA, OnCopyData)
	MESSAGE_HANDLER(WM_SET_COMMAND, OnSetCommand)
    MESSAGE_HANDLER(WM_EXECUTE_COMMAND, OnExecuteCommand)
    MESSAGE_HANDLER(WM_GET_RESPONSE_LENGTH, OnGetResponseLength)
    MESSAGE_HANDLER(WM_GET_RESPONSE, OnGetResponse)
    MESSAGE_HANDLER(WM_WAIT, OnWait)
  END_MSG_MAP()

  BEGIN_SINK_MAP(InProcessDriverEngine)
	  SINK_ENTRY_EX(BROWSER_EVENTS_ID, DIID_DWebBrowserEvents2, DISPID_BEFORENAVIGATE2, &InProcessDriverEngine::OnBeforeNavigate2)
	  SINK_ENTRY_EX(BROWSER_EVENTS_ID, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2, &InProcessDriverEngine::OnNavigateComplete2)
	  SINK_ENTRY_EX(BROWSER_EVENTS_ID, DIID_DWebBrowserEvents2, DISPID_DOCUMENTCOMPLETE, &InProcessDriverEngine::OnDocumentComplete)
	  SINK_ENTRY_EX(BROWSER_EVENTS_ID, DIID_DWebBrowserEvents2, DISPID_NEWWINDOW3, &InProcessDriverEngine::OnNewWindow)
	  SINK_ENTRY_EX(BROWSER_EVENTS_ID, DIID_DWebBrowserEvents2, DISPID_NEWPROCESS, &InProcessDriverEngine::OnNewProcess)
	  SINK_ENTRY_EX(BROWSER_EVENTS_ID, DIID_DWebBrowserEvents2, DISPID_ONQUIT, &InProcessDriverEngine::OnQuit)
  END_SINK_MAP()

  // IObjectWithSite
  STDMETHOD(SetSite)(_In_opt_ IUnknown* pUnkSite);

  // IOleWindow
  STDMETHOD(GetWindow)(__RPC__deref_out_opt HWND* pHwnd);
  STDMETHOD(ContextSensitiveHelp)(BOOL fEnterMode) { return E_NOTIMPL; }

  // IDiagnosticsScriptEngineSite
  STDMETHOD(OnMessage)(LPCWSTR* pszData, ULONG ulDataCount);
  STDMETHOD(OnScriptError)(IActiveScriptError* pScriptError);

  // DWebBrowserEvents2
  STDMETHOD_(void, OnBeforeNavigate2)(LPDISPATCH pObject,
	  VARIANT* pvarUrl,
	  VARIANT* pvarFlags,
	  VARIANT* pvarTargetFrame,
	  VARIANT* pvarData,
	  VARIANT* pvarHeaders,
	  VARIANT_BOOL* pbCancel);
  STDMETHOD_(void, OnNavigateComplete2)(LPDISPATCH pDisp, VARIANT* URL);
  STDMETHOD_(void, OnDocumentComplete)(LPDISPATCH pDisp, VARIANT* URL);
  STDMETHOD_(void, OnNewWindow)(LPDISPATCH ppDisp,
	  VARIANT_BOOL* pbCancel,
	  DWORD dwFlags,
	  BSTR bstrUrlContext,
	  BSTR bstrUrl);
  STDMETHOD_(void, OnNewProcess)(DWORD lCauseFlag,
	  IDispatch* pWB2,
	  VARIANT_BOOL* pbCancel);
  STDMETHOD_(void, OnQuit)();

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

public:
  LRESULT OnCopyData(UINT nMsg, WPARAM wParam, LPARAM lParam, _Inout_ BOOL& /*bHandled*/);
  LRESULT OnSetCommand(UINT nMsg, WPARAM wParam, LPARAM lParam, _Inout_ BOOL& /*bHandled*/);
  LRESULT OnExecuteCommand(UINT nMsg, WPARAM wParam, LPARAM lParam, _Inout_ BOOL& /*bHandled*/);
  LRESULT OnGetResponseLength(UINT nMsg, WPARAM wParam, LPARAM lParam, _Inout_ BOOL& /*bHandled*/);
  LRESULT OnGetResponse(UINT nMsg, WPARAM wParam, LPARAM lParam, _Inout_ BOOL& /*bHandled*/);
  LRESULT OnWait(UINT nMsg, WPARAM wParam, LPARAM lParam, _Inout_ BOOL& /*bHandled*/);

private:
  void SendResponse(HWND window_handle, std::string response);

  void GetTitle(void);
  void Navigate(void);

  HWND return_window_;
  std::string current_command_;
  std::string command_data_;
  std::string response_;
  bool is_navigating_;

  CComPtr<IWebBrowser2> browser_;
  CComPtr<IHTMLDocument> script_host_document_;
  CComPtr<IDiagnosticsScriptEngine> script_engine_;
};

OBJECT_ENTRY_AUTO(__uuidof(InProcessDriverEngine), InProcessDriverEngine)
