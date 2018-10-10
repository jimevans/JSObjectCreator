// InProcessDriverEngine.h : Declaration of the InProcessDriverEngine

#pragma once
#include "resource.h"       // main symbols

#include "messages.h"
#include "mshtmldiagnostics.h"
#include "ScriptEngineProxy_i.h"



#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif

using namespace ATL;


// InProcessDriverEngine

class ATL_NO_VTABLE InProcessDriverEngine :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<InProcessDriverEngine, &CLSID_InProcessDriverEngine>,
  public CWindowImpl<InProcessDriverEngine>,
  public IObjectWithSiteImpl<InProcessDriverEngine>,
  public IOleWindow,
  public IDiagnosticsScriptEngineSite,
  public IInProcessDriverEngine
{
public:
	InProcessDriverEngine()
	{
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
    MESSAGE_HANDLER(WM_EXECUTE_SCRIPT, OnExecuteScript)
  END_MSG_MAP()

  // IObjectWithSite
  STDMETHOD(SetSite)(_In_opt_ IUnknown* pUnkSite);

  // IOleWindow
  STDMETHOD(GetWindow)(__RPC__deref_out_opt HWND* pHwnd);
  STDMETHOD(ContextSensitiveHelp)(BOOL fEnterMode) { return E_NOTIMPL; }

  // IDiagnosticsScriptEngineSite
  STDMETHOD(OnMessage)(LPCWSTR* pszData, ULONG ulDataCount);
  STDMETHOD(OnScriptError)(IActiveScriptError* pScriptError);

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

public:
  LRESULT OnExecuteScript(UINT nMsg, WPARAM wParam, LPARAM lParam, _Inout_ BOOL& /*bHandled*/);

private:
  int DoScriptExecution(const std::wstring& source);
  int CreateJavaScriptFunction(const std::wstring& source, VARIANT* function_variant);

  CComPtr<IWebBrowser2> browser_;
  CComPtr<IHTMLDocument> script_host_document_;
  CComPtr<IDiagnosticsScriptEngine> script_engine_;
};

OBJECT_ENTRY_AUTO(__uuidof(InProcessDriverEngine), InProcessDriverEngine)
