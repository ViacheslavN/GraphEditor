#include "TranStorageIO.h"
#include "../../../CommonLib/filesystem/filesystem.h"

namespace bptreedb
{
	namespace db
	{
		CTranStorageIO::CTranStorageIO(storage::IStorageIOPtr ptrCommitted, uint32_t nMemoryPages, const std::wstring& sTempPath) :
			m_ptrCommitted(ptrCommitted), m_nPageSize(ptrCommitted->GetPageSize()), m_sTempPath(sTempPath)
		{
			m_nMemoryBytes = (uint64_t)(std::max)(nMemoryPages, 1u) * m_nPageSize;
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
			if (nSize == 0 || (nSize % storage::BlockSize) != 0)
				throw CommonLib::CExcBase("[CTranStorageIO] size {0} isn't a multiple of the block {1}", nSize, storage::BlockSize);
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
					if (it->second.m_data.size() != nSize)
						throw CommonLib::CExcBase("the private page has {0} bytes, read {1}", (uint64_t)it->second.m_data.size(), nSize);
					memcpy(pData, it->second.m_data.data(), nSize);
					return;
				}
				auto sit = m_spilled.find(nAddr);
				if (sit != m_spilled.end())
				{
					if (sit->second.m_nSize != nSize)
						throw CommonLib::CExcBase("the private page has {0} bytes, read {1}", sit->second.m_nSize, nSize);
					m_ptrTempFile->SetFilePos64(sit->second.m_nPos, CommonLib::soFromBegin);
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
					// a spilled page stays in its place (a page of another size goes to the end of the file)
					if (sit->second.m_nSize != nSize)
					{
						sit->second.m_nPos = m_nFileEnd;
						sit->second.m_nSize = nSize;
						m_nFileEnd += nSize;
					}
					m_ptrTempFile->SetFilePos64(sit->second.m_nPos, CommonLib::soFromBegin);
					if (m_ptrTempFile->Write(pData, nSize) != (std::streamsize)nSize)
						throw CommonLib::CExcBase("failed to write the temporary page");
					return;
				}

				auto it = m_memory.find(nAddr);
				if (it != m_memory.end())
				{
					m_nBytes -= it->second.m_data.size();
					it->second.m_data.assign(pData, pData + nSize);
					m_nBytes += nSize;
					m_lru.splice(m_lru.begin(), m_lru, it->second.m_lru);
					return;
				}

				while (!m_memory.empty() && m_nBytes + nSize > m_nMemoryBytes)
					SpillOne();

				m_lru.push_front(nAddr);
				SMemPage& page = m_memory[nAddr];
				page.m_data.assign(pData, pData + nSize);
				page.m_lru = m_lru.begin();
				m_nBytes += nSize;
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("[CTranStorageIO] failed to write page {0}", nAddr, excSrc);
				throw;
			}
		}

		void CTranStorageIO::DropData(int64_t nAddr, uint32_t /*nSize*/)
		{
			// the private copy of a dropped page isn't written (the pager frees the page with the commit)
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			auto it = m_memory.find(nAddr);
			if (it != m_memory.end())
			{
				m_nBytes -= it->second.m_data.size();
				m_lru.erase(it->second.m_lru);
				m_memory.erase(it);
			}
			m_spilled.erase(nAddr);      // its place in the temporary file stays unused
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
			uint32_t nSize = (uint32_t)it->second.m_data.size();
			SSpilled spilled{ m_nFileEnd, nSize };
			m_ptrTempFile->SetFilePos64(spilled.m_nPos, CommonLib::soFromBegin);
			if (m_ptrTempFile->Write(it->second.m_data.data(), nSize) != (std::streamsize)nSize)
				throw CommonLib::CExcBase("failed to write the temporary page {0}", nAddr);
			m_nFileEnd += nSize;
			m_spilled[nAddr] = spilled;
			m_nBytes -= nSize;
			m_lru.pop_back();
			m_memory.erase(it);
		}

		size_t CTranStorageIO::PrivatePageCount() const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			return m_memory.size() + m_spilled.size();
		}

		void CTranStorageIO::ForEachPage(const std::function<void(int64_t, const byte_t*, uint32_t)>& func)
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			std::vector<int64_t> addrs;
			addrs.reserve(m_memory.size());
			for (const auto& item : m_memory)
				addrs.push_back(item.first);
			std::sort(addrs.begin(), addrs.end());
			for (int64_t nAddr : addrs)
			{
				const std::vector<byte_t>& data = m_memory.find(nAddr)->second.m_data;
				func(nAddr, data.data(), (uint32_t)data.size());
			}

			if (m_spilled.empty())
				return;

			// the pages in the order of the temporary file, read by blocks of adjacent pages
			struct SPlace
			{
				uint64_t nPos;
				uint32_t nSize;
				int64_t  nAddr;
			};
			std::vector<SPlace> places;
			places.reserve(m_spilled.size());
			for (const auto& item : m_spilled)
				places.push_back(SPlace{ item.second.m_nPos, item.second.m_nSize, item.first });
			std::sort(places.begin(), places.end(), [](const SPlace& a, const SPlace& b) { return a.nPos < b.nPos; });

			const uint64_t nMaxBlockBytes = 8 * 1024 * 1024;
			std::vector<byte_t> block;
			for (size_t i = 0; i < places.size(); )
			{
				size_t nCount = 1;
				uint64_t nBytes = places[i].nSize;
				while (i + nCount < places.size() && nBytes + places[i + nCount].nSize <= nMaxBlockBytes &&
					places[i + nCount].nPos == places[i].nPos + nBytes)
				{
					nBytes += places[i + nCount].nSize;
					++nCount;
				}

				block.resize((size_t)nBytes);
				m_ptrTempFile->SetFilePos64(places[i].nPos, CommonLib::soFromBegin);
				if (m_ptrTempFile->Read(block.data(), (uint32_t)block.size()) != (std::streamsize)block.size())
					throw CommonLib::CExcBase("[CTranStorageIO] failed to read the temporary pages at {0}", places[i].nPos);
				for (size_t j = 0; j < nCount; ++j)
					func(places[i + j].nAddr, block.data() + (places[i + j].nPos - places[i].nPos), places[i + j].nSize);
				i += nCount;
			}
		}

		void CTranStorageIO::Clear()
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			m_memory.clear();
			m_lru.clear();
			m_spilled.clear();
			m_nBytes = 0;
			m_nFileEnd = 0;
			if (m_ptrTempFile.get())
			{
				m_ptrTempFile->CloseFile();
				m_ptrTempFile.reset();
				CommonLib::CFileUtils::DelFile(m_sTempPath);
			}
		}
	}
}
