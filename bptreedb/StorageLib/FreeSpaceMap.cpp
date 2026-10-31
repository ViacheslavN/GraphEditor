#include "pch.h"
#include "FreeSpaceMap.h"
#include "WriteStreamPage.h"
#include "ReadStreamPage.h"

namespace bptreedb
{
	namespace storage
	{
		namespace
		{
			void WriteVarint(CommonLib::IWriteStream& stream, uint64_t nValue)
			{
				while (nValue >= 0x80)
				{
					stream.Write((byte_t)(nValue | 0x80));
					nValue >>= 7;
				}
				stream.Write((byte_t)nValue);
			}

			uint64_t ReadVarint(CommonLib::IReadStream& stream)
			{
				uint64_t nValue = 0;
				for (int nShift = 0; nShift < 64; nShift += 7)
				{
					byte_t b = stream.ReadByte();
					nValue |= (uint64_t)(b & 0x7F) << nShift;
					if ((b & 0x80) == 0)
						return nValue;
				}
				throw CommonLib::CExcBase("a wrong varint");
			}
		}

		CFreeSpaceMap::CFreeSpaceMap()
		{
		}

		CFreeSpaceMap::~CFreeSpaceMap()
		{
		}

		bool CFreeSpaceMap::Take(uint32_t nBlocks, int64_t& nAddr)
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			if (m_bSaving)
				return false;
			auto it = m_free.find(nBlocks);
			if (it == m_free.end() || it->second.empty())
				return false;
			nAddr = *it->second.begin();
			it->second.erase(it->second.begin());
			if (it->second.empty())
				m_free.erase(it);
			m_nPages -= 1;
			m_nBlocks -= nBlocks;
			return true;
		}

		void CFreeSpaceMap::Add(int64_t nAddr, uint32_t nBlocks)
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			if (nAddr < 0 || nBlocks == 0)
				throw CommonLib::CExcBase("CFreeSpaceMap: wrong free page {0}, {1} blocks", nAddr, nBlocks);
			if (!m_free[nBlocks].insert(nAddr).second)
				throw CommonLib::CExcBase("CFreeSpaceMap: the page {0} of {1} blocks is freed twice", nAddr, nBlocks);
			m_nPages += 1;
			m_nBlocks += nBlocks;
		}

		void CFreeSpaceMap::Clear()
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			m_free.clear();
			m_nPages = m_nBlocks = 0;
		}

		uint64_t CFreeSpaceMap::PageCount() const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			return m_nPages;
		}

		uint64_t CFreeSpaceMap::FreeBlocks() const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			return m_nBlocks;
		}

		int64_t CFreeSpaceMap::EndAddr() const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			int64_t nEnd = 0;
			for (const auto& item : m_free)
				if (!item.second.empty())
					nEnd = (std::max)(nEnd, *item.second.rbegin() + (int64_t)item.first);
			return nEnd;
		}

		void CFreeSpaceMap::Save(IFilePagerPtr ptrPager, int64_t nRootRef, const std::vector<std::pair<int64_t, uint32_t> >& extra)
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			try
			{
				std::map<uint32_t, std::set<int64_t> > all = m_free;
				for (const auto& page : extra)
					if (!all[page.second].insert(page.first).second)
						throw CommonLib::CExcBase("the page {0} of {1} blocks is freed twice", page.first, page.second);

				// the new stream pages (the map grew) come from the end of the storage
				m_bSaving = true;
				struct SReset { bool& b; ~SReset() { b = false; } } reset{ m_bSaving };

				TWriteStreamPage stream(ptrPager);
				stream.Open(nRootRef, false);
				stream.Write(Magic);
				stream.Write(Version);
				stream.Write((uint32_t)all.size());
				for (const auto& item : all)
				{
					stream.Write(item.first);
					stream.Write((uint64_t)item.second.size());
					int64_t nPrev = 0;
					for (int64_t nAddr : item.second)
					{
						WriteVarint(stream, (uint64_t)(nAddr - nPrev));
						nPrev = nAddr;
					}
				}
				stream.Close();
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("CFreeSpaceMap: failed to save, root {0}", nRootRef, excSrc);
				throw;
			}
		}

		void CFreeSpaceMap::WriteEmpty(IFilePagerPtr ptrPager, int64_t nRootRef)
		{
			try
			{
				TWriteStreamPage stream(ptrPager);
				stream.Open(nRootRef, true);
				stream.Write(Magic);
				stream.Write(Version);
				stream.Write((uint32_t)0);
				stream.Close();
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("CFreeSpaceMap: failed to write the empty map, root {0}", nRootRef, excSrc);
				throw;
			}
		}

		void CFreeSpaceMap::Load(IFilePagerPtr ptrPager, int64_t nRootRef)
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			try
			{
				Clear();
				TReadStreamPage stream(ptrPager);
				stream.Open(nRootRef);
				if (stream.ReadIntu32() != Magic)
					throw CommonLib::CExcBase("not a free space map");
				uint32_t nVersion = stream.ReadIntu32();
				if (nVersion != Version)
					throw CommonLib::CExcBase("unsupported version {0}", nVersion);
				uint32_t nSizes = stream.ReadIntu32();
				for (uint32_t i = 0; i < nSizes; ++i)
				{
					uint32_t nBlocks = stream.ReadIntu32();
					uint64_t nCount = stream.ReadIntu64();
					int64_t nAddr = 0;
					for (uint64_t k = 0; k < nCount; ++k)
					{
						nAddr += (int64_t)ReadVarint(stream);
						Add(nAddr, nBlocks);
					}
				}
			}
			catch (std::exception& excSrc)
			{
				Clear();
				CommonLib::CExcBase::RegenExcT("CFreeSpaceMap: failed to load, root {0}", nRootRef, excSrc);
				throw;
			}
		}
	}
}
