#include "OSMNodeTreeBuilder.h"

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            void CNodeTreeBuilder::BuildInner(std::vector<TLevelEntry>& level)
            {
                try
                {
                    if (this->m_pRoot.get() == nullptr)
                        this->LoadTree();
                    if (!this->m_pRoot->IsLeaf() || this->m_pRoot->Count() != 0)
                        throw CommonLib::CExcBase("the tree isn't empty");

                    if (level.size() == 1)
                    {
                        // one leaf: its elements go to the root (it has a fixed address)
                        TBPTreeNodePtr pLeaf = this->LoadNodeFromStorage(level[0].second);
                        for (uint32_t i = 0, n = pLeaf->Count(); i < n; ++i)
                            this->m_pRoot->AppendInLeaf(pLeaf->Key(i), pLeaf->Value(i));
                        this->m_pStorage->DropFilePage(level[0].second);
                        this->m_pRoot->SetFlags(bptreedb::CHANGE_NODE, true);
                    }

                    while (level.size() > 1)
                    {
                        if (FitsInOneInnerNode(level))
                        {
                            this->m_pRoot->Clear();
                            this->m_pRoot->TransformToInner(this->m_pAllocsSet, this->m_nNodePageSize, this->m_pCompressParams);
                            this->m_pRoot->SetLess(level[0].second);
                            for (size_t i = 1; i < level.size(); ++i)
                                this->m_pRoot->InsertInInnerNode(this->m_comp, level[i].first, level[i].second);
                            this->m_pRoot->SetFlags(bptreedb::CHANGE_NODE, true);
                            break;
                        }

                        std::vector<TLevelEntry> upper;
                        TBPTreeNodePtr pPrevInner;
                        size_t i = 0;
                        while (i < level.size())
                        {
                            TBPTreeNodePtr pInner = this->NewNode(false, false);
                            int64_t firstKey = level[i].first;
                            pInner->SetLess(level[i].second);
                            size_t nStart = ++i;

                            while (i < level.size())
                            {
                                pInner->InsertInInnerNode(this->m_comp, level[i].first, level[i].second);
                                if (pInner->IsNeedSplit())
                                {
                                    pInner->RemoveByIndex(pInner->Count() - 1);
                                    break;
                                }
                                ++i;
                            }

                            while (this->TrySaveNode(pInner) != 0)
                            {
                                if (i <= nStart + 1)
                                    throw CommonLib::CExcBase("an inner node doesn't fit");
                                pInner->RemoveByIndex(pInner->Count() - 1);
                                --i;
                            }

                            if (pInner->Count() == 0)
                            {
                                // the last group got a single child: an inner node without keys is not allowed,
                                // it gets the last child of the previous node (or the child moves there)
                                if (pPrevInner.get() == nullptr)
                                    throw CommonLib::CExcBase("an inner node without keys");
                                TBPTreeNodePtr& pPrev = pPrevInner;
                                if (pPrev->Count() >= 2)
                                {
                                    uint32_t nLast = pPrev->Count() - 1;
                                    int64_t lastKey = pPrev->Key(nLast);
                                    int64_t lastLink = pPrev->Link(nLast);
                                    pPrev->RemoveByIndex(nLast);
                                    pInner->InsertInInnerNode(this->m_comp, firstKey, pInner->Less());
                                    pInner->SetLess(lastLink);
                                    firstKey = lastKey;
                                    if (this->TrySaveNode(pPrev) != 0 || this->TrySaveNode(pInner) != 0)
                                        throw CommonLib::CExcBase("failed to rebalance the last inner node");
                                }
                                else
                                {
                                    pPrev->InsertInInnerNode(this->m_comp, firstKey, pInner->Less());
                                    if (this->TrySaveNode(pPrev) != 0)
                                        throw CommonLib::CExcBase("failed to merge the last inner node");
                                    this->m_pStorage->DropFilePage(pInner->GetAddr());
                                    break;   // pPrev is in upper already
                                }
                            }

                            upper.emplace_back(firstKey, pInner->GetAddr());
                            pPrevInner = pInner;
                        }
                        level.swap(upper);
                    }
                    this->Flush();
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExcT("[CNodeTreeBuilder] failed to build the inner levels, leaves: {0}", (uint64_t)level.size(), exc);
                    throw;
                }
            }

            bool CNodeTreeBuilder::FitsInOneInnerNode(const std::vector<TLevelEntry>& level)
            {
                TBPTreeNodePtr pTest = this->AllocateNewNode(-1, false);
                pTest->SetLess(level[0].second);
                for (size_t i = 1; i < level.size(); ++i)
                {
                    pTest->InsertInInnerNode(this->m_comp, level[i].first, level[i].second);
                    if (pTest->IsNeedSplit())
                        return false;
                }
                return true;
            }
        }
    }
}
