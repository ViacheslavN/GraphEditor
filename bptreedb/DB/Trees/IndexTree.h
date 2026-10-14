#pragma once

// The index trees ((value, row id) -> nothing): TIndexTree. IndexTrees*.cpp instantiate it.

#include "TreeTypes.h"

namespace bptreedb
{
	namespace db
	{
		namespace trees
		{
			//--------------------------------------------------------------------------------------------
			// indexes

			template <class K, bool bCut = false>
			struct TIndexKeys
			{
				typedef typename TIndexTypes<K, bCut>::TTree TTree;
				static TTree* NewTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, int64_t nInfoPage)
				{
					return new TTree(nInfoPage, ptrPager, ptrContext->m_ptrAlloc, ptrContext->m_nTreeCacheSize, ptrContext->m_nPageSize, 0);
				}
				static K MakeKey(TTree&, const CommonLib::CVariant& value) { return value.Get<K>(); }

				// the build keeps the keys as TItem: sorted like the tree (TOrderedComp)
				typedef K TItem;
				static TItem Item(const CommonLib::CVariant& value) { return value.Get<K>(); }
				static bool Less(const TItem& a, const TItem& b) { return TOrderedComp<K>().LE(a, b); }
				static bool Equal(const TItem& a, const TItem& b) { return TOrderedComp<K>().EQ(a, b); }
				static K Key(TTree&, const TItem& item) { return item; }
			};

			template <bool bCut>
			struct TIndexKeys<StringValue, bCut>
			{
				typedef typename TIndexTypes<StringValue, bCut>::TTree TTree;
				static TTree* NewTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, int64_t nInfoPage)
				{
					return new TTree(nInfoPage, ptrPager, ptrContext->m_ptrAlloc, ptrContext->m_nTreeCacheSize, ptrContext->m_nPageSize, MaxIndexStringLength);
				}
				static StringValue MakeKey(TTree& tree, const CommonLib::CVariant& value) { return tree.MakeKey(value.Get<CommonLib::astr_t>()); }

				// the build keeps std::string, sorted like CompStringValue (bytes up to the terminating zero)
				typedef std::string TItem;
				static TItem Item(const CommonLib::CVariant& value) { return value.Get<CommonLib::astr_t>(); }
				static bool Less(const TItem& a, const TItem& b) { return strcmp(a.c_str(), b.c_str()) < 0; }
				static bool Equal(const TItem& a, const TItem& b) { return strcmp(a.c_str(), b.c_str()) == 0; }
				static StringValue Key(TTree& tree, const TItem& item) { return tree.MakeKey(item); }
			};

			// bCut: the string keys by TStringMultiKeyCutEncoder (CIndex::FormatStringCut)
			template <class K, bool bCut = false>
			class TIndexTree : public IIndexTree, public std::enable_shared_from_this<TIndexTree<K, bCut> >
			{
			public:
				typedef TIndexKeys<K, bCut> TKeys;
				typedef typename TKeys::TTree TTree;
				typedef typename TIndexTypes<K, bCut>::TMKey TMKey;
				typedef typename TIndexTypes<K, bCut>::TComp TComp;
				typedef typename TTree::iterator iterator;

				TIndexTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, int64_t nInfoPage) :
					m_holder(ptrContext, std::unique_ptr<TTree>(TKeys::NewTree(ptrContext, ptrPager, nInfoPage)), nInfoPage,
						std::is_same<K, StringValue>::value ? MaxIndexStringLength : 0, false)
				{
				}

				virtual void SetMinSplit(bool bMinSplit) { m_holder.Tree().SetMinSplit(bMinSplit); }

				virtual uint64_t Build(IValueReaderPtr pReader, bool bUnique, bool& bChanged)
				{
					typedef typename TKeys::TItem TItem;
					std::vector<std::pair<TItem, int64_t> > items;
					int64_t nRowId = 0;
					CommonLib::CVariant value;
					while (pReader->Next(nRowId, value))
						items.emplace_back(TKeys::Item(value), nRowId);

					// the order of the tree: the key, then the row id
					std::sort(items.begin(), items.end(), [](const std::pair<TItem, int64_t>& a, const std::pair<TItem, int64_t>& b)
					{
						if (TKeys::Less(a.first, b.first)) return true;
						if (TKeys::Less(b.first, a.first)) return false;
						return a.second < b.second;
					});
					if (bUnique)
					{
						for (size_t i = 1; i < items.size(); ++i)
							if (TKeys::Equal(items[i - 1].first, items[i].first))
								throw CommonLib::CExcBase("the index is unique, rows {0} and {1} have the same value", items[i - 1].second, items[i].second);
					}

					struct SSource
					{
						TTree& m_tree;
						const std::vector<std::pair<TItem, int64_t> >& m_items;
						size_t m_nPos;
						bool Next(TMKey& key, uint8_t& v)
						{
							if (m_nPos == m_items.size())
								return false;
							const std::pair<TItem, int64_t>& item = m_items[m_nPos++];
							key = TMKey(TKeys::Key(m_tree, item.first), item.second);
							v = 0;
							return true;
						}
					} source{ m_holder.Tree(), items, 0 };

					bChanged = true;
					m_holder.Changed();
					return m_holder.Tree().BulkLoad(source);
				}

