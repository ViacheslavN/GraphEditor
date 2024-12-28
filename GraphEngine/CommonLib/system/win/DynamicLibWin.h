#pragma once

namespace CommonLib
{
namespace system
{
namespace win
{

	class CDynamicLibraryWin
	{
	public:
		CDynamicLibraryWin(const astr& path);
		CDynamicLibraryWin(const wstr& path);
		~CDynamicLibraryWin();

		CDynamicLibraryWin(const CDynamicLibraryWin&) = delete;
		CDynamicLibraryWin& operator=(const CDynamicLibraryWin&) = delete;

		FARPROC  GetProcAddr(const astr& proc_name);
		FARPROC  GetProcAddr(const wstr& proc_name);

		template <typename TFunkPtrType>
		void GetProcAddrEx(const astr& procName, TFunkPtrType& result)
		{
			result = (TFunkPtrType)GetProcAddr(procName);
		}

		template <typename TFunkPtrType>
		void GetProcAddrEx(const wstr& procName, TFunkPtrType& result)
		{
			result = (TFunkPtrType)GetProcAddr(procName);
		}
		
	private:
		void LoadLib(const wstr& path);
 

	private:
		HMODULE m_handle;
		
	};

}
    typedef win::CDynamicLibraryWin TDynamicLib;
    typedef std::shared_ptr<TDynamicLib> TDynamicLibPtr;

}
}