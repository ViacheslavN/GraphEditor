// The row tree of a table (row id -> nothing) and the choice of the value storage of a field.

#include "TreeTypes.h"

namespace bptreedb
{
	namespace db
	{
		using namespace trees;

		namespace
		{
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
				virtual void GetSpace(STreeSpace& space, bool /*bBlobs*/) { BPTreeSpace(m_holder.Tree(), space); }

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
		}

		uint32_t MaxInlineString(uint32_t nPageSize)
		{
			return nPageSize / 8;
		}

		EValueStorage ChooseValueStorage(eDataTypes type, uint32_t nLength, uint32_t nPageSize)
		{
			if (IsNumberType(type))
				return evsNumberNumLen;
			if (type == dtString && nLength != 0 && nLength <= MaxInlineString(nPageSize))
				return evsStringCut;
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
	}
}
