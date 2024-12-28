#pragma once
#include <exception>
#include "../synch/thread.h"
#include "../str/StringEncoding.h"
#include "../str/str.h"

namespace CommonLib
{

class CExcBase : public std::exception
{
public:
	CExcBase() noexcept;
	CExcBase(const CExcBase& value) noexcept;
	CExcBase(const astr& msg) noexcept;
	CExcBase(const wstr& msg) noexcept;

	template<class TArg>
	CExcBase(const astr& format, const TArg& arg) : m_srcThreadId(synch::CThread::GetCurThreadId())
	{
		AddMsgT(format, arg);
	}

	template<class TArg1, class TArg2>
	CExcBase(const astr& format, const TArg1& arg1, const TArg2& arg2) : m_srcThreadId(synch::CThread::GetCurThreadId())
	{
		AddMsgT(format, arg1, arg2);
	}

	template<class TArg1, class TArg2, class TArg3>
	CExcBase(const astr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3) : m_srcThreadId(synch::CThread::GetCurThreadId())
	{
		AddMsgT(format, arg1, arg2, arg3);
	}

	template<class TArg1, class TArg2, class TArg3, class TArg4>
	CExcBase(const astr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3, const TArg4& arg4) : m_srcThreadId(synch::CThread::GetCurThreadId())
	{
		AddMsgT(format, arg1, arg2, arg3, arg4);
	}
	

	template<class TArg>
	CExcBase(const wstr& format, const TArg& arg) : m_srcThreadId(synch::CThread::GetCurThreadId())
	{
		AddMsgT(format, arg);
	}

	template<class TArg1, class TArg2>
	CExcBase(const wstr& format, const TArg1& arg1, const TArg2& arg2) : m_srcThreadId(synch::CThread::GetCurThreadId())
	{
		AddMsgT(format, arg1, arg2);
	}

	template<class TArg1, class TArg2, class TArg3>
	CExcBase(const wstr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3) : m_srcThreadId(synch::CThread::GetCurThreadId())
	{
		AddMsgT(format, arg1, arg2, arg3);
	}


	virtual ~CExcBase() noexcept;

	virtual std::shared_ptr<CExcBase> Clone() const;
	virtual const char*  what() const noexcept;
	virtual void Throw() const;
	
	const astrvec& GetChain() const;

	virtual void AddMsg(const astr& msg);
	virtual void AddMsg(const astr& format, const astr& msg);
	virtual void AddMsg(const astr& format, const astr& msg1, const astr& msg2);
	virtual void AddMsg(const astr& format, const astr& msg1, const astr& msg2, const astr& msg3);
	virtual void AddMsg(const astr& format, const astr& msg1, const astr& msg2, const astr& msg3, const astr& msg4);
	   
	template<class TArg>
	void AddMsgT(const astr& format, const TArg& arg1)
	{
		return AddMsg(format, str_utils::AStrFrom(arg1));
	}

	template<class TArg1, class TArg2>
	void AddMsgT(const astr& format, const TArg1& arg1, const TArg2& arg2)
	{
		return AddMsg(format, str_utils::AStrFrom(arg1), str_utils::AStrFrom(arg2));
	}

	template<class TArg1, class TArg2, class TArg3>
	void AddMsgT(const astr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3)
	{
		return AddMsg(format, str_utils::AStrFrom(arg1), str_utils::AStrFrom(arg2), str_utils::AStrFrom(arg3));
	}

	template<class TArg1, class TArg2, class TArg3, class TArg4>
	void AddMsgT(const astr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3, const TArg4& arg4)
	{
		return AddMsg(format, str_utils::AStrFrom(arg1), str_utils::AStrFrom(arg2), str_utils::AStrFrom(arg3), str_utils::AStrFrom(arg4));
	}




	template<class TArg>
	void AddMsgT(const wstr& format, const TArg& arg1)
	{
		return AddMsg(StringEncoding::str_w2utf8_safe(format), str_utils::AStrFrom(arg1));
	}

	template<class TArg1, class TArg2>
	void AddMsgT(const wstr& format, const TArg1& arg1, const TArg2& arg2)
	{
		return AddMsg(StringEncoding::str_w2utf8_safe(format), str_utils::AStrFrom(arg1), str_utils::AStrFrom(arg2));
	}

	template<class TArg1, class TArg2, class TArg3>
	void AddMsgT(const wstr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3)
	{
		return AddMsg(StringEncoding::str_w2utf8_safe(format), str_utils::AStrFrom(arg1), str_utils::AStrFrom(arg2), str_utils::AStrFrom(arg3));
	}

	template<class TArg1, class TArg2, class TArg3, class TArg4>
	void AddMsgT(const wstr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3, const TArg4& arg4)
	{
		return AddMsg(StringEncoding::str_w2utf8_safe(format), str_utils::AStrFrom(arg1), str_utils::AStrFrom(arg2), str_utils::AStrFrom(arg3), str_utils::AStrFrom(arg4));
	}


	virtual void AddMsg(const wstr& msg);
	virtual void AddMsg(const wstr& format, const wstr& msg);
	virtual void AddMsg(const wstr& format, const wstr& msg1, const wstr& msg2);
	virtual void AddMsg(const wstr& format, const wstr& msg1, const wstr& msg2, const wstr& msg3);
	virtual void AddMsg(const wstr& format, const wstr& msg1, const wstr& msg2, const wstr& msg3, const wstr& msg4);

	template<class TArg>
	static void RegenExcT( const TArg& arg, std::exception& exc_src)
	{
		return RegenExc(str_utils::AStrFrom(arg), exc_src);
	}
	
	template<class TArg>
	static void RegenExcT(const astr& format, const TArg& arg1, std::exception& exc_src)
	{
		return RegenExc(format, str_utils::AStrFrom(arg1), exc_src);
	}

	template<class TArg1, class TArg2>
	static void RegenExcT(const astr& format, const TArg1& arg1, const TArg2& arg2, std::exception& exc_src)
	{
		return RegenExc(format, str_utils::AStrFrom(arg1), str_utils::AStrFrom(arg2), exc_src);
	}

	template<class TArg1, class TArg2, class TArg3>
	static void RegenExcT(const astr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3, std::exception& exc_src)
	{
		return RegenExc(format, str_utils::AStrFrom(arg1), str_utils::AStrFrom(arg2), str_utils::AStrFrom(arg3), exc_src);
	}

	template<class TArg1, class TArg2, class TArg3, class TArg4>
	static void RegenExcT(const astr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3, const TArg4& arg4, std::exception& exc_src)
	{
		return RegenExc(format, str_utils::AStrFrom(arg1), str_utils::AStrFrom(arg2), str_utils::AStrFrom(arg3), str_utils::AStrFrom(arg4), exc_src);
	}

	static void RegenExc(const astr& msg, std::exception& exc_src);
	static void RegenExc(const astr& format, const astr& msg, std::exception& exc_src);
	static void RegenExc(const astr& format, const astr& msg1, const astr& msg2, std::exception& exc_src);
	static void RegenExc(const astr& format, const astr& msg1, const astr& msg2, const astr& msg3, std::exception& exc_src);
	static void RegenExc(const astr& format, const astr& msg1, const astr& msg2, const astr& msg3, const astr& msg4, std::exception& exc_src);
	static astr GetErrorDesc(const std::exception& exc);
	static std::shared_ptr<CExcBase> CloneFromExc(const std::exception& exc);
	static astrvec GetChainFromExc(const std::exception& exc);
protected:
	astrvec m_msgChain;
	thread_id_t m_srcThreadId;
	mutable astr m_what;

};

}