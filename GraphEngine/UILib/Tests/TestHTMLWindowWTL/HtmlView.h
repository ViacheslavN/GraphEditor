// TestGraphicsView.h : interface of the CMapView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include "../../UILib/Windows/WTLBase/BaseHtmlWindowWTL.h"

class CHtmlView : public GraphEngine::UILib::CBaseHtmlWindowsWTL
{
public:
    CHtmlView();
    ~CHtmlView();


    virtual HINSTANCE get_resource_instance() ;

};
