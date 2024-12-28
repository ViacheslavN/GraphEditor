#pragma once

#include "logger.h"
#include "../synch/SyncQueue.h"

namespace CommonLib
{
	namespace file
	{
		class IFile;
	}

	class CLogFileWriterBase : public IlogWriter
	{
	public:

		CLogFileWriterBase(size_t maxLogSize, std::shared_ptr<IlogRetention>& pLogRetention);
		~CLogFileWriterBase();

		virtual void Open(const astr& path);
		virtual void Close();
		virtual const astr& GetPath() const;
		virtual const astr& GetName() const;
		virtual size_t GetSize() const;

	protected:

		void ChangeCurrentLogFile();

	protected:
		astr m_logName;
		astr m_path;
		size_t m_maxLogSize;
		size_t m_currSize;
		std::shared_ptr<IlogRetention> m_logRetention;

		std::shared_ptr<file::IFile> m_file;

	};
}

