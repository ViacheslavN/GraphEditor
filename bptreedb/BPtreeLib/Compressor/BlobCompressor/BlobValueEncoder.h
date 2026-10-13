#pragma once
#include "../../DeVector.h"
#include "../CompressorParams.h"
#include "../../../../CommonLib/CommonLib.h"
#include "../../../../CommonLib/exception/exc_base.h"
#include "../../../../CommonLib/alloc/alloc.h"
#include "../../../../CommonLib/alloc/stl_alloc.h"
#include "../../BlobVal.h"
#include "../../BPTreeContext.h"
#include "../../AllocsSet.h"

namespace bptreedb
{
	// Value encoder of the blob map leaves (no compression), per value:
	//   int64 addr
	//   addr == 0: uint16 size, size bytes   (inline blob)
	//   addr != 0: nothing more              (blob in the stream pages starting at addr, the size is in the stream)
	class CBlobValueEncoder
	{
	public:
		typedef CommonLib::STLAllocator<BlobValue> TAlloc;
		typedef TDeVector<BlobValue, TAlloc> TValueMemSet;

		CBlobValueEncoder(TAllocsSetPtr pAllocsSet, TCompressorParamsBasePtr pParamsBase, ECompressParams type);
		~CBlobValueEncoder();

		void AddSymbol(uint32_t nSize, int nIndex, const BlobValue& value, const TValueMemSet& vecValues);
		void RemoveSymbol(uint32_t nSize, int nIndex, const BlobValue& value, const TValueMemSet& vecValues);
		void UpdateSymbol(int nIndex, BlobValue& newValue, const BlobValue& oldValue, const TValueMemSet& vecValues);
		uint32_t GetCompressSize() const;
		void BeginEncoding(const TValueMemSet& vecValues);
		uint32_t Encode(const TValueMemSet& vecValues, CommonLib::IMemoryWriteStream* pStream, uint32_t maxCompSize, CBPTreeContext* pContext);
		void Decode(uint32_t nCount, TValueMemSet& vecValues, CommonLib::IMemoryReadStream* pStream, uint32_t nCompSize, CBPTreeContext* pContext);
		void Clear();
		uint32_t Count() const;

	private:
		uint32_t m_nCount{ 0 };
		uint32_t m_nRowSize{ 0 };
		CommonLib::IAllocPtr m_pAlloc;
	};
}
