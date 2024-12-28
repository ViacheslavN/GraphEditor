#pragma once

#include "exc_base.h"

namespace CommonLib
{
	class CWinExc : public CExcBase
	{
		public:
			CWinExc(DWORD error = ::GetLastError());
			CWinExc(const CWinExc& exc);
			CWinExc(const astr& msg, DWORD error = ::GetLastError());
			CWinExc(const wstr& msg, DWORD error = ::GetLastError());

			template<class TArg>
			CWinExc(const astr& format, const TArg& arg, DWORD error = ::GetLastError()) : CExcBase(format, arg), m_error(error)
			{
				AddMsg(GetErrorMessageA(m_error));
			}

			template<class TArg1, class TArg2>
			CWinExc(const astr& format, const TArg1& arg1, const TArg2& arg2, DWORD error = ::GetLastError()) : CExcBase(format, arg1, arg2), m_error(error)
			{
				AddMsg(GetErrorMessageA(m_error));
			}

			template<class TArg1, class TArg2, class TArg3>
			CWinExc(const astr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3, DWORD error = ::GetLastError()) : CExcBase(format, arg1, arg2, arg3), m_error(error)
			{
				AddMsg(GetErrorMessageA(m_error));
			}

			template<class TArg1, class TArg2, class TArg3, class TArg4>
			CWinExc(const astr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3, const TArg4& arg4, DWORD error = ::GetLastError()) : CExcBase(format, arg1, arg2, arg3, arg4), m_error(error)
			{
				AddMsg(GetErrorMessageA(m_error));
			}


			template<class TArg>
			CWinExc(const wstr& format, const TArg& arg, DWORD error = ::GetLastError()) : CExcBase(format, arg), m_error(error)
			{
				AddMsg(GetErrorMessageA(m_error));
			}
					

			template<class TArg1, class TArg2>
			CWinExc(const wstr& format, const TArg1& arg1, const TArg2& arg2, DWORD error = ::GetLastError()) : CExcBase(format, arg1, arg2), m_error(error)
			{
				AddMsg(GetErrorMessageA(m_error));
			}

					   			 
			virtual ~CWinExc();

			virtual std::shared_ptr<CExcBase> Clone() const;
			DWORD  GetErrCode() const;

			static bool GetSysMessage(DWORD errCode, astr& result);
			static bool GetWinInetMessage(DWORD errCode, astr& result);

			static astr GetErrorMessageA(DWORD errCode);
			static wstr GetErrorMessageW(DWORD errCode);

			virtual void AddMsg(DWORD error = ::GetLastError());
			virtual void AddMsg(const astr& msg, DWORD error = ::GetLastError());
			virtual void AddMsg(const astr& format, const astr& msg, DWORD error = ::GetLastError());
			virtual void AddMsg(const astr& format, const astr& msg1, const astr& msg2, DWORD error = ::GetLastError());
			virtual void AddMsg(const astr& format, const astr& msg1, const astr& msg2, const astr& msg3, DWORD error = ::GetLastError());
			virtual void AddMsg(const astr& format, const astr& msg1, const astr& msg2, const astr& msg3, const astr& msg4, DWORD error = ::GetLastError());

			virtual void AddMsg(const wstr& msg, DWORD error = ::GetLastError());
			virtual void AddMsg(const wstr& format, const wstr& msg, DWORD error = ::GetLastError());
			virtual void AddMsg(const wstr& format, const wstr& msg1, const wstr& msg2, DWORD error = ::GetLastError());
			virtual void AddMsg(const wstr& format, const wstr& msg1, const wstr& msg2, const wstr& msg3, DWORD error = ::GetLastError());
			virtual void AddMsg(const wstr& format, const wstr& msg1, const wstr& msg2, const wstr& msg3, const wstr& msg4, DWORD error = ::GetLastError());

		protected:
			DWORD m_error;
	};
}