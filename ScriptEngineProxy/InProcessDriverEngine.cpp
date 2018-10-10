// InProcessDriverEngine.cpp : Implementation of InProcessDriverEngine

#include "stdafx.h"
#include <string>
#include <vector>
#include <iostream>
#include "InProcessDriverEngine.h"


// InProcessDriverEngine

STDMETHODIMP_(HRESULT __stdcall) InProcessDriverEngine::SetSite(IUnknown * pUnkSite)
{
  HRESULT hr = S_OK;
  if (pUnkSite == nullptr) {
    return hr;
  }

  CComPtr<IWebBrowser2> browser;
  pUnkSite->QueryInterface<IWebBrowser2>(&browser);
  if (browser != nullptr)
  {
    CComPtr<IDispatch> document_dispatch;
    hr = browser->get_Document(&document_dispatch);
    ATLENSURE_RETURN_HR(document_dispatch.p != nullptr, E_FAIL);

    CComPtr<IHTMLDocument> document;
    hr = document_dispatch->QueryInterface<IHTMLDocument>(&document);

    CComQIPtr<IServiceProvider> spSP(document_dispatch);
    ATLENSURE_RETURN_HR(spSP != nullptr, E_NOINTERFACE);
    CComPtr<IDiagnosticsScriptEngineProvider> spEngineProvider;
    hr = spSP->QueryService(IID_IDiagnosticsScriptEngineProvider, &spEngineProvider);
    hr = spEngineProvider->CreateDiagnosticsScriptEngine(this, FALSE, 0, &this->script_engine_);
    if (this->m_hWnd == NULL) {
      this->Create(HWND_MESSAGE);
    }
    this->browser_ = browser;
    this->script_host_document_ = document;
    hr = this->script_engine_->EvaluateScript(L"browser.addEventListener('consoleMessage', function(e) { external.sendMessage('consoleMessage', JSON.stringify(e)); });", L"");
  }
  return hr;
}

STDMETHODIMP_(HRESULT __stdcall) InProcessDriverEngine::GetWindow(HWND * pHwnd)
{
  *pHwnd = this->m_hWnd;
  return S_OK;
}

STDMETHODIMP_(HRESULT __stdcall) InProcessDriverEngine::OnMessage(LPCWSTR * pszData, ULONG ulDataCount)
{
  std::vector<std::wstring> message;
  for (ULONG i = 0; i < ulDataCount; ++i) {
    std::wstring data(*pszData);
    message.push_back(data);
    ++pszData;
  }
  return S_OK;
}

STDMETHODIMP_(HRESULT __stdcall) InProcessDriverEngine::OnScriptError(IActiveScriptError * pScriptError)
{
  return S_OK;
}

LRESULT InProcessDriverEngine::OnExecuteScript(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &)
{
  this->DoScriptExecution(L"console.log('hello world');");
  this->DoScriptExecution(L"return document.title;");
  return 0;
}

int InProcessDriverEngine::DoScriptExecution(const std::wstring& source)
{
  HRESULT hr = S_OK;
  CComVariant created_javascript_function;
  this->CreateJavaScriptFunction(source, &created_javascript_function);
  CComPtr<IDispatchEx> function_dispatch;
  hr = created_javascript_function.pdispVal->QueryInterface<IDispatchEx>(&function_dispatch);
  if (FAILED(hr)) {
    return 4;
  }

  // Grab the "call" method out of the returned function
  DISPID call_member_id;
  CComBSTR call_member_name = L"call";
  hr = function_dispatch->GetDispID(call_member_name, 0, &call_member_id);

  CComPtr<IHTMLDocument2> doc;
  this->script_host_document_->QueryInterface<IHTMLDocument2>(&doc);
  CComPtr<IHTMLWindow2> win;
  hr = doc->get_parentWindow(&win);
  CComVariant window_variant(win);

  std::vector<CComVariant> function_args(1);
  function_args[0].Copy(&window_variant);

  DISPPARAMS call_parameters = { 0 };
  memset(&call_parameters, 0, sizeof call_parameters);
  call_parameters.cArgs = 1;
  call_parameters.rgvarg = &function_args[0];

  CComVariant result;
  hr = function_dispatch->InvokeEx(call_member_id,
                                   LOCALE_USER_DEFAULT,
                                   DISPATCH_METHOD,
                                   &call_parameters,
                                   &result,
                                   NULL,
                                   NULL);
  return 0;
}

int InProcessDriverEngine::CreateJavaScriptFunction(const std::wstring& source, VARIANT* function_variant)
{
  if (NULL == this->script_host_document_) {
    std::cout << "Invalid document pointer\n";
    return 5;
  }

  CComPtr<IDispatch> script_dispatch;
  HRESULT hr = this->script_host_document_->get_Script(&script_dispatch);
  if (FAILED(hr)) {
    return 1;
  }

  CComPtr<IDispatchEx> script_engine;
  hr = script_dispatch->QueryInterface<IDispatchEx>(&script_engine);
  if (FAILED(hr)) {
    return 2;
  }

  std::wstring item_type = L"Function";
  CComBSTR name(item_type.c_str());

  // Find the javascript object using the IDispatchEx of the script engine
  DISPID dispatch_id;
  hr = script_engine->GetDispID(name, 0, &dispatch_id);
  if (FAILED(hr)) {
    return 3;
  }

  std::vector<CComVariant> function_constructor_args(1);
  CComVariant function_src = source.c_str();
  function_constructor_args[0].Copy(&function_src);

  DISPPARAMS constructor_params = { 0 };
  memset(&constructor_params, 0, sizeof constructor_params);
  constructor_params.cArgs = 1;
  constructor_params.rgvarg = &function_constructor_args[0];

  // Create the jscript object by calling its constructor
  // The below InvokeEx call returns E_INVALIDARG in this case
  hr = script_engine->InvokeEx(dispatch_id,
                               LOCALE_USER_DEFAULT,
                               DISPATCH_CONSTRUCT,
                               &constructor_params,
                               function_variant,
                               NULL,
                               NULL);
  return 0;
}
