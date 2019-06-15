#include "main.hpp"
#include <dialog.hpp>
#include <ice/context.hpp>
#include <wrl/client.h>
#include <thread>

using Microsoft::WRL::ComPtr;

class Application : public Dialog<Application> {
public:
  Application(HINSTANCE hinstance) noexcept : Dialog(hinstance, nullptr, IDD_MAIN, IDI_MAIN) {
    thread_ = std::thread([this]() { io_.run(); });
  }

  ~Application() {
    io_.stop();
    thread_.join();
  }

  ice::schedule Io() noexcept {
    return io_;
  }

  ice::task<void> OnCreate() noexcept {
    co_await Io();
    SetStatus(L"Waiting for device...");
    Sleep(2000);
    EnableWindow(GetControl(IDC_INSTALL), TRUE);
    SetStatus(L"");
    co_return;
  }

  ice::task<void> OnClose() noexcept {
    DestroyWindow(hwnd_);
    co_return;
  }

  ice::task<void> OnDestroy() noexcept {
    PostQuitMessage(0);
    co_return;
  }

  ice::task<void> OnInstall() noexcept {
    SendMessage(hwnd_, WM_CLOSE, 0, 0);
    co_return;
  }

  BOOL OnMenu(UINT id) noexcept {
    switch (id) {
    case IDC_INSTALL:
      OnInstall().detach();
      return TRUE;
    }
    return FALSE;
  }

  BOOL OnAccelerator(UINT id) noexcept {
    return FALSE;
  }

  BOOL OnCommand(UINT code, UINT id, HWND hwnd) noexcept {
    switch (code) {
    case 0:
      return OnMenu(id);
    case 1:
      return OnAccelerator(id);
    }
    return FALSE;
  }

  void SetStatus(LPCWSTR status) {
    SendMessage(GetDlgItem(hwnd_, IDC_STATUS), SB_SETTEXT, 0, reinterpret_cast<LPARAM>(status));
  }

  static BOOL Initialize() noexcept {
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    return InitCommonControlsEx(&icc);
  }

  static int Run() noexcept {
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
  }

private:
  ice::context io_;
  std::thread thread_;
};

int __stdcall wWinMain(HINSTANCE hinstance, HINSTANCE, LPWSTR, int) {
  Application::Initialize();
  Application application(hinstance);
  return Application::Run();
}
