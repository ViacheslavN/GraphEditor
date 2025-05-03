
#include "BaseWindow.h"
#include <WinUser.h>
#include "../../CommonLib/exception/WinExc.h"

namespace GraphEngine {
    namespace UILib {
        namespace Windows {


            IBaseWindow::IBaseWindow() : m_pfnSuperWindowProc(::DefWindowProc), m_hWnd(0)
            {

            }

            IBaseWindow::~IBaseWindow()
            {

            }

            int64_t IBaseWindow::GetNativeHandle() const
            {
                return (int64_t)m_hWnd;
            }

            void IBaseWindow::ShowWindow(bool bShow)
            {
                ::ShowWindow(m_hWnd, bShow ? SW_SHOW : SW_HIDE);
            }

            WNDPROC IBaseWindow::GetWindowProc()
            {
                return WindowProc;
            }

            void IBaseWindow::RegisterClass(const std::wstring& className, HINSTANCE hInst)
            {
                m_wndClass.cbSize = sizeof(m_wndClass);
                if (! ::GetClassInfoEx(hInst, className.c_str(), &m_wndClass) )
                {
                    memset(&m_wndClass, 0,sizeof(m_wndClass));
                    m_wndClass.cbSize = sizeof(m_wndClass);
                    m_wndClass.lpfnWndProc   = StartWndProc;
                    m_wndClass.cbClsExtra    = 0;
                    m_wndClass.cbWndExtra    = 0;
                    m_wndClass.hInstance     = NULL;
                    m_wndClass.hIcon         = NULL;
                    m_wndClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
                    m_wndClass.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);//(COLOR_WINDOW + 1);
                    m_wndClass.lpszMenuName  = NULL;
                    m_wndClass.style          = CS_HREDRAW | CS_VREDRAW;

                    if(!className.empty())
                        m_wndClass.lpszClassName = className.c_str();
                    else
                    {
                        m_GeneretedClassName = CommonLib::str_format::WStrFormatSafeT(L"BaseWindow_{0}", (int64_t )&m_wndClass);
                        m_wndClass.lpszClassName = m_GeneretedClassName.c_str();
                    }
                    m_wndClass.hIconSm = NULL;
                   m_atom = RegisterClassEx(&m_wndClass);
                   if(m_atom == 0)
                       throw CommonLib::CWinExc("Failed to registerClass");
                }
            }


            void IBaseWindow::Create(DWORD dwExStyle, const std::wstring& className,  const std::wstring& windowName, DWORD dwStyle,
                                int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInst,  LPVOID lpParam )
            {
                try
                {
                    RegisterClass(className, hInst);

                    CWndObjectHolder::Instance().AddObject(this);


                   m_hWnd = ::CreateWindowEx(dwExStyle, MAKEINTATOM(m_atom), windowName.c_str(),
                                                 dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu,
                                                 hInst, lpParam);
                   if(m_hWnd == NULL)
                       throw CommonLib::CWinExc("HWND is Null");

                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExc("IBaseWindow: Failed to create window", exc);
                }

            }


            LRESULT CALLBACK IBaseWindow::StartWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
            {

                IBaseWindow* pThis =  (IBaseWindow*)CWndObjectHolder::Instance().GetObject();
                if(pThis == nullptr)
                    return ::DefWindowProc(hWnd, uMsg, wParam, lParam);

                pThis->m_hWnd = hWnd;
                pThis->m_thunk.Init(pThis->GetWindowProc(), pThis);
                WNDPROC pProc = pThis->m_thunk.GetWNDPROC();
                WNDPROC pOldProc = (WNDPROC)::SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)pProc);

                return pProc(hWnd, uMsg, wParam, lParam);
            }

            LRESULT CALLBACK IBaseWindow::WindowProc( HWND hWnd, UINT uMsg,  WPARAM wParam, LPARAM lParam)
            {
                IBaseWindow* pThis =( IBaseWindow* )hWnd;
                return pThis->ProcessWindowMessage(hWnd, uMsg, wParam, lParam);
            }

            LRESULT  IBaseWindow::DefWindowProc(
                    _In_ UINT uMsg,
                    _In_ WPARAM wParam,
                    _In_ LPARAM lParam)
                    {

                return ::CallWindowProc(m_pfnSuperWindowProc, this->m_hWnd, uMsg, wParam, lParam);
            }


        }}
        }