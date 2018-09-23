// JSObjectCreator.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <ctime>
#include <iostream>
#include <vector>
#include <iepmapi.h>


struct ProcessWindowInfo {
  DWORD dwProcessId;
  HWND hwndBrowser;
};

#define IE_FRAME_WINDOW_CLASS "IEFrame"
#define SHELL_DOCOBJECT_VIEW_WINDOW_CLASS "Shell DocObject View"
#define IE_SERVER_CHILD_WINDOW_CLASS "Internet Explorer_Server"
#define HTML_GETOBJECT_MSG L"WM_HTML_GETOBJECT"
#define OLEACC_LIBRARY_NAME L"OLEACC.DLL"

BOOL CALLBACK FindChildWindowForProcess(HWND hwnd, LPARAM arg) {
  ProcessWindowInfo *process_window_info = reinterpret_cast<ProcessWindowInfo*>(arg);

  // Could this be an Internet Explorer Server window?
  // 25 == "Internet Explorer_Server\0"
  char name[25];
  if (::GetClassNameA(hwnd, name, 25) == 0) {
    // No match found. Skip
    return TRUE;
  }

  if (strcmp(IE_SERVER_CHILD_WINDOW_CLASS, name) != 0) {
    return TRUE;
  } else {
    DWORD process_id = NULL;
    ::GetWindowThreadProcessId(hwnd, &process_id);
    if (process_window_info->dwProcessId == process_id) {
      // Once we've found the first Internet Explorer_Server window
      // for the process we want, we can stop.
      process_window_info->hwndBrowser = hwnd;
      return FALSE;
    }
  }

  return TRUE;
}

BOOL CALLBACK FindBrowserWindow(HWND hwnd, LPARAM arg) {
  // Could this be an IE instance?
  // 8 == "IeFrame\0"
  // 21 == "Shell DocObject View\0";
  char name[21];
  if (::GetClassNameA(hwnd, name, 21) == 0) {
    // No match found. Skip
    return TRUE;
  }

  if (strcmp(IE_FRAME_WINDOW_CLASS, name) != 0 &&
    strcmp(SHELL_DOCOBJECT_VIEW_WINDOW_CLASS, name) != 0) {
    return TRUE;
  }

  return EnumChildWindows(hwnd, FindChildWindowForProcess, arg);
}

bool GetDocumentFromWindowHandle(HWND window_handle,
                                 IHTMLDocument2** document) {
  UINT html_getobject_msg = ::RegisterWindowMessage(HTML_GETOBJECT_MSG);

  // Explicitly load MSAA so we know if it's installed
  HINSTANCE oleacc_instance_handle = ::LoadLibrary(OLEACC_LIBRARY_NAME);
  if (window_handle != NULL && oleacc_instance_handle) {
    LRESULT result;

    ::SendMessageTimeout(window_handle,
                         html_getobject_msg,
                         0L,
                         0L,
                         SMTO_ABORTIFHUNG,
                         1000,
                         reinterpret_cast<PDWORD_PTR>(&result));

    LPFNOBJECTFROMLRESULT object_pointer = reinterpret_cast<LPFNOBJECTFROMLRESULT>(::GetProcAddress(oleacc_instance_handle, "ObjectFromLresult"));
    if (object_pointer != NULL) {
      HRESULT hr;
      hr = (*object_pointer)(result,
                             IID_IHTMLDocument2,
                             0,
                             reinterpret_cast<void**>(document));
      if (SUCCEEDED(hr)) {
        return true;
      }
    }
  }

  if (oleacc_instance_handle) {
    ::FreeLibrary(oleacc_instance_handle);
  }
  return false;
}


bool LaunchIE(const std::wstring initial_browser_url,
              IHTMLDocument2** document) {
  bool is_launched = false;
  DWORD browser_attach_timeout = 10000;
  PROCESS_INFORMATION proc_info;
  HRESULT hr = ::IELaunchURL(initial_browser_url.c_str(),
                             &proc_info,
                             NULL);
  DWORD process_id = proc_info.dwProcessId;
  ::WaitForInputIdle(proc_info.hProcess, 2000);

  if (proc_info.hThread != NULL) {
    ::CloseHandle(proc_info.hThread);
  }

  if (proc_info.hProcess != NULL) {
    ::CloseHandle(proc_info.hProcess);
  }

  ProcessWindowInfo process_window_info;
  process_window_info.dwProcessId = proc_info.dwProcessId;
  process_window_info.hwndBrowser = NULL;
  clock_t end = clock() + (browser_attach_timeout / 1000 * CLOCKS_PER_SEC);
  while (process_window_info.hwndBrowser == NULL) {
    if (browser_attach_timeout > 0 && (clock() > end)) {
      break;
    }
    ::EnumWindows(&FindBrowserWindow,
                  reinterpret_cast<LPARAM>(&process_window_info));
    if (process_window_info.hwndBrowser == NULL) {
      ::Sleep(250);
    }
  }
  if (process_window_info.hwndBrowser) {
    is_launched = GetDocumentFromWindowHandle(process_window_info.hwndBrowser,
                                              document);
  }
  return is_launched;
}

int CreateJavaScriptObject(IHTMLDocument2* script_engine_host,
                           CComVariant* created_object) {
  if (NULL == script_engine_host) {
    std::cout << "Invalid document pointer\n";
    return 5;
  }

  CComPtr<IDispatch> script_dispatch;
  HRESULT hr = script_engine_host->get_Script(&script_dispatch);
  if (FAILED(hr)) {
    std::cout << "Failed to get script IDispatch from document pointer\n";
    return 1;
  }

  CComPtr<IDispatchEx> script_engine;
  hr = script_dispatch->QueryInterface<IDispatchEx>(&script_engine);
  if (FAILED(hr)) {
    std::cout << "Failed to get script IDispatchEx from script IDispatch\n";
    return 2;
  }

  // Create the variables we need
  DISPPARAMS no_arguments_dispatch_params = { NULL, NULL, 0, 0 };
  CComVariant created_javascript_object;
  DISPID dispatch_id;

  // Find the javascript object using the IDispatchEx of the script engine
  std::wstring item_type = L"Object";
  CComBSTR name(item_type.c_str());
  hr = script_engine->GetDispID(name, 0, &dispatch_id);
  if (FAILED(hr)) {
    std::cout << "Failed to get DispID for Object constructor\n";
    return 3;
  }

  // Create the jscript object by calling its constructor
  // The below InvokeEx call returns E_INVALIDARG in this case
  hr = script_engine->InvokeEx(dispatch_id,
                               LOCALE_USER_DEFAULT,
                               DISPATCH_CONSTRUCT,
                               &no_arguments_dispatch_params,
                               &created_javascript_object,
                               NULL,
                               NULL);
  if (FAILED(hr)) {
    std::cout << "Failed to call InvokeEx on Object constructor\n";
    return 4;
  }
  *created_object = created_javascript_object;
  return 0;
}
int main() {
  ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

  std::wstring initial_browser_url = L"https://stackoverflow.com";
  CComPtr<IHTMLDocument2> document;
  bool is_launched = LaunchIE(initial_browser_url, &document);
  if (is_launched) {
    CComVariant created_object;
    int created_status = CreateJavaScriptObject(document, &created_object);
  }
  std::cout << "Hello World!\n";

  ::CoUninitialize();
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
