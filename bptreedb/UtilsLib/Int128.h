#pragma once
#include "../../CommonLib/CommonLib.h"
#include "../../CommonLib/stream/FixedMemoryStream.h"
#include "../../CommonLib/stream/MemoryStream.h"

namespace bptreedb
{
	namespace util
	{
		struct CUInt128
		{
		public:
			CUInt128(){}
			~CUInt128(){}

			bool operator < (const CUInt128& val) const
			{
				if (m_hi != val.m_hi)
					return m_hi < val.m_hi;

				return m_lo < val.m_lo;
			}

			bool operator >= (const CUInt128& val) const
			{
				if (m_hi != val.m_hi)
					return m_hi >= val.m_hi;

				return m_lo >= val.m_lo;
			}

			bool operator <= (const CUInt128& val) const
			{
				if (m_hi != val.m_hi)
					return m_hi <= val.m_hi;

				return m_lo <= val.m_lo;
			}

			bool operator == (const CUInt128& val) const
			{
				return m_hi == val.m_hi && m_lo == val.m_lo;
			}

			bool operator != (const CUInt128& val) const
			{
				return m_hi != val.m_hi || m_lo != val.m_lo;
			}


			void Add(uint32_t nVal)
			{
				uint64_t lo = m_lo + nVal;
				if (lo < m_lo)          // overflow of the low word: carry into the high word
					++m_hi;
				m_lo = lo;
			}


			void Save(CommonLib::IMemoryWriteStreamPtr ptrStream)
			{
				ptrStream->Write(m_lo);
				ptrStream->Write(m_hi);
			}

			void Load(CommonLib::IMemoryReadStreamPtr ptrStream)
			{
				m_lo = ptrStream->ReadIntu64();
				m_hi = ptrStream->ReadIntu64();
			}


  			uint64_t m_hi{ 0 };
			uint64_t m_lo{ 0 };



		};
	}
}