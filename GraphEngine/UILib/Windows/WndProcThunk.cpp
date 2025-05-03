#include "WndProcThunk.h"
#include "../../CommonLib/exception/exc_base.h"

namespace GraphEngine {
    namespace UILib {
        namespace Windows {

            BOOL CWndProcTrunk::Init(
                    _In_opt_ WNDPROC proc,
                    _In_opt_ void* pThis)
            {
                return thunk.Init((DWORD_PTR)proc, pThis);
            }

            WNDPROC CWndProcTrunk::GetWNDPROC()
            {
                return (WNDPROC)thunk.GetCodeAddress();
            }


            CWndObjectHolder& CWndObjectHolder::Instance()
            {
                static CWndObjectHolder object;
                return  object;
            }

            void CWndObjectHolder::AddObject(void *pObject)
            {
                std::lock_guard lock(m_mutex);

                TStackObject& stackObject = m_mapObjects[GetCurrentThreadId()];
                stackObject.push(pObject);

            }

            void* CWndObjectHolder::GetObject()
            {
                std::lock_guard  lock(m_mutex);
                TStackObject& stackObject = m_mapObjects[GetCurrentThreadId()];
                if(stackObject.empty())
                    return nullptr;

                void *pObject = stackObject.top();
                stackObject.pop();

                return  pObject;

            }


        }
    }
}