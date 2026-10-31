#include "pch.h"
#include "WriteStreamPage.h"
#include <algorithm>

namespace bptreedb
{
	namespace storage
	{
		TWriteStreamPage::TWriteStreamPage(IFilePagerPtr ptrIFilePager, uint32_t nPageSize) :
			m_nPageSize(nPageSize), m_ptrIFilePager(ptrIFilePager)
		{

		}

		uint32_t TWriteStreamPage::MaxClass(uint32_t nMaxPageSize)
		{
			uint32_t nMax = (std::min)(nMaxPageSize, PageRef::MaxPageSize());
			uint32_t nClass = BlockSize;
			while ((uint64_t)nClass * 2 <= nMax)
				nClass *= 2;
			return nClass;
		}

		uint64_t TWriteStreamPage::WasteBudget(uint64_t nTotalBytes)
		{
			return (std::max)((uint64_t)2 * BlockSize, nTotalBytes / 8);
		}

		uint32_t TWriteStreamPage::NextPageSize(uint64_t nLeft, uint64_t nBudget, uint32_t nMaxClass)
		{
			// one page of the smallest class holding the rest, if it loses little
			for (uint32_t nClass = BlockSize; nClass <= nMaxClass; nClass *= 2)
			{
				uint64_t nPayload = nClass - PageOverhead;
				if (nPayload >= nLeft)
				{
					if (nPayload - nLeft <= nBudget)
						return nClass;
					break;
				}
			}
			// the biggest class the rest fills
			uint32_t nClass = nMaxClass;
			while (nClass > BlockSize && (uint64_t)(nClass - PageOverhead) > nLeft)
				nClass /= 2;
			return nClass;
		}

		std::vector<uint32_t> TWriteStreamPage::PlanPages(uint64_t nTotalBytes, uint32_t nMaxPageSize)
		{
			std::vector<uint32_t> pages;
			uint32_t nMaxClass = MaxClass(nMaxPageSize);
			uint64_t nBudget = WasteBudget(nTotalBytes);
			uint64_t nLeft = nTotalBytes;
			do
			{
				uint32_t nPage = NextPageSize(nLeft, nBudget, nMaxClass);
				pages.push_back(nPage);
				nLeft -= (std::min)(nLeft, (uint64_t)(nPage - PageOverhead));
			} while (nLeft != 0);
			return pages;
		}

		int64_t TWriteStreamPage::Create(uint64_t nTotalBytes, uint32_t nMaxPageSize, bool bNonZero)
		{
			try
			{
				m_nMaxClass = MaxClass(nMaxPageSize);
				m_nBudget = WasteBudget(nTotalBytes);
				m_nLeft = nTotalBytes;
				int64_t nRef = NewPageRef();
				if (bNonZero && nRef == 0)
					nRef = NewPageRef();
				OpenNextPage(nRef, true);
				return nRef;
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed to create WriteStreamPage, size: {0}", nTotalBytes, excSrc);
				throw;
			}
		}

		int64_t TWriteStreamPage::NewPageRef()
		{
			if (m_nMaxClass != 0)
				return m_ptrIFilePager->GetNewFilePageRef(NextPageSize(m_nLeft, m_nBudget, m_nMaxClass));
			return m_ptrIFilePager->GetNewFilePageRef(m_nPageSize);
		}

		TWriteStreamPage::~TWriteStreamPage()
		{

		}

		void TWriteStreamPage::Open(int64_t nPageAddr, bool bCreate)
		{
			try
			{
				OpenNextPage(nPageAddr, bCreate);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed open WriteStreamPage addr: {0}", nPageAddr, excSrc);
				throw;
			}

		}


		std::streamsize TWriteStreamPage::WriteBytes(const byte_t* buffer, size_t size)
		{
			write_bytes_impl(buffer, size, false);
			return size;
		}

		std::streamsize TWriteStreamPage::WriteInverse(const byte_t* buffer, size_t size)
		{
			write_bytes_impl(buffer, size, true);
			return size;
		}

		bool TWriteStreamPage::IsEnoughSpace(size_t size) const
		{
			return true;
		}

		void TWriteStreamPage::Close()
		{
			m_ptrStream->Seek(0, CommonLib::soFromBegin);
			m_ptrStream->Write(m_nNextAddr);
			m_ptrStream->Write(m_nPrevAddr);

			m_ptrIFilePager->SavePage(m_ptrCurrentPage);
		}
 
		void TWriteStreamPage::write_bytes_impl(const byte_t* buffer, size_t size, bool bInvers)
		{
			try
			{
				m_nLeft -= (std::min)((uint64_t)size, m_nLeft);
				size_t nPos = 0;
				while (size)
				{
					size_t nFreeSize = m_ptrStream->Size() - m_ptrStream->Pos();
					if (size <= nFreeSize)
					{
						if (bInvers)
							m_ptrStream->WriteInverse(buffer + nPos, size);
						else
							m_ptrStream->WriteBytes(buffer + nPos, size);
						size = 0;
					}
					else
					{

						size_t nWriteSize = nFreeSize;
						if (bInvers)
							m_ptrStream->WriteInverse(buffer + nPos, nWriteSize);
						else
							m_ptrStream->WriteBytes(buffer + nPos, nWriteSize);

						size -= nWriteSize;
						nPos += nWriteSize;

						int64_t nNext = m_nNextAddr;
						if (m_nNextAddr == -1)
						{
							// the bytes of this call not written yet are left too
							uint64_t nLeft = m_nLeft;
							m_nLeft += size;
							m_nNextAddr = NewPageRef();
							m_nLeft = nLeft;
						}

						OpenNextPage(m_nNextAddr, nNext == -1);
					}

				}
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("WritePageStream: Failed write bytes, size {0} ", size, excSrc);
				throw;
			}

		}

		void TWriteStreamPage::OpenNextPage(int64_t nPageAddr, bool bCreate)
		{
			try
			{

				if (m_ptrCurrentPage.get() != nullptr)
				{
					m_ptrStream->Seek(0, CommonLib::soFromBegin);
					m_ptrStream->Write(nPageAddr);
					m_ptrStream->Write(m_nPrevAddr);

					m_ptrIFilePager->SavePage(m_ptrCurrentPage);
					m_nPrevAddr = m_ptrCurrentPage->GetAddr();
				}


				if (bCreate)
					m_ptrCurrentPage = m_ptrIFilePager->GetNewPage(nPageAddr);
				else
				{
					if (m_ptrCurrentPage.get() == nullptr)
						m_ptrCurrentPage = m_ptrIFilePager->ReadPage(nPageAddr);
					else
						m_ptrIFilePager->ReadPage(m_ptrCurrentPage, nPageAddr);
				}
				

				m_nNextAddr = -1;

				if (!bCreate)
				{

					CommonLib::IMemoryReadStreamPtr  ptrStream = m_ptrCurrentPage->GetReadStream();
					m_nNextAddr = ptrStream->ReadInt64();
					int64_t nPrev = ptrStream->ReadInt64();

					if (nPrev != m_nPrevAddr)
						throw CommonLib::CExcBase("WriteStreamPage: Failed open page addr: {0}  prev: {1}, page prev: {2}", nPageAddr, m_nPrevAddr, nPrev);

					m_nPrevAddr = nPrev;

				}

				m_ptrStream = m_ptrCurrentPage->GetWriteStream();
				m_ptrStream->Seek(2 * sizeof(int64_t), CommonLib::soFromBegin);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("WriteStreamPage: Failed open page addr: {0}", nPageAddr, excSrc);
				throw;
			}

		}
	}
}