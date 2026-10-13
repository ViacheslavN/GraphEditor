#include "DBTrees.h"
#include "Values.h"

#include "../../CommonLib/stream/MemoryStream.h"
#include "../../CommonLib/stream/FixedMemoryStream.h"
#include "../../CommonLib/data/blob.h"
#include "../../CommonLib/SpatialData/GeoShape.h"

#include "../BPtreeLib/BPMap.h"
#include "../BPtreeLib/BPMultiMap.h"
#include "../BPtreeLib/BPStringValueMap.h"
#include "../BPtreeLib/BPBlobValueMap.h"
#include "../BPtreeLib/BPTreeInnerNode.h"
#include "../BPtreeLib/BPTreeMapLeafNode.h"
#include "../BPtreeLib/BaseNodeCompressor.h"
#include "../BPtreeLib/BaseEncoder.h"
#include "../BPtreeLib/BaseDIffEncoder.h"
#include "../BPtreeLib/OrderedKey.h"
#include "../BPtreeLib/Compressor/EntropyEncoder/UnsignedNumLenEncoder.h"
#include "../BPtreeLib/Compressor/StringCompressor/ZLibStringEncoder.h"
#include "../BPtreeLib/Compressor/BlobCompressor/BlobValueEncoder.h"
#include "../BPtreeLib/Compressor/OrderedEncoder/OrderedDiffEncoder.h"
#include "../BPtreeLib/Compressor/OrderedEncoder/ShuffleZlibEncoder.h"

#include "../SpatialLib/RTree/RTree.h"
#include "../SpatialLib/ZOrderTree/BPZOrderGridIndex.h"

#include <algorithm>
#include <cstring>

namespace bptreedb
{
	namespace db
	{
		namespace
		{
			typedef TValueDiffEncoder<int64_t, int64_t, CUnsignedNumLenEncoder> TRowIdEncoder;   // sorted row ids
			typedef TBaseValueEncoder<int64_t, CUnsignedNumLenEncoder> TLinkEncoder;            // child addresses
			typedef TOrderedComp<int64_t> TRowIdComp;

			// a tree with row id keys
			template <class TValue, class TValueEncoder>
			struct TRowIdNodes
			{
				typedef BPTreeLeafNodeMap<int64_t, TValue, TBaseNodeCompressor<int64_t, TValue, TRowIdEncoder, TValueEncoder> > TLeaf;
				typedef BPTreeInnerNode<int64_t, TBaseNodeCompressor<int64_t, int64_t, TRowIdEncoder, TLinkEncoder> > TInner;
			};

			typedef TRowIdNodes<uint8_t, TNullValueEncoder<uint8_t> > TRowNodes;
			typedef TBPMap<int64_t, uint8_t, TRowIdComp, storage::IFilePager, TRowNodes::TInner, TRowNodes::TLeaf> TRowTreeImpl;

			template <class T>
			struct TNumberTree
			{
				typedef TRowIdNodes<T, TShuffleZlibEncoder<T> > TNodes;
				typedef TBPMap<int64_t, T, TRowIdComp, storage::IFilePager, typename TNodes::TInner, typename TNodes::TLeaf> TTree;
			};

			typedef TRowIdNodes<StringValue, CZlibStringComp> TStringNodes;
			typedef TBPStringValueMap<int64_t, TRowIdComp, storage::IFilePager, TStringNodes::TInner, TStringNodes::TLeaf> TStringTree;

			typedef TRowIdNodes<BlobValue, CBlobValueEncoder> TBlobNodes;
			typedef TBPBlobValueMap<int64_t, TRowIdComp, storage::IFilePager, TBlobNodes::TInner, TBlobNodes::TLeaf> TBlobTree;

