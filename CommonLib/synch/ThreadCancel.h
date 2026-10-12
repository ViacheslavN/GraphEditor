#pragma once

#include <functional>

namespace CommonLib
{
	namespace synch
	{
		// The cancel of the work of the current thread, seen by code deep below which has no cancel parameter
		// (f.e. the SQLite progress handler interrupts a long sqlite3_step: a query reading many pages from a HDD).
		// While a scope is alive, Continue() of this thread asks its function; the scopes nest, other threads
		// aren't affected.
		class CThreadCancelScope
		{
		public:
			explicit CThreadCancelScope(std::function<bool()> fnContinue) :
					m_fnContinue(std::move(fnContinue)), m_pPrev(Current())
			{
				Current() = this;
			}

			~CThreadCancelScope()
			{
				Current() = m_pPrev;
			}

			// false - the work of the current thread is canceled
			static bool Continue()
			{
				for(CThreadCancelScope* pScope = Current(); pScope; pScope = pScope->m_pPrev)
				{
					if(pScope->m_fnContinue && !pScope->m_fnContinue())
						return false;
				}
				return true;
			}

		private:
			CThreadCancelScope(const CThreadCancelScope&);
			CThreadCancelScope& operator=(const CThreadCancelScope&);

			static CThreadCancelScope*& Current()
			{
				thread_local CThreadCancelScope* pCurrent = nullptr;
				return pCurrent;
			}

		private:
			std::function<bool()> m_fnContinue;
			CThreadCancelScope*   m_pPrev;
		};
	}
}
