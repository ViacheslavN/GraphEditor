#pragma once

#include "../../CommonLib/CommonLib.h"
#include "../../CommonLib/exception/exc_base.h"
#include "../../CommonLib/alloc/alloc.h"
#include "../../CommonLib/alloc/simpleAlloc.h"
#include "../../CommonLib/alloc/stl_alloc.h"
#include <stack>
#include <set>


namespace bptreedb
{

	class IReadTreeCancel
	{
		public:
			virtual bool IsCancel() = 0;
	};

	template <class _TKey, class _TBTreeNode, class _TBTree>
	class TBPTreeStatistics
	{
	public:
		typedef _TKey      TKey;
		typedef int64_t     TLink;
		typedef _TBTreeNode TBTreeNode;
		typedef std::shared_ptr<TBTreeNode> TBTreeNodePtr;

		typedef CommonLib::STLAllocator<TKey> TAlloc;
		typedef std::vector<TKey, TAlloc> TKeyMemSet;
		typedef _TBTree TBTree;

		struct SNodeInfo
		{

			SNodeInfo(TBTreeNodePtr _pBTNode, int32_t _nPos) : pBTNode(_pBTNode), nPos(_nPos)
			{}

			TBTreeNodePtr pBTNode;
			int32_t nPos;
		};

		TBPTreeStatistics(TBTree *pTree) :
			m_pTree(pTree), m_bFinishRead(false)
		{
 
		}

		~TBPTreeStatistics()
		{

		}

		// Walks the whole tree and counts the nodes and the keys exactly (every leaf is loaded; the node
		// cache is trimmed after every leaf, so memory stays bounded). The old version multiplied the
		// key count of the first leaf of an inner node by the number of its children.
		void CalcNodesInTree(IReadTreeCancel *pCancel = nullptr)
		{
			try
			{
				m_bFinishRead = false;
				Clear();

				TBTreeNodePtr pRootNode = m_pTree->Root();
				if (!pRootNode.get())
					throw CommonLib::CExcBase("Root not found");

				if (pRootNode->IsLeaf())
				{
					m_nLeafNodeCount = 1;
					m_nKeyCount = pRootNode->Count();
					m_bFinishRead = true;
					return;
				}

				m_nInnerNodeCount = 1;
				std::stack<SNodeInfo> nNodes;
				nNodes.push(SNodeInfo(pRootNode, -1));

				while (!nNodes.empty())
				{
					if (pCancel && pCancel->IsCancel())
						return;

					SNodeInfo& node = nNodes.top();
					if (node.nPos == (int32_t)node.pBTNode->Count())
					{
						nNodes.pop();
						continue;
					}

					int32_t nPos = node.nPos++;
					TLink nLink = nPos == -1 ? node.pBTNode->Less() : node.pBTNode->Link(nPos);
					TBTreeNodePtr pNode = m_pTree->GetNode(nLink);
					if (!pNode.get())
						throw CommonLib::CExcBase("node {0} not found", nLink);

					if (pNode->IsLeaf())
					{
						m_nLeafNodeCount += 1;
						m_nKeyCount += pNode->Count();
						m_setHeights.insert(nNodes.size());       // inner levels above the leaf (as before)
						m_pTree->CheckCache();
						continue;
					}

					m_nInnerNodeCount += 1;
					nNodes.push(SNodeInfo(pNode, -1));
				}

				if (!m_setHeights.empty())
					m_nHeight = *m_setHeights.rbegin();
				m_bFinishRead = true;
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExcT("[TBPTreeStatistics] failed CalcNodesInTree", exc);
				throw;
			}
		}


		void Clear()
		{
			m_nLeafNodeCount = 0;
			m_nInnerNodeCount = 0;
			m_nKeyCount = 0;
			m_nHeight = 0;
			m_setHeights.clear();
		}

		


	public:

		TBTree *m_pTree{ nullptr };
		bool m_bFinishRead{ false };

		uint64_t m_nLeafNodeCount{ 0 };
		uint64_t m_nInnerNodeCount{ 0 };
		uint64_t m_nKeyCount{ 0 };
		uint64_t m_nHeight{ 0 };
		std::set<uint64_t> m_setHeights;
	};

}