			// indexes: (value, row id) keys
			template <class K>
			struct TIndexTypes
			{
				typedef TMultiKey<K> TMKey;
				typedef TOrderedComp<K> TComp;
				typedef TOrderedMultiKeyEncoder<K> TKeyEncoder;
				typedef BPTreeLeafNodeMap<TMKey, uint8_t, TBaseNodeCompressor<TMKey, uint8_t, TKeyEncoder, TNullValueEncoder<uint8_t> > > TLeaf;
				typedef BPTreeInnerNode<TMKey, TBaseNodeCompressor<TMKey, int64_t, TKeyEncoder, TLinkEncoder> > TInner;
				typedef TBPMultiMap<K, TComp, uint8_t, storage::IFilePager, TInner, TLeaf> TTree;
			};

			template <>
			struct TIndexTypes<StringValue>
			{
				typedef TMultiKey<StringValue> TMKey;
				typedef CompStringValue TComp;
				typedef TStringMultiKeyEncoder TKeyEncoder;
				typedef BPTreeLeafNodeMap<TMKey, uint8_t, TBaseNodeCompressor<TMKey, uint8_t, TKeyEncoder, TNullValueEncoder<uint8_t> > > TLeaf;
				typedef BPTreeInnerNode<TMKey, TBaseNodeCompressor<TMKey, int64_t, TKeyEncoder, TLinkEncoder> > TInner;
				typedef TBPMultiMap<StringValue, TComp, uint8_t, storage::IFilePager, TInner, TLeaf> TTree;
			};

			uint32_t BlobInlineSize(uint32_t nPageSize)
			{
				// (inline + 2 * 8 + 8) * 2 <= page size (TBPBlobValueMap)
				return (std::min)((uint32_t)TBlobTree::DefaultMaxInlineSize, nPageSize / 4);
			}

			//--------------------------------------------------------------------------------------------
			// common part of the trees: creation, flush, change counter

			template <class TTree>
			class TTreeHolder
			{
			public:
				// bMinSplit: the keys are inserted in ascending order (row ids)
				TTreeHolder(SDBContextPtr ptrContext, std::unique_ptr<TTree>&& ptrTree, int64_t nInfoPage, uint32_t nMaxLen, bool bMinSplit) :
					m_ptrContext(ptrContext), m_ptrTree(std::move(ptrTree))
				{
					if (nInfoPage == -1)
					{
						m_ptrTree->InnitTree(ptrContext->MakeTreeParams(nMaxLen), bMinSplit, 0, storage::eEmptyType, 0, storage::eEmptyType);
						m_bChanged = true;
					}
					m_ptrTree->SetMinSplit(bMinSplit);
				}

				TTree& Tree() { return *m_ptrTree; }
				int64_t InfoPage() const { return m_ptrTree->GetPageBTreeInfo(); }
				uint64_t Changes() const { return m_nChanges; }
				void Changed() { ++m_nChanges; m_bChanged = true; }

				void FlushTree()
				{
					if (!m_bChanged)
						return;
					m_ptrTree->Flush();
					m_bChanged = false;
				}

			protected:
				SDBContextPtr m_ptrContext;
				std::unique_ptr<TTree> m_ptrTree;
				uint64_t m_nChanges{ 0 };
				bool m_bChanged{ false };
			};

			//--------------------------------------------------------------------------------------------
			// row tree

			class CRowTree : public IRowTree, public std::enable_shared_from_this<CRowTree>
			{
			public:
				typedef TRowTreeImpl::iterator iterator;

				CRowTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, int64_t nInfoPage) :
					m_holder(ptrContext, std::unique_ptr<TRowTreeImpl>(new TRowTreeImpl(nInfoPage, ptrPager, ptrContext->m_ptrAlloc,
						ptrContext->m_nTreeCacheSize, ptrContext->m_nPageSize)), nInfoPage, 0, true)
				{
				}

				virtual int64_t GetInfoPage() const { return m_holder.InfoPage(); }
				virtual void Flush() { m_holder.FlushTree(); }
				virtual uint64_t GetChangeCount() const { return m_holder.Changes(); }

				virtual void Insert(int64_t nRowId)
				{
					m_holder.Changed();
					m_holder.Tree().insert(nRowId, 0);
				}

				virtual bool Remove(int64_t nRowId)
				{
					m_holder.Changed();
					return m_holder.Tree().remove(nRowId);
				}

