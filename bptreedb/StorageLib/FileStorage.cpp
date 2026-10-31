#include "pch.h"
#include "FileStorage.h"

namespace bptreedb
{
	namespace storage
	{
		CFileStorage::CFileStorage(CommonLib::IAllocPtr ptrAlloc, int32_t storageId, IStorageCipherPtr ptrCipher) : m_ptrAlloc(ptrAlloc),
			m_storage_id(storageId), m_ptrStorageCipher(ptrCipher)
		{

		}

		CFileStorage::~CFileStorage()
		{

		}

		void CFileStorage::Open(const char* pszNameUtf8, bool bCreate, uint64_t offset, uint32_t nPageSize)
		{
			try
			{
				if (nPageSize == 0 || (nPageSize % BlockSize) != 0 || nPageSize > PageRef::MaxPageSize())
					throw CommonLib::CExcBase("Wrong page size: {0} (a multiple of {1})", nPageSize, BlockSize);

				m_offset = offset;
				CommonLib::file::enOpenFileMode nOpenMode = bCreate ? CommonLib::file::ofmCreateAlways : CommonLib::file::ofmOpenExisting;
				CommonLib::file::enAccesRights nReadWrite = CommonLib::file::aeReadWrite;

				m_nPageSize = nPageSize;
				m_file.OpenFileA(pszNameUtf8, nOpenMode, nReadWrite, CommonLib::file::smNoMode, CommonLib::file::oftBinary);

				// the blocks start after the offset; a partly written last block counts
				uint64_t nSize = m_file.GetFileSize();
				uint64_t nDataSize = nSize > m_offset ? nSize - m_offset : 0;
				m_lastAddr = (int64_t)((nDataSize + BlockSize - 1) / BlockSize);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Error open storage path: {0}, create: {1}", pszNameUtf8, bCreate ? 1 : 0, excSrc);
				throw;
			}
		}

		void CFileStorage::Open(const wchar_t* pszName, bool bCreate, uint64_t offset, uint32_t nPageSize)
		{
			Open(CommonLib::StringEncoding::str_w2utf8_safe(pszName).c_str(), bCreate, offset, nPageSize);
		}

		void CFileStorage::Close()
		{
			try
			{
				m_file.CloseFile();
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Error close  storage", excSrc);
				throw;
			}
		}

		// caller holds m_mutex
		void CFileStorage::CheckRange(int64_t nAddr, uint32_t nSize) const
		{
			if (nSize == 0 || (nSize % BlockSize) != 0)
				throw CommonLib::CExcBase("Wrong size: {0} (a multiple of {1})", nSize, BlockSize);
			if (nAddr < 0 || nAddr + (int64_t)(nSize / BlockSize) > m_lastAddr)
				throw CommonLib::CExcBase("Outrange addr {0}, size {1}, lastAddr: {2}", nAddr, nSize, m_lastAddr);
		}

		void CFileStorage::ReadData(int64_t nAddr, byte_t* pData, uint32_t nSize)
		{
			try
			{
				std::lock_guard<std::recursive_mutex> locker(m_mutex);
				CheckRange(nAddr, nSize);
				{
					CommonLib::CPrefCounterHolder holder(m_ptrStoragePerformer, eReadData, nSize);
					m_file.SetFilePos64(m_offset + (uint64_t)nAddr * BlockSize, CommonLib::soFromBegin);
					uint32_t nCnt = (uint32_t)m_file.Read(pData, nSize);
					if (nCnt != nSize)
						throw CommonLib::CExcBase("Can't read the requested size: {0}, returned size: {1}", nSize, nCnt);
				}

				if (m_ptrStorageCipher.get())
				{
					CommonLib::CPrefCounterHolder holder(m_ptrStoragePerformer, eDecryptData, nSize);
					for (uint32_t i = 0; i < nSize / BlockSize; ++i)
						m_ptrStorageCipher->Decrypt(nAddr + i, pData + (size_t)i * BlockSize, BlockSize);
				}
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed to read data, addr: {0}, size: {1} ", nAddr, nSize, excSrc);
				throw;
			}
		}

		void CFileStorage::WriteData(int64_t nAddr, const byte_t* pData, uint32_t nSize)
		{
			try
			{
				std::lock_guard<std::recursive_mutex> locker(m_mutex);
				CheckRange(nAddr, nSize);

				const byte_t* pWrite = pData;
				if (m_ptrStorageCipher.get() != nullptr)
				{
					if (m_bufForChiper.size() < nSize)
						m_bufForChiper.resize(nSize);
					CommonLib::CPrefCounterHolder holder(m_ptrStoragePerformer, eEncryptData, nSize);
					for (uint32_t i = 0; i < nSize / BlockSize; ++i)
						m_ptrStorageCipher->Encrypt(nAddr + i, pData + (size_t)i * BlockSize, BlockSize, &m_bufForChiper[(size_t)i * BlockSize], BlockSize);
					pWrite = m_bufForChiper.data();
				}

				CommonLib::CPrefCounterHolder holder(m_ptrStoragePerformer, eWriteData, nSize);
				m_file.SetFilePos64(m_offset + (uint64_t)nAddr * BlockSize, CommonLib::soFromBegin);
				uint32_t nCnt = (uint32_t)m_file.Write(pWrite, nSize);
				if (nCnt != nSize)
					throw CommonLib::CExcBase("Can't write the data, size: {0}, written bytes {1}", nSize, nCnt);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed to write data, addr: {0}, size {1}", nAddr, nSize, excSrc);
				throw;
			}
		}

		void CFileStorage::DropData(int64_t nAddr, uint32_t nSize)
		{
			try
			{
				std::lock_guard<std::recursive_mutex> locker(m_mutex);
				CheckRange(nAddr, nSize);
				// the space isn't reused yet (no free space management)
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed to drop data, addr: {0}, size {1}", nAddr, nSize, excSrc);
				throw;
			}
		}

		void CFileStorage::ReserveAddr(int64_t nEndAddr)
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			if (m_lastAddr < nEndAddr)
				m_lastAddr = nEndAddr;
		}

		int64_t CFileStorage::GetEndAddr() const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			return m_lastAddr;
		}

		void CFileStorage::Flush()
		{
			try
			{
				m_file.Flush();
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed to flush storage", excSrc);
				throw;
			}
		}

		void CFileStorage::SetStoragePerformer(CommonLib::TPrefCounterPtr pStoragePerformer)
		{
			m_ptrStoragePerformer = pStoragePerformer;
		}

		int32_t CFileStorage::GetStorageId() const
		{
			return m_storage_id;
		}

		int64_t CFileStorage::GetNewFilePageAddr(uint32_t nSize)
		{
			try
			{
				std::lock_guard<std::recursive_mutex> locker(m_mutex);

				if (nSize == 0)
					nSize = m_nPageSize;

				if ((nSize % BlockSize) != 0)
					throw CommonLib::CExcBase("Wrong page size: {0} (a multiple of {1})", nSize, BlockSize);

				int64_t nAddr = m_lastAddr;
				m_lastAddr += nSize / BlockSize;
				return nAddr;
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed to get new page addr", excSrc);
				throw;
			}
		}

		uint32_t CFileStorage::GetPageSize() const
		{
			return m_nPageSize;
		}

		void CFileStorage::Lock()
		{
			m_mutex.lock();
		}

		void CFileStorage::UnLock()
		{
			m_mutex.unlock();
		}

		bool CFileStorage::TryLock()
		{
			return 	m_mutex.try_lock();
		}
	}
}