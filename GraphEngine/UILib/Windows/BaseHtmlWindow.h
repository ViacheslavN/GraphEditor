#include "BaseWindow.h"
#include "sciter-x.h"

namespace GraphEngine {
    namespace UILib {
        namespace Windows {

            class CBaseHtmlWindow : public IBaseWindow,
                                    public sciter::host<CBaseHtmlWindow>, // Sciter host window primitives
                                public sciter::event_handler_raw       // Sciter DOM event handling
            {
            public:
                CBaseHtmlWindow();
                virtual ~CBaseHtmlWindow();


                HWINDOW   get_hwnd();
                virtual HINSTANCE get_resource_instance() = 0;
                virtual bool handle_mouse  (HELEMENT he, MOUSE_PARAMS& params ) override;
                virtual bool handle_key    (HELEMENT he, KEY_PARAMS& params ) override;
                virtual bool handle_focus  (HELEMENT he, FOCUS_PARAMS& params )  override;
                virtual bool handle_event (HELEMENT he, BEHAVIOR_EVENT_PARAMS& params ) override;
                virtual bool handle_method_call (HELEMENT he, METHOD_PARAMS& params )  override;
                bool on_script_call(LPCSTR name, const sciter::value* args, UINT argc, sciter::value& result);

                virtual int64_t GetNativeHandle() const;

                virtual void LoadHtml(const std::wstring& html);
                virtual void LoadFromResource(const std::wstring& resource) ;

            protected:

                virtual LRESULT CALLBACK ProcessWindowMessage(HWND, UINT, WPARAM, LPARAM);

                virtual bool HandleMouseImpl  (HELEMENT he, MOUSE_PARAMS& params );
                virtual bool HandleKeyImpl  (HELEMENT he, KEY_PARAMS& params );
                virtual bool HandleFocusImpl  (HELEMENT he, FOCUS_PARAMS& params );
                virtual bool HandleEventImpl  (HELEMENT he, BEHAVIOR_EVENT_PARAMS& params );
                virtual bool HandleMethodCallImpl  (HELEMENT he, METHOD_PARAMS& params );
                virtual bool HandleOnScriptCallImpl  (LPCSTR name, const sciter::value* args, UINT argc, sciter::value& result);

            };

        }}
        }