				virtual bool Exists(int64_t nRowId)
				{
					return !m_holder.Tree().find(nRowId).IsNull();
				}

				class CScan : public IRowSource
				{
				public:
					CScan(std::shared_ptr<CRowTree> ptrTree) : m_ptrTree(ptrTree) {}
					virtual bool Next(int64_t& nRowId)
					{
						TRowTreeImpl& tree = m_ptrTree->m_holder.Tree();
						if (!m_bStarted)
						{
							m_it = tree.begin();
							m_bStarted = true;
						}
						else if (m_nChanges != m_ptrTree->m_holder.Changes())
							m_it = tree.upper_bound(m_nLast);
						else if (!m_it.IsNull())
							m_it.Next();

						m_nChanges = m_ptrTree->m_holder.Changes();
						if (m_it.IsNull())
							return false;
						nRowId = m_nLast = m_it.Key();
						return true;
					}
				private:
					std::shared_ptr<CRowTree> m_ptrTree;
					iterator m_it;
					bool m_bStarted{ false };
					int64_t m_nLast{ 0 };
					uint64_t m_nChanges{ 0 };
				};

				virtual IRowSourcePtr Scan()
				{
					return std::make_shared<CScan>(shared_from_this());
				}

			private:
				TTreeHolder<TRowTreeImpl> m_holder;
			};

			//--------------------------------------------------------------------------------------------
			// value fields: the traits convert the values between the tree and CVariant

			template <class T>
			struct TNumberValues
			{
				typedef typename TNumberTree<T>::TTree TTree;

				static TTree* NewTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, int64_t nInfoPage, uint32_t /*nLength*/)
				{
					return new TTree(nInfoPage, ptrPager, ptrContext->m_ptrAlloc, ptrContext->m_nTreeCacheSize, ptrContext->m_nPageSize);
				}
				static void Insert(TTree& tree, eDataTypes, int64_t nRowId, const CommonLib::CVariant& value)
				{
					tree.insert(nRowId, value.Get<T>());
				}
				static void Get(TTree&, eDataTypes, const typename TTree::iterator& it, CommonLib::CVariant& value)
				{
					value = it.Value();
				}
			};

			struct TStringValues
			{
				typedef TStringTree TTree;

