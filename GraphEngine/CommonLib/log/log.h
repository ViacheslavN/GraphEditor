#pragma once

namespace CommonLib
{
	enum eMsgType
	{
		eTrace = 1,
		eInfo = 2,
		eWarning = 4,
		eError = 8
	};

	class IlogLineFormat
	{
	public:
		IlogLineFormat() {}
		virtual ~IlogLineFormat(){}
		virtual void FormatLogline(const astr& msg, eMsgType type, astr& outMsg) = 0;
	};	

	class IlogRetention
	{
	public:
		IlogRetention() {}
		virtual ~IlogRetention() {}

		virtual void LogProcessing(const astr& logDirPath, const astr& logFileName) = 0;
		virtual void ArcLogFile(const astr& logPath) = 0;
	};

	class IlogWriter
	{
	public:
		IlogWriter() {}
		virtual ~IlogWriter() {}

		IlogWriter(const IlogWriter&) = delete;
		IlogWriter& operator=(const IlogWriter&) = delete;

	//	virtual void Open(const astr& path) = 0;
	//	virtual void Close() = 0;
		virtual void Write(const astr& msg) = 0;
		//virtual const astr& GetPath() const = 0;
		//virtual const astr& GetName() const = 0;
		//virtual size_t GetSize() const = 0;
	};
}