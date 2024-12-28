#pragma once
#include "log.h"

namespace CommonLib
{

	class CLogFilePerformer : public IlogLineFormat
	{
	public:
		CLogFilePerformer();
		~CLogFilePerformer();

		virtual void FormatLogline(const astr& msg, eMsgType type, astr& outMsg);
	private:
		void AddCurrentTime(astr& msg);
		void AddThreadID(astr& msg);
		void AddTypeMsg(eMsgType type, astr& msg);
		void AddMsg(const astr& msg, astr& outMsg);

	};
}
