#include "TranStorageIO.h"
#include "../../../CommonLib/filesystem/filesystem.h"

namespace bptreedb
{
	namespace db
	{
		CTranStorageIO::CTranStorageIO(storage::IStorageIOPtr ptrCommitted, uint32_t nMemoryPages, const std::wstring& sTempPath) :
			m_ptrCommitted(ptrCommitted), m_nPageSize(ptrCommitted->GetPageSize()), m_nMemoryPages((std::max)(nMemoryPages, 1u)), m_sTempPath(sTempPath)
		{
		}

		CTranStorageIO::~CTranStorageIO()
		{
			try
			{
				Clear();
			}
			catch (...)
			{
			}
		}

		uint32_t CTranStorageIO::GetPageSize() const
		{
			return m_nPageSize;
		}

		void CTranStorageIO::CheckSize(uint32_t nSize) const
		{
			if (nSize != m_nPageSize)
				throw CommonLib::CExcBase("[CTranStorageIO] size {0} isn't the page size {1}", nSize, m_nPageSize);
		}

		int64_t CTranStorageIO::GetNewFilePageAddr(uint32_t nSize)
		{
			// the address of a new page of the file: nothing is written until the commit
			return m_ptrCommitted->GetNewFilePageAddr(nSize);
		}

		void CTranStorageIO::ReadData(int64_t nAddr, byte_t* pData, uint32_t nSize)
		{
			try
			{
				std::lock_guard<std::recursive_mutex> locker(m_mutex);
				CheckSize(nSize);
				auto it = m_memory.find(nAddr);
				if (it != m_memory.end())
				{
					memcpy(pData, it->second.m_data.data(), nSize);
					return;
				}
				auto sit = m_spilled.find(nAddr);
				if (sit != m_spilled.end())
				{
					m_ptrTempFile->SetFilePos64(sit->second * m_nPageSize, CommonLib::soFromBegin);
					if (m_ptrTempFile->Read(pData, nSize) != (std::streamsize)nSize)
						throw CommonLib::CExcBase("failed to read the temporary page");
					return;
				}
				m_ptrCommitted->ReadData(nAddr, pData, nSize);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("[CTranStorageIO] failed to read page {0}", nAddr, excSrc);
				throw;
			}
		}

		void CTranStorageIO::WriteData(int64_t nAddr, const byte_t* pData, uint32_t nSize)
		{
			try
			{
				std::lock_guard<std::recursive_mutex> locker(m_mutex);
				CheckSize(nSize);

				auto sit = m_spilled.find(nAddr);
				if (sit != m_spilled.end())
				{
					// a spilled page stays in its slot
					m_ptrTempFile->SetFilePos64(sit->second * m_nPageSize, CommonLib::soFromBegin);
					if (m_ptrTempFile->Write(pData, nSize) != (std::streamsize)nSize)
						throw CommonLib::CExcBase("failed to write the temporary page");
					return;
				}

				auto it = m_memory.find(nAddr);
				if (it != m_memory.end())
				{
					memcpy(it->second.m_data.data(), pData, nSize);
					m_lru.splice(m_lru.begin(), m_lru, it->second.m_lru);
					return;
				}

				if (m_memory.size() >= m_nMemoryPages)
					SpillOne();

				m_lru.push_front(nAddr);
				SMemPage& page = m_memory[nAddr];
				page.m_data.assign(pData, pData + nSize);
				page.m_lru = m_lru.begin();
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("[CTranStorageIO] failed to write page {0}", nAddr, excSrc);
				throw;
			}
		}

		void CTranStorageIO::DropData(int64_t /*nAddr*/, uint32_t /*nSize*/)
		{
			// pages aren't reused yet (no free page management): a dropped page stays where it is
		}

		void CTranStorageIO::OpenTempFile()
		{
			if (m_ptrTempFile.get())
				return;
			m_ptrTempFile = CommonLib::file::CFileCreator::OpenFile(m_sTempPath.c_str(), CommonLib::file::ofmCreateAlways,
				CommonLib::file::aeReadWrite, CommonLib::file::smNoMode, CommonLib::file::oftBinary);
		}

		void CTranStorageIO::SpillOne()
		{
			OpenTempFile();
			int64_t nAddr = m_lru.back();
			auto it = m_memory.find(nAddr);
			uint64_t nSlot = m_nSlots++;
			m_ptrTempFile->SetFilePos64(nSlot * m_nPageSize, CommonLib::soFromBegin);
			if (m_ptrTempFile->Write(it->second.m_data.data(), m_nPageSize) != (std::streamsize)m_nPageSize)
				throw CommonLib::CExcBase("failed to write the temporary page {0}", nAddr);
			m_spilled[nAddr] = nSlot;
			m_lru.pop_back();
			m_memory.erase(it);
		}

		size_t CTranStorageIO::PrivatePageCount() const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			return m_memory.size() + m_spilled.size();
		}

		void CTranStorageIO::ForEachPage(const std::function<void(int64_t, const byte_t*)>& func)
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			std::vector<int64_t> addrs;
			addrs.reserve(m_memory.size() + m_spilled.size());
			for (const auto& item : m_memory)
				addrs.push_back(item.first);
			for (const auto& item : m_spilled)
				addrs.push_back(item.first);
			std::sort(addrs.begin(), addrs.end());

			m_buffer.resize(m_nPageSize);
			for (int64_t nAddr : addrs)
			{
				auto it = m_memory.find(nAddr);
				if (it != m_memory.end())
					func(nAddr, it->second.m_data.data());
				else
				{
					ReadData(nAddr, m_buffer.data(), m_nPageSize);
					func(nAddr, m_buffer.data());
				}
			}
		}

		void CTranStorageIO::Clear()
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			m_memory.clear();
			m_lru.clear();
			m_spilled.clear();
			m_nSlots = 0;
			if (m_ptrTempFile.get())
			{
				m_ptrTempFile->CloseFile();
				m_ptrTempFile.reset();
				CommonLib::CFileUtils::DelFile(m_sTempPath);
			}
		}
	}
}
