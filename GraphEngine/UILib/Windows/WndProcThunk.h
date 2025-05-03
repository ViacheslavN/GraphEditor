
#include "../UILib.h"

#include <atlstdthunk.h>

namespace GraphEngine {
    namespace UILib {
        namespace Windows {

        class CWndProcTrunk
        {
        public:
            ATL::CStdCallThunk thunk;
            BOOL Init( _In_opt_ WNDPROC proc,  _In_opt_ void* pThis);
            WNDPROC GetWNDPROC();

        };


        class CWndObjectHolder
        {

            CWndObjectHolder() = default;
        public:
            static CWndObjectHolder& Instance();

            void AddObject(void *pObject);
            void* GetObject();

        private:
            std::mutex m_mutex;

            typedef std::stack<void *> TStackObject;
            typedef std::map<DWORD, TStackObject> TMapObjectsByThreadId;

            TMapObjectsByThreadId m_mapObjects;

        };


        }
    }
}