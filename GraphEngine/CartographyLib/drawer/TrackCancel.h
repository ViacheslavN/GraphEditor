#pragma once
#include "../Cartography.h"

namespace GraphEngine {
    namespace Cartography {

        // thread safe cancel flag for long drawing operations
        class CTrackCancel : public Display::ITrackCancel
        {
        public:
            CTrackCancel() : m_bContinue(true)
            {}
            virtual ~CTrackCancel()
            {}

            virtual void Cancel()
            {
                m_bContinue = false;
            }

            virtual bool Continue()
            {
                return m_bContinue;
            }

            virtual void Reset()
            {
                m_bContinue = true;
            }

        private:
            std::atomic<bool> m_bContinue;
        };

        typedef std::shared_ptr<CTrackCancel> CTrackCancelPtr;
    }
}
