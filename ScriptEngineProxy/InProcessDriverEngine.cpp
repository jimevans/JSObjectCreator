// InProcessDriverEngine.cpp : Implementation of InProcessDriverEngine

#include "stdafx.h"
#include <string>
#include <vector>
#include <iostream>
#include <comdef.h>
#include "InProcessDriverEngine.h"
#include "StringUtilities.h"

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
	this->DispEventAdvise(browser);
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

STDMETHODIMP_(void) InProcessDriverEngine::OnBeforeNavigate2(LPDISPATCH pDisp,
	VARIANT* pvarUrl,
	VARIANT* pvarFlags,
	VARIANT* pvarTargetFrame,
	VARIANT* pvarData,
	VARIANT* pvarHeaders,
	VARIANT_BOOL* pbCancel) {
}

STDMETHODIMP_(void) InProcessDriverEngine::OnNavigateComplete2(LPDISPATCH pDisp, VARIANT* URL) {
}

STDMETHODIMP_(void) InProcessDriverEngine::OnDocumentComplete(LPDISPATCH pDisp, VARIANT* URL) {
	if (this->browser_.IsEqualObject(pDisp)) {
		this->is_navigating_ = false;
		this->response_ = "done";
	}
}

STDMETHODIMP_(void) InProcessDriverEngine::OnNewWindow(LPDISPATCH ppDisp,
	VARIANT_BOOL* pbCancel,
	DWORD dwFlags,
	BSTR bstrUrlContext,
	BSTR bstrUrl) {
}

STDMETHODIMP_(void) InProcessDriverEngine::OnNewProcess(DWORD lCauseFlag,
	IDispatch* pWB2,
	VARIANT_BOOL* pbCancel) {
}

STDMETHODIMP_(void) InProcessDriverEngine::OnQuit() {
}

LRESULT InProcessDriverEngine::OnCopyData(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL & handled) {
  COPYDATASTRUCT* data = reinterpret_cast<COPYDATASTRUCT*>(lParam);
  this->return_window_ = reinterpret_cast<HWND>(wParam);
  std::vector<char> buffer(data->cbData + 1);
  memcpy_s(&buffer[0], data->cbData, data->lpData, data->cbData);
  buffer[buffer.size() - 1] = '\0';
  std::string received_data(&buffer[0]);
  if (received_data.find("\n") != std::string::npos) {
    std::vector<std::string> tokens;
    StringUtilities::Split(received_data, "\n", &tokens);
    this->current_command_ = tokens.at(0);
    this->command_data_ = tokens.at(1);
  } else {
    this->current_command_ = received_data;
  }
  
  return 0;
}

LRESULT InProcessDriverEngine::OnSetCommand(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& handled) {
    LPCSTR command = reinterpret_cast<LPCSTR>(lParam);
    this->current_command_ = command;
    return 0;
}

LRESULT InProcessDriverEngine::OnGetResponseLength(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& handled) {
    return this->response_.size();
}

LRESULT InProcessDriverEngine::OnGetResponse(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& handled) {
	HWND return_window_handle = NULL;
	if (wParam != NULL) {
		return_window_handle = reinterpret_cast<HWND>(wParam);
	}
	
	this->SendResponse(return_window_handle, this->response_);
    // Reset the serialized response for the next command.
    this->response_ = "";
    this->current_command_ = "";
	this->command_data_ = "";
    return 0;
}

LRESULT InProcessDriverEngine::OnWait(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& handled) {
	return 0;
}

void InProcessDriverEngine::SendResponse(HWND window_handle, std::string response) {
	std::vector<char> buffer(response.size() + 1);
	memcpy_s(&buffer[0], response.size() + 1, response.c_str(), response.size());
	buffer[buffer.size() - 1] = '\0';
	COPYDATASTRUCT copy_data;
	copy_data.cbData = buffer.size();
	copy_data.lpData = reinterpret_cast<void*>(&buffer[0]);
	::SendMessage(window_handle, WM_COPYDATA, NULL, reinterpret_cast<LPARAM>(&copy_data));
}

void InProcessDriverEngine::GetTitle() {
	CComPtr<IDispatch> dispatch;
	HRESULT hr = this->browser_->get_Document(&dispatch);
	if (FAILED(hr)) {
	}

	CComPtr<IHTMLDocument2> doc;
	hr = dispatch->QueryInterface(&doc);
	if (FAILED(hr)) {
	}

	CComBSTR title;
	hr = doc->get_title(&title);
	if (FAILED(hr)) {
	}

	std::wstring converted_title = title;
	this->response_ = StringUtilities::ToString(converted_title);
}

void InProcessDriverEngine::Navigate() {
	this->is_navigating_ = true;
	std::wstring url = StringUtilities::ToWString(this->command_data_);
	this->is_navigating_ = true;
	CComVariant dummy;
	CComVariant url_variant(url.c_str());
	HRESULT hr = this->browser_->Navigate2(&url_variant,
											&dummy,
											&dummy,
											&dummy,
											&dummy);
	if (FAILED(hr)) {
		this->is_navigating_ = false;
		_com_error error(hr);
		std::wstring formatted_message = StringUtilities::Format(L"Received error: 0x%08x ['%s']",
			hr,
			error.ErrorMessage());
		this->response_ = StringUtilities::ToString(formatted_message);
	}
}

LRESULT InProcessDriverEngine::OnExecuteCommand(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL & handled) {
  std::string response = "";
  if (this->current_command_ == "getTitle") {
	this->GetTitle();
  } else if (this->current_command_ == "get") {
	this->Navigate();
  }

  return 0;
}
