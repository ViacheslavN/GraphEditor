#pragma once
#include "../../DeVector.h"
#include "../../BPTreeContext.h"
#include "../../AllocsSet.h"
#include "../CompressorParams.h"
#include "../../../../CommonLib/compress/zlib/zlib.h"
#include <vector>
#include <cstring>
#include <type_traits>

namespace bptreedb
{
	// Value encoder for fixed size values (float, double, integers, POD structs), any order:
	// the bytes are grouped by their position in the value (all byte 0s, all byte 1s, ...: the sign / exponent
	// bytes of doubles become long runs) and the block is compressed with zlib; stored raw if zlib doesn't help.
	//
	//   uint8 mode (0 raw, 1 zlib), uint32 size, data
	//
	// Size estimate: raw size / ratio + margin; the ratio starts with the param "compressRate" (default 2)
	// and follows the real ratio of the last encode / decode of this node.
	template <class _TValue>
	class TShuffleZlibEncoder
	{
	public:
		typedef _TValue TValue;
		typedef CommonLib::STLAllocator<TValue> TAlloc;
		typedef TDeVector<TValue, TAlloc> TValueMemSet;
		static_assert(std::is_trivially_copyable<TValue>::value, "TShuffleZlibEncoder: a trivially copyable value");

		static constexpr uint32_t HeadSize = sizeof(uint8_t) + sizeof(uint32_t);

		TShuffleZlibEncoder(TAllocsSetPtr pAllocsSet, TCompressorParamsBasePtr pParams, ECompressParams type)
		{
			TCompressorParamsPtr p = pParams.get() ? pParams->GetCompressParams(type) : TCompressorParamsPtr();
			if (p.get())
			{
				m_nLevel = p->GetIntParam("compressLevel", m_nLevel);
				m_dRatio = (double)p->GetIntParam("compressRate", 2);
			}
			if (m_dRatio < 1.0)
				m_dRatio = 1.0;
		}

		void AddSymbol(uint32_t, int, const TValue&, const TValueMemSet&) { ++m_nCount; }
		void RemoveSymbol(uint32_t, int, const TValue&, const TValueMemSet&) { --m_nCount; }
		void UpdateSymbol(int, TValue&, const TValue&, const TValueMemSet&) {}

		uint32_t GetCompressSize() const
		{
			if (m_nCount == 0)
				return HeadSize;
			double dRaw = (double)m_nCount * sizeof(TValue);
			return HeadSize + (uint32_t)(std::min)(dRaw, dRaw / m_dRatio * 1.05 + 16);
		}

		void BeginEncoding(const TValueMemSet&) {}

		uint32_t Encode(const TValueMemSet& vecValues, CommonLib::IMemoryWriteStream* pStream, uint32_t maxCompSize, CBPTreeContext* pContext)
		{
			uint32_t n = (uint32_t)vecValues.size();
			uint32_t nSplit = (std::max)(1u, n / 2);
			if (m_nCount != n)
				throw CommonLib::CExcBase("[TShuffleZlibEncoder] wrong size, count: {0}, values: {1}", m_nCount, n);

			size_t nRaw = (size_t)n * sizeof(TValue);
			m_raw.resize(nRaw);
			for (uint32_t i = 0; i < n; ++i)
			{
				const byte_t* p = (const byte_t*)&vecValues[i];
				for (size_t b = 0; b < sizeof(TValue); ++b)
					m_raw[b * n + i] = p[b];
			}

			size_t nFree = (std::min)((size_t)maxCompSize, (size_t)(pStream->Size() - pStream->Pos()));
			if (nFree < HeadSize)
				return nSplit;

			uLongf nComp = compressBound((uLong)nRaw);
			m_comp.resize(nComp);
			int res = compress2(m_comp.data(), &nComp, m_raw.data(), (uLong)nRaw, m_nLevel);
			if (res != Z_OK)
				throw CommonLib::CExcBase("[TShuffleZlibEncoder] compress error {0}", res);

			uint8_t mode = nComp < nRaw ? 1 : 0;
			uint32_t nSize = mode ? (uint32_t)nComp : (uint32_t)nRaw;
			if (HeadSize + nSize > nFree)
				return nSplit;

			pStream->Write(mode);
			pStream->Write(nSize);
			if (nSize)
				pStream->WriteBytes(mode ? m_comp.data() : m_raw.data(), nSize);
			UpdateRatio(nRaw, nSize);
			return 0;
		}

		void Decode(uint32_t nCount, TValueMemSet& vecValues, CommonLib::IMemoryReadStream* pStream, uint32_t nCompSize, CBPTreeContext* pContext)
		{
			uint8_t mode = pStream->ReadByte();
			uint32_t nSize = pStream->ReadIntu32();
			size_t nRaw = (size_t)nCount * sizeof(TValue);
			m_raw.resize(nRaw);
			if (mode == 0)
			{
				if (nSize != nRaw)
					throw CommonLib::CExcBase("[TShuffleZlibEncoder] wrong raw size {0}, expected {1}", nSize, (uint64_t)nRaw);
				if (nSize)
					pStream->ReadBytes(m_raw.data(), nSize);
			}
			else
			{
				m_comp.resize(nSize);
				pStream->ReadBytes(m_comp.data(), nSize);
				uLongf nOut = (uLongf)nRaw;
				int res = uncompress(m_raw.data(), &nOut, m_comp.data(), nSize);
				if (res != Z_OK || nOut != nRaw)
					throw CommonLib::CExcBase("[TShuffleZlibEncoder] uncompress error {0}, size {1}, expected {2}", res, (uint64_t)nOut, (uint64_t)nRaw);
			}

			for (uint32_t i = 0; i < nCount; ++i)
			{
				TValue v;
				byte_t* p = (byte_t*)&v;
				for (size_t b = 0; b < sizeof(TValue); ++b)
					p[b] = m_raw[b * nCount + i];
				vecValues.push_back(v);
			}
			m_nCount = nCount;
			UpdateRatio(nRaw, nSize);
		}

		void Clear() { m_nCount = 0; }
		uint32_t Count() const { return m_nCount; }

	private:
		void UpdateRatio(size_t nRaw, size_t nComp)
		{
			if (nRaw >= 256 && nComp > 0)
				m_dRatio = (std::max)(1.0, (double)nRaw / (double)nComp);
		}

	private:
		uint32_t m_nCount{ 0 };
		int32_t m_nLevel{ 6 };
		double m_dRatio{ 2.0 };
		std::vector<byte_t> m_raw;
		std::vector<byte_t> m_comp;
	};
}