				virtual int64_t GetInfoPage() const { return m_holder.InfoPage(); }
				virtual void Flush() { m_holder.FlushTree(); }
				virtual uint64_t GetChangeCount() const { return m_holder.Changes(); }
				virtual void GetSpace(STreeSpace& space, bool /*bBlobs*/) { BPTreeSpace(m_holder.Tree(), space); }

				virtual void Insert(const CommonLib::CVariant& key, int64_t nRowId)
				{
					m_holder.Changed();
					m_holder.Tree().insert(TKeys::MakeKey(m_holder.Tree(), key), nRowId);
				}

				virtual bool Remove(const CommonLib::CVariant& key, int64_t nRowId)
				{
					m_holder.Changed();
					return m_holder.Tree().remove(TKeys::MakeKey(m_holder.Tree(), key), nRowId);
				}

				virtual bool Exists(const CommonLib::CVariant& key, int64_t* pRowId)
				{
					K k = TKeys::MakeKey(m_holder.Tree(), key);
					iterator it = m_holder.Tree().lower_bound_key(k);
					if (it.IsNull() || !m_comp.EQ(it.Key().m_key, k))
						return false;
					if (pRowId)
						*pRowId = it.Key().m_nSub;
					return true;
				}

				class CQuery : public IRowSource
				{
				public:
					CQuery(std::shared_ptr<TIndexTree> ptrIndex, K&& key, OpType op) : m_ptrIndex(ptrIndex), m_key(std::move(key)), m_op(op) {}

					virtual bool Next(int64_t& nRowId)
					{
						if (m_bEnd)
							return false;

						TTree& tree = m_ptrIndex->m_holder.Tree();
						if (!m_bStarted)
						{
							switch (m_op)
							{
							case OpEqual:
							case OpGreaterOrEqual: m_it = tree.lower_bound_key(m_key); break;
							case OpGreater: m_it = tree.upper_bound_key(m_key); break;
							default: m_it = tree.begin(); break;
							}
							m_bStarted = true;
						}
						else if (m_nChanges != m_ptrIndex->m_holder.Changes())
							m_it = tree.upper_bound(m_last);
						else if (!m_it.IsNull())
							m_it.Next();
						m_nChanges = m_ptrIndex->m_holder.Changes();

						const TComp& comp = m_ptrIndex->m_comp;
						for (; !m_it.IsNull(); m_it.Next())
						{
							const TMKey& k = m_it.Key();
							bool bStop = false, bSkip = false;
							switch (m_op)
							{
							case OpEqual: bStop = !comp.EQ(k.m_key, m_key); break;
							case OpLess: bStop = !comp.LE(k.m_key, m_key); break;
							case OpLessOrEqual: bStop = comp.LE(m_key, k.m_key); break;
							case OpNoEqual: bSkip = comp.EQ(k.m_key, m_key); break;
							default: break;
							}
							if (bStop)
								break;
							if (bSkip)
								continue;

							m_last = k;
							nRowId = k.m_nSub;
							return true;
						}
						m_bEnd = true;
						return false;
					}

				private:
					std::shared_ptr<TIndexTree> m_ptrIndex;
					K m_key;
					OpType m_op;
					iterator m_it;
					TMKey m_last;
					bool m_bStarted{ false };
					bool m_bEnd{ false };
					uint64_t m_nChanges{ 0 };
				};

				virtual IRowSourcePtr Query(const CommonLib::CVariant& key, OpType op)
				{
					return std::make_shared<CQuery>(this->shared_from_this(), TKeys::MakeKey(m_holder.Tree(), key), op);
				}

			private:
				TComp m_comp;
				TTreeHolder<TTree> m_holder;
			};

		}
	}
}
