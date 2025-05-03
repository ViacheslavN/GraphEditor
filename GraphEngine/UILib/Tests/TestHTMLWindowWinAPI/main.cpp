#include <atlbase.h>
#include <atlapp.h>
#include "../../CommonLib/CommonLib.h"
#include "../../UILib/UILib.h"
#include "HtmlWindow.h"

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR    lpCmdLine,
                      _In_ int       nCmdShow)
{


    ::SciterSetOption(NULL, SCITER_SET_SCRIPT_RUNTIME_FEATURES,
                      ALLOW_FILE_IO |
                      ALLOW_SOCKET_IO |
                      ALLOW_EVAL |
                      ALLOW_SYSINFO |
                      ALLOW_CMODULES );

    CHtmlWindow window(hInstance);
    window.Create(0, L"", L"TestHtml", WS_OVERLAPPEDWINDOW, 200, 200, 800, 800, NULL, NULL, hInstance);
    //window.LoadFromResource(L"F:\\work\\MyProject\\GraphEditor\\GraphEngine\\UILib\\Tests\\TestHTMLWindowWTL\\res\\default.html");
    window.LoadFromResource(L"F:\\work\\Test\\sciter-js-sdk-main\\demos\\gsciter\\res\\default.htm");

    window.ShowWindow(true);
    UpdateWindow(window.get_hwnd());
    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {

            TranslateMessage(&msg);
            DispatchMessage(&msg);

    }

    return (int) msg.wParam;
}