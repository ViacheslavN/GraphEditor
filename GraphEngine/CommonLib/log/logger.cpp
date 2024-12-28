#include "stdafx.h"
#include "logger.h"
#include "../str/str.h"
#include "LogLinePerformer.h"
#include "../exception/exc_base.h"
#include "../filesystem/File.h"
#include "../filesystem/PathUtils.h"
#include "../filesystem/filesystem.h"
 

//CommonLib::CLogger& Log = *(new CommonLib::CLogger());

CommonLib::CLogger Log;

namespace CommonLib
{	   

	CLogger::CLogger() 
  	{}

	CLogger::~CLogger()
	{
		
	}

	
	void CLogger::Msg(eMsgType type, const astr& msg)
	{
		if (m_logLevel > type)
			return;

		if (m_pLogLineFormat.get() == nullptr)
			return;

		astr outMsg;
		m_pLogLineFormat->FormatLogline(msg, type, outMsg);

		MessageStr(outMsg);
	}

	void CLogger::MessageStr(const astr& msg)
	{
		if (m_pLogWriter.get() == nullptr)
			return;

		m_pLogWriter->Write(msg);

	}

	

	void CLogger::InnitLog(std::shared_ptr<IlogLineFormat> pLogLineFormat,std::shared_ptr<IlogWriter> pLogWriter)
	{

		m_pLogLineFormat = pLogLineFormat;
		m_pLogWriter = pLogWriter;

		if (m_pLogLineFormat.get() == nullptr)
			m_pLogLineFormat.reset(new CLogFilePerformer());


		Info("---------------------------------------------------");
		Info("init logging");

	}

	void CLogger::Msg(eMsgType type, const astr& format, const astr& msg)
	{
		if (m_logLevel > type)
			return;

		Msg(type, str_format::StrFormatSafe(format, msg));
	}

	void CLogger::Msg(eMsgType type, const astr& format, const astr& msg, const astr& msg1)
	{
		if (m_logLevel > type)
			return;

		Msg(type, str_format::StrFormatSafe(format, msg, msg1));
	}

	void CLogger::Msg(eMsgType type, const astr& format, const astr& msg, const astr& msg1, const astr& msg2)
	{
		if (m_logLevel > type)
			return;

		Msg(type, str_format::StrFormatSafe(format, msg, msg1, msg2));
	}

	void CLogger::Msg(eMsgType type, const astr& format, const astr& msg, const astr& msg1, const astr& msg2, const astr& msg3)
	{
		if (m_logLevel > type)
			return;

		Msg(type, str_format::StrFormatSafe(format, msg, msg1, msg2, msg3));
	}

	void CLogger::Msg(eMsgType type, const astr& format, const astr& msg, const astr& msg1, const astr& msg2, const astr& msg3, const astr& msg4)
	{
		if (m_logLevel > type)
			return;

		Msg(type, str_format::StrFormatSafe(format, msg, msg1, msg2, msg3, msg4));
	}

	void CLogger::Msg(eMsgType type, const astr& format, const astr& msg, const astr& msg1, const astr& msg2, const astr& msg3, const astr& msg4, const astr& msg5)
	{
		if (m_logLevel > type)
			return;

		Msg(type, str_format::StrFormatSafe(format, msg, msg1, msg2, msg3, msg4, msg5));
	}


	void CLogger::Trace(const astr& msg)
	{
		Msg(eTrace, msg);
	}

	void CLogger::Trace(const astr& format, const astr& msg)
	{
		Msg(eTrace, format, msg);
	}

	void CLogger::Trace(const astr& format, const astr& msg, const astr& msg1)
	{
		Msg(eTrace, format, msg, msg1);
	}

	void CLogger::Trace(const astr& format, const astr& msg, const astr& msg1, const astr& msg2)
	{
		Msg(eTrace, format, msg, msg1, msg2);
	}

	void CLogger::Trace(const astr& format, const astr& msg, const astr& msg1, const astr& msg2, const astr& msg3)
	{
		Msg(eTrace, format, msg, msg1, msg2, msg3);
	}

	void CLogger::Trace(const astr& format, const astr& msg, const astr& msg1, const astr& msg2, const astr& msg3, const astr& msg4)
	{
		Msg(eTrace, format, msg, msg1, msg2, msg3, msg4);
	}

