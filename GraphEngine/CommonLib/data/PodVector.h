#pragma once
#include "CommonLib.h"
#include "../alloc/simpleAlloc.h"
#include "../exception/exc_base.h"

namespace CommonLib {
    namespace Data {

        template<class _TValue>
        class TPodVector
        {
        public:


            template<class T> inline void swap_elements(T& a, T& b)
            {
                T temp = a;
                a = b;
                b = temp;
            }

            typedef _TValue TValue;
            typedef TPodVector<TValue> TSelfType;

            TPodVector(uint32_t nSize = 0, IAllocPtr pAlloc = IAllocPtr()) : m_nSize(0), m_pAlloc(pAlloc), m_pData(NULL), m_nCapacity(nSize)
            {
                if(m_pAlloc.get() == nullptr)
                    m_pAlloc = std::make_shared<CSimpleAlloc>();

                if(m_nCapacity != 0)
                {
                    m_pData = (TValue *)m_pAlloc->Alloc(m_nCapacity);
                }
            }

            TPodVector(const TSelfType& vec) : m_nSize(vec.m_nSize),  m_pData(NULL), m_nCapacity(vec.m_nCapacity), m_pAlloc(vec.m_pAlloc)
            {
                m_pAlloc = vec.m_pAlloc;
                if(m_nSize != 0)
                {
                    m_pData = (TValue *)m_pAlloc->Alloc(m_nCapacity);
                    memcpy(m_pData, vec.m_pData, m_nSize);
                }
            }

            const TSelfType& operator = (const TSelfType& vec)
            {
                free();
                m_pAlloc = vec.m_pAlloc;
                if(vec.size() != 0)
                {
                    resize(vec.size());
                    memcpy(m_pData, vec.m_pData, sizeof(TValue) * m_nSize);
                }

                return *this;
            }

            ~TPodVector()
            {
                free();
            }

            TValue& operator [](uint32_t nIndex)
            {
                if(nIndex >= m_nSize)
                    throw CExcBase("Out of range");

                return m_pData[nIndex];
            }
            const TValue& operator [](uint32_t nIndex) const
            {
                if(nIndex >= m_nSize)
                    throw CExcBase("Out of range");

                return m_pData[nIndex];
            }

            const TValue* begin() const
            {
                return m_pData;
            }

            TValue* begin()
            {
                return m_pData;
            }

            bool reserve(uint32_t nSize, bool bClear = false)
            {
                if(nSize > m_nCapacity)
                {
                    m_nCapacity = nSize;
                    TValue* pTmp = (TValue*)m_pAlloc->Alloc(m_nCapacity * sizeof(TValue));
                    if(!pTmp)
                        return false;

                    if(m_pData)
                    {
                        if(!bClear)
                            memcpy(pTmp, m_pData, m_nSize* sizeof(TValue));
                        m_pAlloc->Free(m_pData);
                    }
                    m_pData = pTmp;
                }
                if(bClear)
                    m_nSize = 0;
                return true;
            }
            void resize(uint32_t nSize)
            {
                if(nSize > m_nSize)
                {
                    if(m_pData)
                        m_pAlloc->Free(m_pData);

                    m_pData = (TValue *)m_pAlloc->Alloc(nSize * sizeof(TValue));
                    m_nCapacity = nSize;
                }

                m_nSize = nSize;
            }

            uint32_t size() const {return m_nSize;}
            uint32_t capacity() const{return m_nCapacity;}
            bool empty() const {return m_nSize == 0;}
            void clear()
            {
                m_nSize = 0;
            }
            void free()
            {
                if(m_pData)
                {
                    m_pAlloc->Free(m_pData);
                    m_nSize = 0;
                    m_nCapacity = 0;
                    m_pData = NULL;
                }
            }

            void push_back(const TValue& value)
            {
                if(m_nSize == m_nCapacity)
                    reserve( m_nSize != 0 ? m_nSize *2 : 2);

                m_pData[m_nSize] = value;
                m_nSize++;
            }

