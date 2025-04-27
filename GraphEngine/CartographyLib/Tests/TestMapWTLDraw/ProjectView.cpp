// TestGraphicsView.cpp : implementation of the CMapView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "ProjectView.h"


/*
BOOL CProjectView::ProcessWindowMessage(_In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam,	_In_ LPARAM lParam, _Inout_ LRESULT& lResult, _In_ DWORD dwMsgMapID = 0);
{

}
*/


BOOL CProjectView::PreTranslateMessage(MSG* pMsg)
{
    pMsg;
    return FALSE;
}

LRESULT CProjectView::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    ::PostQuitMessage(0);
    return 0;
}

LRESULT CProjectView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{

    BOOL    bHandled;



    SciterProcND(m_hWnd, WM_CREATE, 0, 0, &bHandled);

    ::SciterSetOption(m_hWnd, SCITER_SET_SCRIPT_RUNTIME_FEATURES,
                      ALLOW_FILE_IO |
                      ALLOW_SOCKET_IO |
                      ALLOW_EVAL |
                      ALLOW_SYSINFO |
                      ALLOW_CMODULES );

    this->setup_callback(); // attach sciter::host callbacks
    sciter::attach_dom_event_handler(m_hWnd,this); // attach this as a DOM events

    // load intial document
    bool bLoad = this->load_file(L"res:default.htm");

    return S_OK;
}

LRESULT CProjectView::OnMessageForward(UINT uMsg, WPARAM wParam, LPARAM lParam,  BOOL& bHandled)
{
    // Forward to Sciter's message processor
    LRESULT lResult = SciterProcND(m_hWnd, uMsg, wParam, lParam, &bHandled);
    if (bHandled)
        return lResult; // Sciter handled it

    return DefWindowProc(uMsg, wParam, lParam);
}




HWINDOW   CProjectView::get_hwnd() { return this->m_hWnd; }

bool CProjectView::handle_mouse  (HELEMENT he, MOUSE_PARAMS& params )
{
    return false;
}

bool CProjectView::handle_key    (HELEMENT he, KEY_PARAMS& params )
{
    return false;
}

bool CProjectView::handle_focus  (HELEMENT he, FOCUS_PARAMS& params )
{
    return false;
}

bool CProjectView::handle_event (HELEMENT he, BEHAVIOR_EVENT_PARAMS& params )
{
    sciter::dom::element target(params.heTarget);
    switch( params.cmd ) {
        case BUTTON_CLICK:                                 // click on button
        {
            auto bname = target.get_attribute("id");
            if(bname == L"open")
            {
             //   ::MessageBox(m_hWnd, L"Open", L"Open", MB_OK);
                return true;
            }
            break;
        }
    }

return true;
}

bool CProjectView::handle_method_call (HELEMENT he, METHOD_PARAMS& params )
{
return false;
}

HINSTANCE CProjectView::get_resource_instance() { return _Module.m_hInst; }

bool  CProjectView::on_script_call(LPCSTR name, const sciter::value* args, UINT argc, sciter::value& result)
{
if (strcmp(name, "sayHello") == 0 && argc == 1) {
std::wstring fromJS = args[0].get(WSTR(""));
std::wstring message = L"Hello from native C++ to " + fromJS;
result = sciter::value(message.c_str());
return true;
}
return false;
}

