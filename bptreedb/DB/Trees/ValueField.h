#pragma once

// The value trees of the fields (row id -> value): TValueField over the traits of the value kinds.
// ValueFields*.cpp instantiate it (split by the value types).

#include "TreeTypes.h"

namespace bptreedb
{
	namespace db
	{
		namespace trees
		{
			//--------------------------------------------------------------------------------------------
			// value fields: the traits convert the values between the tree and CVariant

			// bNumLen: TNumberValueEncoder (evsNumberNumLen), otherwise TShuffleZlibEncoder (evsNumber, older files)
			template <class T, bool bNumLen = true>
			struct TNumberValues
			{
				typedef typename TNumberTree<T, typename std::conditional<bNumLen, TNumberValueEncoder<T>, TShuffleZlibEncoder<T> >::type>::TTree TTree;

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
				static bool BlobPages(TTree&, STreeSpace&) { return false; }   // no big blobs
			};

			// bCut: TStringCutTree (evsStringCut), otherwise TStringTree (evsString, older files)
			template <bool bCut = true>
			struct TStringValues
			{
				typedef typename std::conditional<bCut, TStringCutTree, TStringTree>::type TTree;

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
				static bool BlobPages(TTree&, STreeSpace&) { return false; }   // the strings are in the leaves
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
				// the stream pages of the big blobs (the leaves are read)
				static bool BlobPages(TTree& tree, STreeSpace& space)
				{
					for (typename TTree::iterator it = tree.begin(); !it.IsNull(); it.Next())
					{
						const BlobValue& value = it.Value();
						if (!value.IsInline())
							space.m_nBlobPages += tree.StreamPageCount(value);
					}
					return true;
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
				virtual void GetSpace(STreeSpace& space, bool bBlobs)
				{
					BPTreeSpace(m_holder.Tree(), space);
					if (bBlobs)
						space.m_bBlobsCounted = TValues::BlobPages(m_holder.Tree(), space);
				}

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

			// the value tree of a number field by its storage
			template <class T>
			IValueFieldPtr MakeNumberValueField(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, eDataTypes type, EValueStorage storageType, uint32_t nLength, int64_t nInfoPage)
			{
				switch (storageType)
				{
				case evsNumberNumLen: return std::make_shared<TValueField<TNumberValues<T, true> > >(ptrContext, ptrPager, type, nLength, nInfoPage);
				case evsNumber: return std::make_shared<TValueField<TNumberValues<T, false> > >(ptrContext, ptrPager, type, nLength, nInfoPage);
				default:
					throw CommonLib::CExcBase("value storage {0} isn't of numbers", (int)storageType);
				}
			}
		}
	}
}
