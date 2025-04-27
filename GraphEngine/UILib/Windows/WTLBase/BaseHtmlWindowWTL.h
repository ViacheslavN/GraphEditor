#pragma once
#include "../WTLHeaders.h"
#include "../../UILib.h"
#include "sciter-x.h"
#include <vector>




namespace GraphEngine {
    namespace UILib {


#define SCITER_MSG_MAP() \
	{                       \
        BOOL    bHandled;\
               \
		lResult = OnMessageForward( uMsg, wParam, lParam, bHandled) ;\
            if(bHandled)             \
			return TRUE; \
	}


        class CBaseHtmlWindowsWTL : public CWindowImpl<CBaseHtmlWindowsWTL>,
                                    public sciter::host<CBaseHtmlWindowsWTL>, // Sciter host window primitives
                                    public sciter::event_handler_raw ,      // Sciter DOM event handling
                                    public IHtmlWindow
        {
            DECLARE_WND_CLASS(NULL)

            BOOL PreTranslateMessage(MSG* pMsg);

            BEGIN_MSG_MAP(CBaseHtmlWindowsWTL)
                        MESSAGE_HANDLER(WM_CREATE, OnCreate)
                        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
                        SCITER_MSG_MAP()
            END_MSG_MAP()



            LRESULT OnMessageForward(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
            LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
            LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);


            HWINDOW   get_hwnd();
            virtual HINSTANCE get_resource_instance() = 0;
            virtual bool handle_mouse  (HELEMENT he, MOUSE_PARAMS& params ) override;
            virtual bool handle_key    (HELEMENT he, KEY_PARAMS& params ) override;
            virtual bool handle_focus  (HELEMENT he, FOCUS_PARAMS& params )  override;
            virtual bool handle_event (HELEMENT he, BEHAVIOR_EVENT_PARAMS& params ) override;
            virtual bool handle_method_call (HELEMENT he, METHOD_PARAMS& params )  override;
            bool on_script_call(LPCSTR name, const sciter::value* args, UINT argc, sciter::value& result);

            virtual int64_t GetNativeHandle() const;
            virtual void CreateFromHtml(const std::string& html, IWindowPtr ptrParentWindow = IWindowPtr(), uint64_t flags = 0, uint64_t flagsEx = 0);
            virtual void CreateFromResource(const std::string& resource, IWindowPtr ptrParentWindow = IWindowPtr(), uint64_t flags = 0, uint64_t flagsEx = 0);

        protected:

            virtual bool HandleMouseImpl  (HELEMENT he, MOUSE_PARAMS& params );
            virtual bool HandleKeyImpl  (HELEMENT he, KEY_PARAMS& params );
            virtual bool HandleFocusImpl  (HELEMENT he, FOCUS_PARAMS& params );
            virtual bool HandleEventImpl  (HELEMENT he, BEHAVIOR_EVENT_PARAMS& params );
            virtual bool HandleMethodCallImpl  (HELEMENT he, METHOD_PARAMS& params );
            virtual bool HandleOnScriptCallImpl  (LPCSTR name, const sciter::value* args, UINT argc, sciter::value& result);
        private:

        };

    }
}