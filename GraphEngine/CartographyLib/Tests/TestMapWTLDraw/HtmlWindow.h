// TestGraphicsView.h : interface of the CMapView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include "sciter-x.h"
#include <vector>



#define SCITER_MSG_MAP() \
	{                       \
        BOOL    bHandled;\
               \
		lResult = OnMessageForward( uMsg, wParam, lParam, bHandled) ;\
            if(bHandled)             \
			return TRUE; \
	}


class CHtmlViewBase : public CWindowImpl<CHtmlViewBase>,
                     public sciter::host<CHtmlViewBase>, // Sciter host window primitives
                     public sciter::event_handler_raw       // Sciter DOM event handling
{
public:
    DECLARE_WND_CLASS(NULL)


    BEGIN_MSG_MAP(CHtmlViewBase)
    MESSAGE_HANDLER(WM_CREATE, OnCreate)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    SCITER_MSG_MAP()
    ALT_MSG_MAP( 1 )
    END_MSG_MAP()


    LRESULT OnMessageForward(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
    LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
    LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);



    HWINDOW   get_hwnd();
    HINSTANCE get_resource_instance();
    virtual bool handle_mouse  (HELEMENT he, MOUSE_PARAMS& params ) override;
    virtual bool handle_key    (HELEMENT he, KEY_PARAMS& params ) override;
    virtual bool handle_focus  (HELEMENT he, FOCUS_PARAMS& params )  override;
    virtual bool handle_event (HELEMENT he, BEHAVIOR_EVENT_PARAMS& params ) override;
    virtual bool handle_method_call (HELEMENT he, METHOD_PARAMS& params )  override;
    virtual bool on_script_call(LPCSTR name, const sciter::value* args, UINT argc, sciter::value& result);
private:

};
