#pragma

#include "../../UILib/Windows/BaseHtmlWindow.h"

class CHtmlWindow : public GraphEngine::UILib::Windows::CBaseHtmlWindow
{
    public:
        CHtmlWindow(HINSTANCE hInstance);
    virtual HINSTANCE get_resource_instance() {return  m_hInstance;}
private:
    HINSTANCE m_hInstance;
};