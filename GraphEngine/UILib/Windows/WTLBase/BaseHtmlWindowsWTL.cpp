#include "../WTLHeaders.h"
#include "BaseHtmlWindowWTL.h"


namespace GraphEngine {
    namespace UILib {



        BOOL CBaseHtmlWindowsWTL::PreTranslateMessage(MSG* pMsg)
        {
            pMsg;
            return FALSE;
        }

        LRESULT CBaseHtmlWindowsWTL::OnMessageForward(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
        {

            // Forward to Sciter's message processor
            LRESULT lResult = SciterProcND(m_hWnd, uMsg, wParam, lParam, &bHandled);
                if (bHandled)
                    return lResult; // Sciter handled it

           return CWindowImpl<CBaseHtmlWindowsWTL>::DefWindowProc(uMsg, wParam, lParam);
        }

        LRESULT CBaseHtmlWindowsWTL::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
        {
            ::PostQuitMessage(0);
            return S_OK;
        }

        LRESULT CBaseHtmlWindowsWTL::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
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
            return S_OK;
        }

        HWINDOW   CBaseHtmlWindowsWTL::get_hwnd()
        {
            return (HWINDOW)m_hWnd;
        }



        bool CBaseHtmlWindowsWTL::handle_mouse  (HELEMENT he, MOUSE_PARAMS& params )
        {
            return HandleMouseImpl(he, params);
        }

        bool CBaseHtmlWindowsWTL::handle_key    (HELEMENT he, KEY_PARAMS& params )
        {
            return HandleKeyImpl(he, params);
        }

        bool CBaseHtmlWindowsWTL::handle_focus  (HELEMENT he, FOCUS_PARAMS& params )
        {
            return HandleFocusImpl(he, params);
        }

        bool CBaseHtmlWindowsWTL::handle_event (HELEMENT he, BEHAVIOR_EVENT_PARAMS& params )
        {
            return HandleEventImpl(he, params);
        }

        bool CBaseHtmlWindowsWTL::handle_method_call (HELEMENT he, METHOD_PARAMS& params )
        {
            return HandleMethodCallImpl(he, params);
        }

        bool CBaseHtmlWindowsWTL::on_script_call(LPCSTR name, const sciter::value* args, UINT argc, sciter::value& result)
        {
            return HandleOnScriptCallImpl(name, args,argc,result);
        }

        int64_t CBaseHtmlWindowsWTL::GetNativeHandle() const
        {
            return (int64_t )m_hWnd;
        }

        void CBaseHtmlWindowsWTL::LoadHtml(const std::wstring& html)
        {

        }

        void CBaseHtmlWindowsWTL::LoadFromResource(const std::wstring& resource)
        {
            bool bLoad = this->load_file(resource.c_str());
        }

        bool CBaseHtmlWindowsWTL::HandleMouseImpl  (HELEMENT he, MOUSE_PARAMS& params )
        {
            return false;
        }

        bool CBaseHtmlWindowsWTL::HandleKeyImpl  (HELEMENT he, KEY_PARAMS& params )
        {
            return false;
        }

        bool CBaseHtmlWindowsWTL::HandleFocusImpl  (HELEMENT he, FOCUS_PARAMS& params )
        {
            return false;
        }

        bool CBaseHtmlWindowsWTL::HandleEventImpl  (HELEMENT he, BEHAVIOR_EVENT_PARAMS& params )
        {
            return false;
        }

        bool CBaseHtmlWindowsWTL::HandleMethodCallImpl  (HELEMENT he, METHOD_PARAMS& params )
        {
            return false;
        }

        bool CBaseHtmlWindowsWTL::HandleOnScriptCallImpl  (LPCSTR name, const sciter::value* args, UINT argc, sciter::value& result)
        {
            return false;
        }

    }
}