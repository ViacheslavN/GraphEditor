#include "../UILib.h"
#include "WndProcThunk.h"

namespace GraphEngine {
    namespace UILib {
        namespace Windows {


            class IMessageFilter
            {
            public:
                IMessageFilter(){}
                virtual ~IMessageFilter(){}
                virtual BOOL PreTranslateMessage(MSG* pMsg) = 0;
            };

            class IIdleHandler
            {
            public:
                IIdleHandler(){}
                virtual ~IIdleHandler(){}
                virtual BOOL OnIdle() = 0;
            };

            class IBaseWindow : public IWindow
            {
            public:
                IBaseWindow();
                virtual ~IBaseWindow();

                virtual int64_t GetNativeHandle() const;
                virtual void ShowWindow(bool bShow);
                virtual void Create(DWORD dwExStyle, const std::wstring& className,  const std::wstring& name, DWORD dwStyle,
                                      int x, int y, int nWidth, int nHeight, HWND hParent, HMENU hMenu, HINSTANCE hInst,  LPVOID lpParam);

            protected:
                virtual WNDPROC GetWindowProc();
                static LRESULT CALLBACK	StartWndProc( HWND hWnd, UINT uMsg,  WPARAM wParam, LPARAM lParam);
                static LRESULT CALLBACK WindowProc( HWND hWnd, UINT uMsg,  WPARAM wParam, LPARAM lParam);


                virtual LRESULT CALLBACK ProcessWindowMessage(HWND, UINT, WPARAM, LPARAM) = 0;

                void RegisterClass(const std::wstring& className, HINSTANCE hInst);
                LRESULT DefWindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
            protected:
                CWndProcTrunk m_thunk;
                HWND  m_hWnd;
                WNDCLASSEX m_wndClass;
                ATOM m_atom;
                std::wstring m_GeneretedClassName;
                WNDPROC m_pfnSuperWindowProc;
            };


        }
    }
    }