				static TTree* NewTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, int64_t nInfoPage, uint32_t nLength)
				{
					return new TTree(nInfoPage, ptrPager, ptrContext->m_ptrAlloc, ptrContext->m_nTreeCacheSize, ptrContext->m_nPageSize, nLength);
				}
				static void Insert(TTree& tree, eDataTypes, int64_t nRowId, const CommonLib::CVariant& value)
				{
					tree.insert(nRowId, value.Get<CommonLib::astr_t>());
				}
				static void Get(TTree&, eDataTypes, const typename TTree::iterator& it, CommonLib::CVariant& value)
				{
					const StringValue& sv = it.Value();
					value = CommonLib::astr_t((const char*)sv.CStr(), sv.StrLen());
				}
			};

			struct TBlobValues
			{
				typedef TBlobTree TTree;

				static TTree* NewTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, int64_t nInfoPage, uint32_t /*nLength*/)
				{
					return new TTree(nInfoPage, ptrPager, ptrContext->m_ptrAlloc, ptrContext->m_nTreeCacheSize, ptrContext->m_nPageSize,
						BlobInlineSize(ptrContext->m_nPageSize));
				}
				static void Insert(TTree& tree, eDataTypes type, int64_t nRowId, const CommonLib::CVariant& value)
				{
					switch (type)
					{
					case dtString:
						{
							const CommonLib::astr_t& s = value.Get<CommonLib::astr_t>();
							tree.insert(nRowId, (const byte_t*)s.data(), (int64_t)s.size());
							break;
						}
					case dtBlob:
						{
							const CommonLib::CBlobPtr_t& ptrBlob = value.Get<CommonLib::CBlobPtr_t>();
							tree.insert(nRowId, ptrBlob->Buffer(), (int64_t)ptrBlob->Size());
							break;
						}
					case dtGeometry:
						{
							const CommonLib::IGeoShapePtr& ptrShape = value.Get<CommonLib::IGeoShapePtr>();
							tree.insert(nRowId, ptrShape->Data(), (int64_t)ptrShape->Size());
							break;
						}
					default:
						throw CommonLib::CExcBase("a blob tree can't keep values of type {0}", DataTypeName(type));
					}
				}
				static void Get(TTree& tree, eDataTypes type, const typename TTree::iterator& it, CommonLib::CVariant& value)
				{
					IBlobPtr ptrBlob = tree.GetBlob(it);
					int64_t nSize = ptrBlob->Size();
					if (nSize > (int64_t)0x7FFFFFFF)
						throw CommonLib::CExcBase("blob of {0} bytes is too big to read in memory", nSize);

					// read straight into the buffer of the value
					CommonLib::ReadStreamPtr ptrStream = nSize != 0 ? ptrBlob->GetReadStream() : CommonLib::ReadStreamPtr();
					if (type == dtString)
					{
						CommonLib::astr_t s((size_t)nSize, '\0');
						if (nSize != 0)
							ptrStream->Read((byte_t*)&s[0], (size_t)nSize);
						value = s;     // CVariant copies (no move assignment)
						return;
					}

					CommonLib::CBlobPtr_t ptrData = std::make_shared<CommonLib::Data::CBlob>((uint32_t)nSize);
					if (nSize != 0)
						ptrStream->Read(ptrData->Buffer(), (size_t)nSize);

					if (type == dtGeometry)
						value = MakeShape(ptrData->Buffer(), ptrData->Size());   // CGeoShape::Import copies the shape blob
					else
						value = ptrData;
				}
			};

			template <class TValues>
			class TValueField : public IValueField, public std::enable_shared_from_this<TValueField<TValues> >
			{
			public:
				typedef typename TValues::TTree TTree;
				typedef typename TTree::iterator iterator;

				TValueField(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, eDataTypes type, uint32_t nLength, int64_t nInfoPage) :
					m_type(type), m_holder(ptrContext, std::unique_ptr<TTree>(TValues::NewTree(ptrContext, ptrPager, nInfoPage, nLength)), nInfoPage, nLength, true)
				{
				}

				virtual int64_t GetInfoPage() const { return m_holder.InfoPage(); }
				virtual void Flush() { m_holder.FlushTree(); }
				virtual uint64_t GetChangeCount() const { return m_holder.Changes(); }

				virtual void Insert(int64_t nRowId, const CommonLib::CVariant& value)
				{
					m_holder.Changed();
					TValues::Insert(m_holder.Tree(), m_type, nRowId, value);
				}

				virtual bool Remove(int64_t nRowId)
				{
					m_holder.Changed();
					return m_holder.Tree().remove(nRowId);
				}

				virtual bool Find(int64_t nRowId, CommonLib::CVariant& value)
				{
					iterator it = m_holder.Tree().find(nRowId);
					if (it.IsNull())
					{
						value = CommonLib::CVariant();
						return false;
					}
					TValues::Get(m_holder.Tree(), m_type, it, value);
					return true;
				}

				class CReader : public IValueReader
				{
				public:
					CReader(std::shared_ptr<TValueField> ptrField) : m_ptrField(ptrField) {}

					virtual bool Seek(int64_t nRowId, CommonLib::CVariant& value)
					{
						TTree& tree = m_ptrField->m_holder.Tree();
						// the iterator is at the first entry >= m_nSeekFrom
						if (m_bSeekValid && m_nSeekChanges == m_ptrField->m_holder.Changes() && m_nSeekFrom <= nRowId)
						{
							// ascending row ids: a few steps forward are cheaper than a search
							int i = 0;
							for (; i < 8 && !m_seekIt.IsNull() && m_seekIt.Key() < nRowId; ++i)
								m_seekIt.Next();

							if (m_seekIt.IsNull() || !(m_seekIt.Key() < nRowId))
							{
								m_nSeekFrom = nRowId;
								if (!m_seekIt.IsNull() && m_seekIt.Key() == nRowId)
									return Get(m_seekIt, value);
								return Null(value);
							}
						}

						m_seekIt = tree.lower_bound(nRowId);
						m_bSeekValid = true;
						m_nSeekFrom = nRowId;
						m_nSeekChanges = m_ptrField->m_holder.Changes();
						if (!m_seekIt.IsNull() && m_seekIt.Key() == nRowId)
							return Get(m_seekIt, value);
						return Null(value);
					}

					virtual bool Next(int64_t& nRowId, CommonLib::CVariant& value)
					{
						TTree& tree = m_ptrField->m_holder.Tree();
						if (!m_bScanStarted)
						{
							m_scanIt = tree.begin();
							m_bScanStarted = true;
						}
						else if (m_nScanChanges != m_ptrField->m_holder.Changes())
							m_scanIt = tree.upper_bound(m_nScanLast);
						else if (!m_scanIt.IsNull())
							m_scanIt.Next();

						m_nScanChanges = m_ptrField->m_holder.Changes();
						if (m_scanIt.IsNull())
							return false;

						nRowId = m_nScanLast = m_scanIt.Key();
						Get(m_scanIt, value);
						return true;
					}

				private:
					bool Get(const iterator& it, CommonLib::CVariant& value)
					{
						TValues::Get(m_ptrField->m_holder.Tree(), m_ptrField->m_type, it, value);
						return true;
					}
					bool Null(CommonLib::CVariant& value)
					{
						value = CommonLib::CVariant();
						return false;
					}

				private:
					std::shared_ptr<TValueField> m_ptrField;
					iterator m_seekIt;
					bool m_bSeekValid{ false };
					int64_t m_nSeekFrom{ 0 };
					uint64_t m_nSeekChanges{ 0 };
					iterator m_scanIt;
					bool m_bScanStarted{ false };
					int64_t m_nScanLast{ 0 };
					uint64_t m_nScanChanges{ 0 };
				};

				virtual IValueReaderPtr CreateReader()
				{
					return std::make_shared<CReader>(this->shared_from_this());
				}

			private:
				eDataTypes m_type;
				TTreeHolder<TTree> m_holder;
			};

			//--------------------------------------------------------------------------------------------
			// indexes

			template <class K>
			struct TIndexKeys
			{
				typedef typename TIndexTypes<K>::TTree TTree;
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

			template <>
			struct TIndexKeys<StringValue>
			{
				typedef TIndexTypes<StringValue>::TTree TTree;
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

			template <class K>
			class TIndexTree : public IIndexTree, public std::enable_shared_from_this<TIndexTree<K> >
			{
			public:
				typedef TIndexKeys<K> TKeys;
				typedef typename TKeys::TTree TTree;
				typedef typename TIndexTypes<K>::TMKey TMKey;
				typedef typename TIndexTypes<K>::TComp TComp;
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

			//--------------------------------------------------------------------------------------------
			// spatial indexes

			bool MatchMode(const CommonLib::bbox& bb, const CommonLib::bbox& extent, eSpatialQueryMode mode)
			{
				bool bIntersects = !(bb.xMax < extent.xMin || extent.xMax < bb.xMin || bb.yMax < extent.yMin || extent.yMax < bb.yMin);
				switch (mode)
				{
				case sqmIntersect: return bIntersects;
				case sqmOver: return bb.xMin <= extent.xMin && bb.yMin <= extent.yMin && extent.xMax <= bb.xMax && extent.yMax <= bb.yMax;
				case sqmInside: return extent.xMin <= bb.xMin && extent.yMin <= bb.yMin && bb.xMax <= extent.xMax && bb.yMax <= extent.yMax;
				default:
					throw CommonLib::CExcBase("spatial query mode {0} isn't supported", (int)mode);
				}
			}

			void SortUnique(std::vector<int64_t>& rows)
			{
				std::sort(rows.begin(), rows.end());
				rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
			}

			class CRTreeIndex : public ISpatialIndexTree
			{
			public:
				typedef spatiallib::rtree::TRTree<double> TTree;
				typedef TTree::TRect TRect;

				CRTreeIndex(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, int64_t nInfoPage)
				{
					spatiallib::rtree::SRTreeParams params;
					params.m_nCompressLevel = (int32_t)ptrContext->m_nCompressLevel;
					m_ptrTree.reset(new TTree(nInfoPage, ptrPager, ptrContext->m_nSpatialCacheSize, params));
					m_ptrTree->Open();   // a new tree is written by Open
				}

				virtual int64_t GetInfoPage() const { return m_ptrTree->GetPageInfo(); }
				virtual uint64_t GetChangeCount() const { return m_nChanges; }
				virtual void Flush()
				{
					if (m_bChanged)
						m_ptrTree->Flush();
					m_bChanged = false;
				}

				virtual void Insert(const CommonLib::bbox& bb, int64_t nRowId)
				{
					++m_nChanges;
					m_bChanged = true;
					m_ptrTree->insert(ToRect(bb), nRowId);
				}

				virtual bool Remove(const CommonLib::bbox& bb, int64_t nRowId)
				{
					++m_nChanges;
					m_bChanged = true;
					return m_ptrTree->remove(ToRect(bb), nRowId);
				}

				virtual void Query(const CommonLib::bbox& extent, eSpatialQueryMode mode, std::vector<int64_t>& rows, bool& bExact)
				{
					m_ptrTree->search(ToRect(extent), [&](const TRect& r, int64_t nRowId)
					{
						CommonLib::bbox bb;
						bb.xMin = r.m_minX; bb.yMin = r.m_minY; bb.xMax = r.m_maxX; bb.yMax = r.m_maxY;
						if (MatchMode(bb, extent, mode))
							rows.push_back(nRowId);
						return true;
					});
					SortUnique(rows);
					bExact = true;
				}

				virtual uint64_t Build(std::vector<std::pair<CommonLib::bbox, int64_t> >& entries)
				{
					std::vector<TTree::TEntry> treeEntries;
					treeEntries.reserve(entries.size());
					for (const auto& e : entries)
						treeEntries.emplace_back(ToRect(e.first), e.second);
					std::vector<std::pair<CommonLib::bbox, int64_t> >().swap(entries);

					uint64_t nCount = treeEntries.size();
					++m_nChanges;
					m_bChanged = true;
					m_ptrTree->BulkLoad(treeEntries);     // STR packing, writes the nodes
					return nCount;
				}

			private:
				static TRect ToRect(const CommonLib::bbox& bb) { return TRect(bb.xMin, bb.yMin, bb.xMax, bb.yMax); }

				std::unique_ptr<TTree> m_ptrTree;
				uint64_t m_nChanges{ 0 };
				bool m_bChanged{ false };
			};

			class CZOrderIndex : public ISpatialIndexTree
			{
			public:
				typedef spatiallib::zorder::TBPZOrderGridIndex<spatiallib::zorder::TZOrderRectTraits<spatiallib::ZOrderRect2DU32> > TIndex;
				typedef TIndex::TGrid TGrid;
				typedef TIndex::TRectD TRectD;

				CZOrderIndex(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, const SSpatialIndexParams& params, int64_t nInfoPage)
				{
					TGrid grid(TRectD(params.m_extent.xMin, params.m_extent.yMin, params.m_extent.xMax, params.m_extent.yMax), params.m_dCellSize);
					m_ptrIndex.reset(new TIndex(nInfoPage, ptrPager, ptrContext->m_ptrAlloc, ptrContext->m_nSpatialCacheSize, ptrContext->m_nPageSize, grid));
					if (nInfoPage == -1)
					{
						m_ptrIndex->Create();
						m_bChanged = true;
					}
				}

				virtual int64_t GetInfoPage() const { return m_ptrIndex->GetPageInfo(); }
				virtual uint64_t GetChangeCount() const { return m_nChanges; }
				virtual void Flush()
				{
					if (m_bChanged)
						m_ptrIndex->Flush();
					m_bChanged = false;
				}

				virtual void Insert(const CommonLib::bbox& bb, int64_t nRowId)
				{
					++m_nChanges;
					m_bChanged = true;
					m_ptrIndex->insert(ToRect(bb), nRowId);
				}

				virtual bool Remove(const CommonLib::bbox& bb, int64_t nRowId)
				{
					++m_nChanges;
					m_bChanged = true;
					return m_ptrIndex->remove(ToRect(bb), nRowId);
				}

				virtual void Query(const CommonLib::bbox& extent, eSpatialQueryMode mode, std::vector<int64_t>& rows, bool& bExact)
				{
					if (mode != sqmIntersect && mode != sqmOver && mode != sqmInside)
						throw CommonLib::CExcBase("spatial query mode {0} isn't supported", (int)mode);

					// the extent clipped by the grid: the index keeps nothing outside it
					const TRectD& ge = m_ptrIndex->Grid().Extent();
					TRectD q((std::max)(extent.xMin, ge.m_minX), (std::max)(extent.yMin, ge.m_minY), (std::min)(extent.xMax, ge.m_maxX), (std::min)(extent.yMax, ge.m_maxY));
					if (q.m_maxX < q.m_minX || q.m_maxY < q.m_minY)
						return;

					m_ptrIndex->search(q, [&](const TRectD&, int64_t nRowId) { rows.push_back(nRowId); return true; });
					SortUnique(rows);
					bExact = false;   // candidates by the grid cells
				}

				virtual uint64_t Build(std::vector<std::pair<CommonLib::bbox, int64_t> >& entries)
				{
					std::vector<std::pair<TRectD, int64_t> > objects;
					objects.reserve(entries.size());
					for (const auto& e : entries)
						objects.emplace_back(ToRect(e.first), e.second);
					std::vector<std::pair<CommonLib::bbox, int64_t> >().swap(entries);

					++m_nChanges;
					m_bChanged = true;
					return m_ptrIndex->BulkLoad(objects);     // sorted Z keys, bulk load of the B+ tree
				}

			private:
				static TRectD ToRect(const CommonLib::bbox& bb) { return TRectD(bb.xMin, bb.yMin, bb.xMax, bb.yMax); }

				std::unique_ptr<TIndex> m_ptrIndex;
				uint64_t m_nChanges{ 0 };
				bool m_bChanged{ false };
			};
		}

		uint32_t MaxInlineString(uint32_t nPageSize)
		{
			return nPageSize / 8;
		}

		EValueStorage ChooseValueStorage(eDataTypes type, uint32_t nLength, uint32_t nPageSize)
		{
			if (IsNumberType(type))
				return evsNumber;
			if (type == dtString && nLength != 0 && nLength <= MaxInlineString(nPageSize))
				return evsString;
			if (type == dtString || type == dtBlob || type == dtGeometry)
				return evsBlob;
			throw CommonLib::CExcBase("unsupported field type {0}", DataTypeName(type));
		}

		IRowTreePtr CreateRowTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, int64_t nInfoPage)
		{
			try
			{
				return std::make_shared<CRowTree>(ptrContext, ptrPager, nInfoPage);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to open the row tree, info page {0}", nInfoPage, excSrc);
				throw;
			}
		}

		IValueFieldPtr CreateValueField(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, eDataTypes type, EValueStorage storageType, uint32_t nLength, int64_t nInfoPage)
		{
			try
			{
				switch (storageType)
				{
				case evsString: return std::make_shared<TValueField<TStringValues> >(ptrContext, ptrPager, type, nLength, nInfoPage);
				case evsBlob: return std::make_shared<TValueField<TBlobValues> >(ptrContext, ptrPager, type, nLength, nInfoPage);
				case evsNumber:
					break;
				default:
					throw CommonLib::CExcBase("unknown value storage {0}", (int)storageType);
				}

				switch (type)
				{
				case dtInteger8: return std::make_shared<TValueField<TNumberValues<int8_t> > >(ptrContext, ptrPager, type, nLength, nInfoPage);
				case dtInteger16: return std::make_shared<TValueField<TNumberValues<int16_t> > >(ptrContext, ptrPager, type, nLength, nInfoPage);
				case dtInteger32: return std::make_shared<TValueField<TNumberValues<int32_t> > >(ptrContext, ptrPager, type, nLength, nInfoPage);
				case dtInteger64:
				case dtOid: return std::make_shared<TValueField<TNumberValues<int64_t> > >(ptrContext, ptrPager, type, nLength, nInfoPage);
				case dtUInteger8: return std::make_shared<TValueField<TNumberValues<uint8_t> > >(ptrContext, ptrPager, type, nLength, nInfoPage);
				case dtUInteger16: return std::make_shared<TValueField<TNumberValues<uint16_t> > >(ptrContext, ptrPager, type, nLength, nInfoPage);
				case dtUInteger32: return std::make_shared<TValueField<TNumberValues<uint32_t> > >(ptrContext, ptrPager, type, nLength, nInfoPage);
				case dtUInteger64: return std::make_shared<TValueField<TNumberValues<uint64_t> > >(ptrContext, ptrPager, type, nLength, nInfoPage);
				case dtFloat: return std::make_shared<TValueField<TNumberValues<float> > >(ptrContext, ptrPager, type, nLength, nInfoPage);
				case dtDouble: return std::make_shared<TValueField<TNumberValues<double> > >(ptrContext, ptrPager, type, nLength, nInfoPage);
				default:
					throw CommonLib::CExcBase("type {0} isn't a number", DataTypeName(type));
				}
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to open the value tree of a {0} field, info page {1}", DataTypeName(type), nInfoPage, excSrc);
				throw;
			}
		}

		IIndexTreePtr CreateIndexTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, eDataTypes type, int64_t nInfoPage)
		{
			try
			{
				switch (type)
				{
				case dtInteger8: return std::make_shared<TIndexTree<int8_t> >(ptrContext, ptrPager, nInfoPage);
				case dtInteger16: return std::make_shared<TIndexTree<int16_t> >(ptrContext, ptrPager, nInfoPage);
				case dtInteger32: return std::make_shared<TIndexTree<int32_t> >(ptrContext, ptrPager, nInfoPage);
				case dtInteger64:
				case dtOid: return std::make_shared<TIndexTree<int64_t> >(ptrContext, ptrPager, nInfoPage);
				case dtUInteger8: return std::make_shared<TIndexTree<uint8_t> >(ptrContext, ptrPager, nInfoPage);
				case dtUInteger16: return std::make_shared<TIndexTree<uint16_t> >(ptrContext, ptrPager, nInfoPage);
				case dtUInteger32: return std::make_shared<TIndexTree<uint32_t> >(ptrContext, ptrPager, nInfoPage);
				case dtUInteger64: return std::make_shared<TIndexTree<uint64_t> >(ptrContext, ptrPager, nInfoPage);
				case dtFloat: return std::make_shared<TIndexTree<float> >(ptrContext, ptrPager, nInfoPage);
				case dtDouble: return std::make_shared<TIndexTree<double> >(ptrContext, ptrPager, nInfoPage);
				case dtString: return std::make_shared<TIndexTree<StringValue> >(ptrContext, ptrPager, nInfoPage);
				default:
					throw CommonLib::CExcBase("fields of type {0} can't be indexed", DataTypeName(type));
				}
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to open the index tree of a {0} field, info page {1}", DataTypeName(type), nInfoPage, excSrc);
				throw;
			}
		}

		ISpatialIndexTreePtr CreateSpatialIndexTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, const SSpatialIndexParams& params, int64_t nInfoPage)
		{
			try
			{
				switch (params.m_type)
				{
				case itSpatialRTree: return std::make_shared<CRTreeIndex>(ptrContext, ptrPager, nInfoPage);
				case itSpatialZOrder: return std::make_shared<CZOrderIndex>(ptrContext, ptrPager, params, nInfoPage);
				default:
					throw CommonLib::CExcBase("index type {0} isn't spatial", (int)params.m_type);
				}
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to open the spatial index, info page {0}", nInfoPage, excSrc);
				throw;
			}
		}
	}
}
