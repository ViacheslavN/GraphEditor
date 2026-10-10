#include "../CommonLib.h"
#include "FileMapping.h"
#include "../exception/exc_base.h"

#ifdef _WIN32
	#include <winioctl.h>
#else
	#include <sys/mman.h>
	#include <cerrno>
	#include <cstring>
#endif

namespace CommonLib
{
	namespace file
	{
		CFileMapping::CFileMapping() : m_pData(nullptr), m_nSize(0), m_nOffset(0)
		{

		}

		CFileMapping::~CFileMapping()
		{
			try
			{
				Unmap();
			}
			catch (...)
			{
			}
		}

#ifdef _WIN32

		void CFileMapping::Map(const IFile& file, uint64_t nOffset, size_t nSize, bool bWrite)
		{
			Unmap();
			if(nSize == 0)
				throw CExcBase("File mapping: the size is 0");
			if(nOffset % Granularity())
				throw CExcBase("File mapping: the offset {0} isn't aligned", nOffset);

			// the mapping object can be closed right away: the view keeps it
			HANDLE hMapping = ::CreateFileMappingW(file.Handle(), NULL, bWrite ? PAGE_READWRITE : PAGE_READONLY, 0, 0, NULL);
			if(hMapping == NULL)
				throw CExcBase("File mapping: CreateFileMapping failed, error {0}", (uint32_t)::GetLastError());

			void* pView = ::MapViewOfFile(hMapping, bWrite ? FILE_MAP_WRITE : FILE_MAP_READ, (DWORD)(nOffset >> 32), (DWORD)(nOffset & 0xFFFFFFFF), nSize);
			DWORD dwError = ::GetLastError();
			::CloseHandle(hMapping);
			if(pView == NULL)
				throw CExcBase("File mapping: MapViewOfFile failed at {0} ({1} bytes), error {2}", nOffset, (uint64_t)nSize, (uint32_t)dwError);

			m_pData = (byte_t*)pView;
			m_nSize = nSize;
			m_nOffset = nOffset;
		}

		void CFileMapping::Unmap()
		{
			if(!m_pData)
				return;
			byte_t* pData = m_pData;
			m_pData = nullptr;
			m_nSize = 0;
			m_nOffset = 0;
			if(!::UnmapViewOfFile(pData))
				throw CExcBase("File mapping: UnmapViewOfFile failed, error {0}", (uint32_t)::GetLastError());
		}

		void CFileMapping::Flush()
		{
			if(m_pData && !::FlushViewOfFile(m_pData, m_nSize))
				throw CExcBase("File mapping: FlushViewOfFile failed, error {0}", (uint32_t)::GetLastError());
		}

		size_t CFileMapping::Granularity()
		{
			SYSTEM_INFO info;
			::GetSystemInfo(&info);
			return info.dwAllocationGranularity;
		}

		void CFileMapping::SetFileSize(IFile& file, uint64_t nSize)
		{
			// the end of the file is set at the file pointer, the pointer is restored
			LARGE_INTEGER zero, current, end;
			zero.QuadPart = 0;
			end.QuadPart = (LONGLONG)nSize;
			if(!::SetFilePointerEx(file.Handle(), zero, &current, FILE_CURRENT) || !::SetFilePointerEx(file.Handle(), end, NULL, FILE_BEGIN))
				throw CExcBase("File mapping: failed to set the file size {0}, error {1}", nSize, (uint32_t)::GetLastError());
			BOOL bOk = ::SetEndOfFile(file.Handle());
			DWORD dwError = ::GetLastError();
			::SetFilePointerEx(file.Handle(), current, NULL, FILE_BEGIN);
			if(!bOk)
				throw CExcBase("File mapping: failed to set the file size {0}, error {1}", nSize, (uint32_t)dwError);
		}

		bool CFileMapping::SetSparse(IFile& file)
		{
			DWORD dwReturned = 0;
			return ::DeviceIoControl(file.Handle(), FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &dwReturned, NULL) != FALSE;
		}

#else

		void CFileMapping::Map(const IFile& file, uint64_t nOffset, size_t nSize, bool bWrite)
		{
			Unmap();
			if(nSize == 0)
				throw CExcBase("File mapping: the size is 0");
			if(nOffset % Granularity())
				throw CExcBase("File mapping: the offset {0} isn't aligned", nOffset);

			void* pView = ::mmap(nullptr, nSize, bWrite ? (PROT_READ | PROT_WRITE) : PROT_READ, MAP_SHARED, file.Handle(), (off_t)nOffset);
			if(pView == MAP_FAILED)
			{
				int nError = errno;
				throw CExcBase("File mapping: mmap failed at {0} ({1} bytes): {2}", nOffset, (uint64_t)nSize, std::string(strerror(nError)));
			}

			m_pData = (byte_t*)pView;
			m_nSize = nSize;
			m_nOffset = nOffset;
		}

		void CFileMapping::Unmap()
		{
			if(!m_pData)
				return;
			byte_t* pData = m_pData;
			size_t nSize = m_nSize;
			m_pData = nullptr;
			m_nSize = 0;
			m_nOffset = 0;
			if(::munmap(pData, nSize) != 0)
				throw CExcBase("File mapping: munmap failed: {0}", std::string(strerror(errno)));
		}

		void CFileMapping::Flush()
		{
			if(m_pData && ::msync(m_pData, m_nSize, MS_SYNC) != 0)
				throw CExcBase("File mapping: msync failed: {0}", std::string(strerror(errno)));
		}

		size_t CFileMapping::Granularity()
		{
			long nPage = ::sysconf(_SC_PAGESIZE);
			return nPage > 0 ? (size_t)nPage : 4096;
		}

		void CFileMapping::SetFileSize(IFile& file, uint64_t nSize)
		{
			if(::ftruncate(file.Handle(), (off_t)nSize) != 0)
				throw CExcBase("File mapping: failed to set the file size {0}: {1}", nSize, std::string(strerror(errno)));
		}

		bool CFileMapping::SetSparse(IFile& /*file*/)
		{
			return true;
		}

#endif
	}
}