	void CLogger::Trace(const astr& format, const astr& msg, const astr& msg1, const astr& msg2, const astr& msg3, const astr& msg4, const astr& msg5)
	{
		Msg(eTrace, format, msg, msg1, msg2, msg3, msg4, msg5);
	}

	void CLogger::Info(const astr& msg)
	{
		Msg(eInfo, msg);
	}

	void CLogger::Info(const astr& format, const astr& msg)
	{
		Msg(eInfo, format, msg);
	}

	void CLogger::Info(const astr& format, const astr& msg, const astr& msg1)
	{
		Msg(eInfo, format, msg, msg1);
	}

	void CLogger::Info(const astr& format, const astr& msg, const astr& msg1, const astr& msg2)
	{
		Msg(eInfo, format, msg, msg1, msg2);
	}

	void CLogger::Info(const astr& format, const astr& msg, const astr& msg1, const astr& msg2, const astr& msg3)
	{
		Msg(eInfo, format, msg, msg1, msg2, msg3);
	}

	void CLogger::Info(const astr& format, const astr& msg, const astr& msg1, const astr& msg2, const astr& msg3, const astr& msg4)
	{
		Msg(eInfo, format, msg, msg1, msg2, msg3, msg4);
	}

	void CLogger::Info(const astr& format, const astr& msg, const astr& msg1, const astr& msg2, const astr& msg3, const astr& msg4, const astr& msg5)
	{
		Msg(eInfo, format, msg, msg1, msg2, msg3, msg4, msg5);
	}

	void CLogger::Error(const astr& msg)
	{
		Msg(eError, msg);
	}

	void CLogger::Error(const astr& format, const astr& msg)
	{
		Msg(eError, format, msg);
	}

	void CLogger::Error(const astr& format, const astr& msg, const astr& msg1)
	{
		Msg(eError, format, msg, msg1);
	}

	void CLogger::Error(const astr& format, const astr& msg, const astr& msg1, const astr& msg2)
	{
		Msg(eError, format, msg, msg1, msg2);
	}

	void CLogger::Error(const astr& format, const astr& msg, const astr& msg1, const astr& msg2, const astr& msg3)
	{
		Msg(eError, format, msg, msg1, msg2, msg3);
	}

	void CLogger::Error(const astr& format, const astr& msg, const astr& msg1, const astr& msg2, const astr& msg3, const astr& msg4)
	{
		Msg(eError, format, msg, msg1, msg2, msg3, msg4);
	}

	void CLogger::Error(const astr& format, const astr& msg, const astr& msg1, const astr& msg2, const astr& msg3, const astr& msg4, const astr& msg5)
	{
		Msg(eError, format, msg, msg1, msg2, msg3, msg4, msg5);
	}

	void CLogger::Warning(const astr& msg)
	{
		Msg(eWarning, msg);
	}

	void CLogger::Warning(const astr& format, const astr& msg)
	{
		Msg(eWarning, format, msg);
	}

	void CLogger::Warning(const astr& format, const astr& msg, const astr& msg1)
	{
		Msg(eWarning, format, msg, msg1);
	}

	void CLogger::Warning(const astr& format, const astr& msg, const astr& msg1, const astr& msg2)
	{
		Msg(eWarning, format, msg, msg1, msg2);
	}

	void CLogger::Warning(const astr& format, const astr& msg, const astr& msg1, const astr& msg2, const astr& msg3)
	{
		Msg(eWarning, format, msg, msg1, msg2, msg3);
	}

	void CLogger::Warning(const astr& format, const astr& msg, const astr& msg1, const astr& msg2, const astr& msg3, const astr& msg4)
	{
		Msg(eWarning, format, msg, msg1, msg2, msg3, msg4);
	}

	void CLogger::Warning(const astr& format, const astr& msg, const astr& msg1, const astr& msg2, const astr& msg3, const astr& msg4, const astr& msg5)
	{
		Msg(eWarning, format, msg, msg1, msg2, msg3, msg4, msg5);
	}

	void CLogger::Exc(const std::exception& exc)
	{
		Msg(eError, str_format::StrFormatSafe("exc: {0}", CExcBase::GetErrorDesc(exc)));
	}
 }