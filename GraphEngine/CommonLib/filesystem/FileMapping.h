#pragma once

#include "File.h"

namespace CommonLib
{
	namespace file
	{
		// A part of a file mapped into the memory (MapViewOfFile on Windows, mmap on POSIX).
		// The pages are loaded by the system when they are touched and are cached by it, the mapped size doesn't take
		// the process memory (only the address space: a 64-bit build is needed for big files).
		class CFileMapping
		{
		public:
			CFileMapping();
			~CFileMapping();

			// maps nSize bytes from nOffset (a multiple of Granularity()) of an open file, the file must be at least
			// nOffset + nSize long (SetFileSize); bWrite - the file is opened for writing and the view is writable
			void Map(const IFile& file, uint64_t nOffset, size_t nSize, bool bWrite);
			void Unmap();
			// writes the changed pages to the file
			void Flush();

			bool     IsMapped() const { return m_pData != nullptr; }
			byte_t*  Data() const { return m_pData; }
			size_t   Size() const { return m_nSize; }
			uint64_t Offset() const { return m_nOffset; }

			// the alignment of the offsets: 64 KB on Windows, the page size on POSIX
			static size_t Granularity();

			// extends (the new part reads as zeros) or truncates the file; no view of it can be mapped (Windows)
			static void SetFileSize(IFile& file, uint64_t nSize);

			// the parts which are never written don't take disk space: NTFS needs the sparse flag,
			// POSIX file systems make the holes by themselves; returns false if the file system doesn't support it
			static bool SetSparse(IFile& file);

		private:
			CFileMapping(const CFileMapping&);
			CFileMapping& operator=(const CFileMapping&);

		private:
			byte_t*  m_pData;
			size_t   m_nSize;
			uint64_t m_nOffset;
		};
	}
}