            bool insert(uint32_t idx, const TValue& value)
            {
                if(idx > m_nSize)
                   throw CExcBase("Out of range");


                if((m_nSize + 1) > m_nCapacity)
                {
                    if(!m_nCapacity)
                        m_nCapacity = 1;
                    reserve(2 * m_nCapacity);
                }

                if(m_nSize == idx)
                {
                    push_back(value);
                    return true;
                }

                ::memmove( m_pData + idx + 1, m_pData + idx, ( m_nSize - idx) * sizeof( TValue ) );
                m_pData[idx] = value;
                m_nSize++;
                return true;
            }

            bool copy(const TPodVector& vec, size_t nPos, size_t nBegin, size_t nEnd)
            {
                size_t nLen = nEnd - nBegin;
                if(m_nSize + nPos + nLen >= m_nCapacity)
                {
                    if(!reserve((2 * m_nCapacity) > (m_nSize + nPos + nLen) ? 2 * m_nCapacity :  2 * (m_nSize + nPos + nLen) ))
                        return false;
                }
                memcpy(m_pData + nPos, vec.m_pData + nBegin,  nLen * sizeof(TValue));

                if((nPos + nLen) > m_nSize)
                    m_nSize += (nPos + nLen) - m_nSize;
                return true;
            }

            void push_back(const TSelfType& vec)
            {
                uint32_t nNewSize = m_nSize + vec.size();
                if(nNewSize >= m_nCapacity)
                    reserve(2 * m_nCapacity > nNewSize ? 2* m_nCapacity : nNewSize);

                memcpy(m_pData + m_nSize, vec.m_pData,  vec.size() * sizeof(TValue));

                m_nSize += vec.size();
            }

            bool remove(uint32_t idx)
            {
                if(idx > m_nSize)
                    return false;

                ::memmove( m_pData + idx, m_pData + idx + 1,
                           ( m_nSize - idx - 1 )*sizeof( TValue ) );
                m_nSize--;
                return true;
            }
            template<class _TComp >
            void quick_sort(_TComp& comp) //TO DO need inner
            {
                if(m_nSize < 2) return;

                TValue* e1;
                TValue* e2;

                int  stack[80];
                int* top = stack;
                int  limit = m_nSize;
                int  base = 0;

                for(;;)
                {
                    int len = limit - base;

                    int i;
                    int j;
                    int pivot;

                    if(len > 9)
                    {
                        // we use base + len/2 as the pivot
                        pivot = base + len / 2;
                        swap_elements(m_pData[base], m_pData[pivot]);

                        i = base + 1;
                        j = limit - 1;

                        // now ensure that *i <= *base <= *j
                        e1 = &(m_pData[j]);
                        e2 = &(m_pData[i]);
                        if(comp.LE(*e1, *e2)) swap_elements(*e1, *e2);

                        e1 = &(m_pData[base]);
                        e2 = &(m_pData[i]);
                        if(comp.LE(*e1, *e2)) swap_elements(*e1, *e2);

                        e1 = &(m_pData[j]);
                        e2 = &(m_pData[base]);
                        if(comp.LE(*e1, *e2)) swap_elements(*e1, *e2);

                        for(;;)
                        {
                            do i++; while(comp.LE(m_pData[i], m_pData[base]));
                            do j--; while(comp.LE(m_pData[base], m_pData[j]));

                            if(i > j)
                            {
                                break;
                            }

                            swap_elements(m_pData[i], m_pData[j]);
                        }

                        swap_elements(m_pData[base], m_pData[j]);

                        // now, push the largest sub-array
                        if(j - base > limit - i)
                        {
                            top[0] = base;
                            top[1] = j;
                            base   = i;
                        }
                        else
                        {
                            top[0] = i;
                            top[1] = limit;
                            limit  = j;
                        }
                        top += 2;
                    }
                    else
                    {
                        // the sub-array is small, perform insertion sort
                        j = base;
                        i = j + 1;

                        for(; i < limit; j = i, i++)
                        {
                            for(; comp.LE(*(e1 = &(m_pData[j + 1])), *(e2 = &(m_pData[j]))); j--)
                            {
                                swap_elements(*e1, *e2);
                                if(j == base)
                                {
                                    break;
                                }
                            }
                        }
                        if(top > stack)
                        {
                            top  -= 2;
                            base  = top[0];
                            limit = top[1];
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
        private:
            TValue *m_pData;
            IAllocPtr m_pAlloc;

            uint32_t m_nSize;
            uint32_t m_nCapacity;
        };
    }

}
