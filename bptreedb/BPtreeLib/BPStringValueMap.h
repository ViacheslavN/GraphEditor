#pragma once

#include "BPMap.h"
#include "StringVal.h"

namespace bptreedb
{
	// B+ tree map with string values (any key type of TBPMap, e.g. int64_t).
	//
	// The values are StringValue in the tree's string allocator (eStringAlloc); typically the leaves
	// compress them with CZlibStringComp:
	//   BPTreeLeafNodeMap<int64_t, StringValue, TBaseNodeCompressor<int64_t, StringValue, TKeyEncoder, CZlibStringComp>>
	// A value is at most nMaxValueLen bytes (default 1024): one entry has to fit in a leaf page with room
	// for other entries, so the limit must stay well below the page size.
	template <class _TKey, class _TComp, class _TStorage,
		class _TInnerNode,
		class _TLeafNode,
		class _TNodeHolder = TBPMapNodeHolder<_TInnerNode, _TLeafNode>
	>
	class TBPStringValueMap : public TBPMap<_TKey, StringValue, _TComp, _TStorage, _TInnerNode, _TLeafNode, _TNodeHolder>
	{
	public:
		typedef TBPMap<_TKey, StringValue, _TComp, _TStorage, _TInnerNode, _TLeafNode, _TNodeHolder> TBase;
		typedef typename TBase::TKey TKey;
		typedef typename TBase::TStorage TStorage;
		typedef typename TBase::iterator iterator;

		static const uint32_t DefaultMaxValueLen = 1024;

		TBPStringValueMap(int64_t nPageBTreeInfo, std::shared_ptr<TStorage> pStorage, CommonLib::IAllocPtr pAlloc, uint32_t nCacheSize, uint32_t nNodesPageSize,
			uint32_t nMaxValueLen = DefaultMaxValueLen, bool bMulti = false, bool bMinSplit = false) :
			TBase(nPageBTreeInfo, pStorage, pAlloc, nCacheSize, nNodesPageSize, bMulti, bMinSplit),
			m_nMaxValueLen(nMaxValueLen)
		{
			if (m_nMaxValueLen * 2 > nNodesPageSize)
				throw CommonLib::CExcBase("TBPStringValueMap: max value length {0} is too big for the page size {1}", m_nMaxValueLen, nNodesPageSize);

			m_pStringAlloc = this->m_pAllocsSet->GetAlloc(eStringAlloc);
		}

		using TBase::insert;

		void insert(const TKey& key, const char* pszValue, uint32_t nValueLen)
		{
			TBase::insert(key, MakeValue(pszValue, nValueLen));
		}

		void insert(const TKey& key, const std::string& value)
		{
			insert(key, value.c_str(), (uint32_t)value.size());
		}

		// a value in the tree's string allocator; throws if it is longer than the maximum
		StringValue MakeValue(const char* pszValue, uint32_t nValueLen) const
		{
			if (nValueLen > m_nMaxValueLen)
				throw CommonLib::CExcBase("TBPStringValueMap: value length {0} exceeds the maximum {1}", nValueLen, m_nMaxValueLen);

			return StringValue((const byte_t*)pszValue, nValueLen, m_pStringAlloc);
		}

		uint32_t MaxValueLen() const { return m_nMaxValueLen; }

	private:
		uint32_t m_nMaxValueLen;
		CommonLib::IAllocPtr m_pStringAlloc;
	};
}
