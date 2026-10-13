#pragma once
#include "BPMap.h"
#include "OrderedKey.h"
#include "StringVal.h"
#include <limits>
#include <type_traits>

namespace bptreedb
{
	// B+ tree with duplicate keys: every entry is (key, row id [, value]), the tree key is TMultiKey
	// (key, row id), so the tree itself stays unique. All entries of a key are next to each other, ordered
	// by the row id. It is the "FieldType -> rowId" index of a table: no value (TValue = uint8_t with
	// TNullValueEncoder), the row id in the key.
	//
	// Node types: the key encoder works on TMultiKey<TKey>:
	//   numbers (int, float, double): TOrderedMultiKeyEncoder<T>, comparator TOrderedComp<T> (or any comparator
	//   with the same order as TOrderedBits);
	//   strings: TStringMultiKeyEncoder, comparator CompStringValue; a key is at most nMaxKeyLen bytes (default 256).
	//
	// The trees of BPtreeLib have unique tree keys: duplicates of a field key are distinct (key, row id) keys.
	template <class _TKey, class _TKeyComp, class _TValue, class _TStorage,
		class _TInnerNode,
		class _TLeafNode,
		class _TNodeHolder = TBPMapNodeHolder<_TInnerNode, _TLeafNode>
	>
	class TBPMultiMap : public TBPMap<TMultiKey<_TKey>, _TValue, TMultiKeyComp<_TKeyComp>, _TStorage, _TInnerNode, _TLeafNode, _TNodeHolder>
	{
	public:
		typedef TBPMap<TMultiKey<_TKey>, _TValue, TMultiKeyComp<_TKeyComp>, _TStorage, _TInnerNode, _TLeafNode, _TNodeHolder> TBase;
		typedef _TKey TFieldKey;
		typedef _TKeyComp TFieldComp;
		typedef TMultiKey<_TKey> TKey;
		typedef _TValue TValue;
		typedef typename TBase::TStorage TStorage;
		typedef typename TBase::iterator iterator;
		static constexpr bool IsString = std::is_same<_TKey, StringValue>::value;
		static constexpr uint32_t DefaultMaxKeyLen = 256;

		TBPMultiMap(int64_t nPageBTreeInfo, std::shared_ptr<TStorage> pStorage, CommonLib::IAllocPtr pAlloc, uint32_t nCacheSize, uint32_t nNodesPageSize,
			uint32_t nMaxKeyLen = DefaultMaxKeyLen, bool bMinSplit = false) :
			TBase(nPageBTreeInfo, pStorage, pAlloc, nCacheSize, nNodesPageSize, bMinSplit), m_nMaxKeyLen(nMaxKeyLen)
		{
			if constexpr (IsString)
			{
				if (m_nMaxKeyLen * 2 > nNodesPageSize)
					throw CommonLib::CExcBase("TBPMultiMap: max key length {0} is too big for the page size {1}", m_nMaxKeyLen, nNodesPageSize);
				m_pStringAlloc = this->m_pAllocsSet->GetAlloc(eStringAlloc);
			}
		}

		using TBase::insert;
		using TBase::remove;

		// throws if (key, row id) is in the tree already
		void insert(const TFieldKey& key, int64_t nRowId, const TValue& value = TValue())
		{
			CheckKey(key);
			TBase::insert(TKey(key, nRowId), value);
		}

		bool remove(const TFieldKey& key, int64_t nRowId)
		{
			return TBase::remove(TKey(key, nRowId));
		}

		bool contains(const TFieldKey& key, int64_t nRowId)
		{
			return !TBase::find(TKey(key, nRowId)).IsNull();
		}

		// the first entry of the key (or the first entry after it, if the key isn't there)
		iterator lower_bound_key(const TFieldKey& key)
		{
			return TBase::lower_bound(TKey(key, (std::numeric_limits<int64_t>::min)()));
		}

		// the first entry after all entries of the key
		iterator upper_bound_key(const TFieldKey& key)
		{
			return TBase::upper_bound(TKey(key, (std::numeric_limits<int64_t>::max)()));
		}

		// func(row id, value) for every entry of the key, in row id order; returns the number of calls.
		// func returns false to stop.
		template <class TFunc>
		uint64_t ForEach(const TFieldKey& key, TFunc func)
		{
			uint64_t n = 0;
			for (iterator it = lower_bound_key(key); !it.IsNull() && this->m_comp.m_comp.EQ(it.Key().m_key, key); it.Next())
			{
				++n;
				if (!func(it.Key().m_nSub, it.Value()))
					break;
			}
			return n;
		}

		uint64_t count(const TFieldKey& key)
		{
			return ForEach(key, [](int64_t, const TValue&) { return true; });
		}

		// strings: a key in the tree's string allocator (throws if it is longer than the maximum)
		template <class T = TFieldKey>
		typename std::enable_if<std::is_same<T, StringValue>::value, StringValue>::type MakeKey(const char* pszUtf8, uint32_t nLen) const
		{
			if (nLen > m_nMaxKeyLen)
				throw CommonLib::CExcBase("TBPMultiMap: key length {0} exceeds the maximum {1}", nLen, m_nMaxKeyLen);
			return StringValue((const byte_t*)pszUtf8, nLen, m_pStringAlloc);
		}

		template <class T = TFieldKey>
		typename std::enable_if<std::is_same<T, StringValue>::value, StringValue>::type MakeKey(const std::string& s) const
		{
			return MakeKey(s.c_str(), (uint32_t)s.size());
		}

		uint32_t MaxKeyLen() const { return m_nMaxKeyLen; }

	private:
		void CheckKey(const TFieldKey& key) const
		{
			if constexpr (IsString)
			{
				if (key.StrLen() > m_nMaxKeyLen)
					throw CommonLib::CExcBase("TBPMultiMap: key length {0} exceeds the maximum {1}", key.StrLen(), m_nMaxKeyLen);
			}
		}

	private:
		uint32_t m_nMaxKeyLen;
		CommonLib::IAllocPtr m_pStringAlloc;
	};
}
