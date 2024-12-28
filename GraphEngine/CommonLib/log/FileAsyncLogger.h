#pragma once

#include "LogFileWriterBase.h"
#include "../synch/SyncQueue.h"

namespace CommonLib
{
	namespace synch
	{
		class CThread;
	}

	namespace file
	{
		class IFile;
	}

	class CFileAsyncLogger : public CLogFileWriterBase
	{
	public:
		CFileAsyncLogger(size_t maxLogSize, std::shared_ptr<IlogRetention>& pLogRetention);
		~CFileAsyncLogger();

		CFileAsyncLogger(const CFileAsyncLogger&) = delete;
		CFileAsyncLogger& operator=(const CFileAsyncLogger&) = delete;


		static std::shared_ptr<CommonLib::IlogWriter> Create(const astr& path, size_t maxLogSize, std::shared_ptr<IlogRetention>& pLogRetention);


		virtual void Write(const astr& msg);


		void ChangeCurrentLogFile();
		void WriteMessage(const astr& msg);
		void ThreadFunc();

	private:

		std::shared_ptr<synch::CThread> m_thread;
		synch::TSyncQueue<astr> m_queue;
	};
}