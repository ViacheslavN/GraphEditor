#include "BaseHtmlWindow.h"

namespace GraphEngine {
    namespace UILib {
        namespace Windows {


            CBaseHtmlWindow::CBaseHtmlWindow()
            {

            }

            CBaseHtmlWindow::~CBaseHtmlWindow()
            {}


            HWINDOW   CBaseHtmlWindow::get_hwnd()
            {
                return (HWINDOW)m_hWnd;
            }

            LRESULT CALLBACK CBaseHtmlWindow::ProcessWindowMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
            {

                if(uMsg == WM_CREATE)
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
                    sciter::attach_dom_event_handler(m_hWnd, this); // attach this as a DOM events
                    return S_OK;
                }

                if(uMsg == WM_DESTROY)
                {
                    ::PostQuitMessage(0);
                    return S_OK;
                }

                // Forward to Sciter's message processor
                BOOL bHandled;
                LRESULT lResult = SciterProcND(m_hWnd, uMsg, wParam, lParam, &bHandled);
                if (bHandled)
                    return lResult; // Sciter handled it

                return DefWindowProc(uMsg, wParam, lParam);
            }


            bool CBaseHtmlWindow::handle_mouse  (HELEMENT he, MOUSE_PARAMS& params )
            {
                return HandleMouseImpl(he, params);
            }

            bool CBaseHtmlWindow::handle_key    (HELEMENT he, KEY_PARAMS& params )
            {
                return HandleKeyImpl(he, params);
            }

            bool CBaseHtmlWindow::handle_focus  (HELEMENT he, FOCUS_PARAMS& params )
            {
                return HandleFocusImpl(he, params);
            }

            bool CBaseHtmlWindow::handle_event (HELEMENT he, BEHAVIOR_EVENT_PARAMS& params )
            {
                return HandleEventImpl(he, params);
            }

            bool CBaseHtmlWindow::handle_method_call (HELEMENT he, METHOD_PARAMS& params )
            {
                return HandleMethodCallImpl(he, params);
            }

            bool CBaseHtmlWindow::on_script_call(LPCSTR name, const sciter::value* args, UINT argc, sciter::value& result)
            {
                return HandleOnScriptCallImpl(name, args,argc,result);
            }

            int64_t CBaseHtmlWindow::GetNativeHandle() const
            {
                return (int64_t )m_hWnd;
            }

            void CBaseHtmlWindow::LoadHtml(const std::wstring& html)
            {

            }

            void CBaseHtmlWindow::LoadFromResource(const std::wstring& resource)
            {
                bool bLoad = this->load_file(resource.c_str());
            }

            bool CBaseHtmlWindow::HandleMouseImpl  (HELEMENT he, MOUSE_PARAMS& params )
            {
                return false;
            }

            bool CBaseHtmlWindow::HandleKeyImpl  (HELEMENT he, KEY_PARAMS& params )
            {
                return false;
            }

            bool CBaseHtmlWindow::HandleFocusImpl  (HELEMENT he, FOCUS_PARAMS& params )
            {
                return false;
            }

            bool CBaseHtmlWindow::HandleEventImpl  (HELEMENT he, BEHAVIOR_EVENT_PARAMS& params )
            {
                return false;
            }

            bool CBaseHtmlWindow::HandleMethodCallImpl  (HELEMENT he, METHOD_PARAMS& params )
            {
                return false;
            }

            bool CBaseHtmlWindow::HandleOnScriptCallImpl  (LPCSTR name, const sciter::value* args, UINT argc, sciter::value& result)
            {
                return false;
            }

        }}
        }