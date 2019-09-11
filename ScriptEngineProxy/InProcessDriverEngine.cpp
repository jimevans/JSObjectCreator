// InProcessDriverEngine.cpp : Implementation of InProcessDriverEngine

#include "stdafx.h"
#include <string>
#include <vector>
#include <iostream>
#include <comdef.h>
#include <ShlGuid.h>
#include "InProcessDriverEngine.h"
#include "StringUtilities.h"

struct WaitThreadContext {
	HWND window_handle;
};

// InProcessDriverEngine

InProcessDriverEngine::InProcessDriverEngine() {
    this->is_navigating_ = false;
    this->current_command_ = "";
    this->command_data_ = "";
    this->response_ = "";
    this->Create(HWND_MESSAGE);
}

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

	CComPtr<IServiceProvider> browser_service_provider;
	hr = browser->QueryInterface<IServiceProvider>(&browser_service_provider);
	CComPtr<IOleWindow> window;
	hr = browser_service_provider->QueryService<IOleWindow>(SID_SShellBrowser, &window);
	HWND handle;
	window->GetWindow(&handle);

	//if (this->m_hWnd == NULL) {
 //     this->Create(HWND_MESSAGE);
 //   }
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
  if (message.at(0) == L"snapshot") {
    this->response_ = StringUtilities::ToString(message.at(1));
  }

  if (message.at(0) == L"debug") {
    ::Sleep(1);
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
	if (this->browser_.IsEqualObject(pDisp)) {
		this->is_navigating_ = false;
		this->CreateWaitThread(this->m_hWnd);
	}
}

STDMETHODIMP_(void) InProcessDriverEngine::OnDocumentComplete(LPDISPATCH pDisp, VARIANT* URL) {
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
	if (this->IsDocumentReady()) {
		this->response_ = "done";
	} else {
		this->CreateWaitThread(this->m_hWnd);
	}
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

void InProcessDriverEngine::DoSomething() {
	CComPtr<IDispatch> dispatch;
	HRESULT hr = this->browser_->get_Document(&dispatch);
	if (FAILED(hr)) {
	}

	CComPtr<IHTMLDocument2> doc;
	hr = dispatch->QueryInterface(&doc);
	if (FAILED(hr)) {
	}

	CComPtr<IHTMLElement> body;
	hr = doc->get_body(&body);

	CComPtr<IDisplayServices> display;
	hr = doc->QueryInterface<IDisplayServices>(&display);
	POINT p = { 0, 0 };
	hr = display->TransformPoint(&p, COORD_SYSTEM_CLIENT, COORD_SYSTEM_GLOBAL, body);
	this->response_ = "over";
}

LRESULT InProcessDriverEngine::OnExecuteCommand(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL & handled) {
  std::string response = "";
  if (this->current_command_ == "getTitle") {
	  this->GetTitle();
  } else if (this->current_command_ == "get") {
    this->Navigate();
  } else if (this->current_command_ == "snapshot") {
    this->TakeSnapshot();
  } else if (this->current_command_ == "foo") {
    this->DoSomething();
  }

  return 0;
}

void InProcessDriverEngine::TakeSnapshot() {
  std::wstring script = L"var shotBlob = browser.takeVisualSnapshot();";
  script.append(L"var reader = new FileReader();");
  script.append(L"reader.onloadend = function() { external.sendMessage('snapshot', reader.result); };");
  script.append(L"reader.readAsText(shotBlob);");
  this->script_engine_->EvaluateScript(script.c_str(), L"");
}

bool InProcessDriverEngine::IsDocumentReady() {
	HRESULT hr = S_OK;
	CComPtr<IDispatch> document_dispatch;
	hr = this->browser_->get_Document(&document_dispatch);
	if (FAILED(hr) || document_dispatch == nullptr) {
		return false;
	}

	CComPtr<IHTMLDocument2> doc;
	hr = document_dispatch->QueryInterface<IHTMLDocument2>(&doc);
	if (FAILED(hr) || doc == nullptr) {
		return false;
	}

	CComBSTR ready_state_bstr;
	hr = doc->get_readyState(&ready_state_bstr);
	if (FAILED(hr)) {
		return false;
	}

	return ready_state_bstr == L"complete";
}

void InProcessDriverEngine::CreateWaitThread(HWND return_handle) {
	// If we are still waiting, we need to wait a bit then post a message to
	// ourselves to run the wait again. However, we can't wait using Sleep()
	// on this thread. This call happens in a message loop, and we would be
	// unable to process the COM events in the browser if we put this thread
	// to sleep.
	WaitThreadContext* thread_context = new WaitThreadContext;
	thread_context->window_handle = this->m_hWnd;
	unsigned int thread_id = 0;
	HANDLE thread_handle = reinterpret_cast<HANDLE>(_beginthreadex(NULL,
																	0,
																	&InProcessDriverEngine::WaitThreadProc,
																	reinterpret_cast<void*>(thread_context),
																	0,
																	&thread_id));
	if (thread_handle != NULL) {
		::CloseHandle(thread_handle);
	}
}


unsigned int WINAPI InProcessDriverEngine::WaitThreadProc(LPVOID lpParameter) {
	WaitThreadContext* thread_context = reinterpret_cast<WaitThreadContext*>(lpParameter);
	HWND window_handle = thread_context->window_handle;
	delete thread_context;

	::Sleep(50);
	::PostMessage(window_handle, WM_WAIT, NULL, NULL);
	return 0;
}
