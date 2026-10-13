#pragma once
#include "../../BPtreeLib/DeVector.h"
#include "../../BPtreeLib/BPTreeContext.h"
#include "../../BPtreeLib/AllocsSet.h"
#include "../../BPtreeLib/Compressor/CompressorParams.h"
#include "../../BPtreeLib/Compressor/EntropyEncoder/UnsignedNumLenEncoder.h"
#include "../../../CommonLib/stream/MemoryStream.h"
#include "ZOrderKey.h"
#include <memory>
#include <vector>

namespace bptreedb
{
	namespace spatiallib
	{
	namespace zorder
	{
		// Key encoder of the Z-order B+ tree (leaf and inner nodes, keys are sorted).
		// As TValueDiffEncoder: the first key raw, then the differences of the next keys, by NumLen.
		// A Z value of several 64 bit limbs (128 / 256 bits) is subtracted as a big number and every limb of
		// the difference goes to its own NumLen stream (the high limbs of close keys are 0 and cost almost
		// nothing). The id goes to one more stream: zigzag of the difference with the previous id.
		//
		//   limbs of the first Z (uint64 each), int64 first id
		//   per stream (limbs low..high, then id): uint32 size, NumLen stream
		template <class _TKey>
		class TZOrderKeyEncoder
		{
		public:
			typedef _TKey TKey;
			typedef typename TKey::ZOrder ZOrder;
			typedef TZLimbs<ZOrder> TLimbs;
			typedef CommonLib::STLAllocator<TKey> TAlloc;
			typedef TDeVector<TKey, TAlloc> TValueMemSet;

			static constexpr size_t LimbCount = TLimbs::Count;
			static constexpr size_t StreamCount = LimbCount + 1;
			static constexpr uint32_t FirstSize = (uint32_t)(LimbCount * sizeof(uint64_t) + sizeof(int64_t));

			struct SDiff
			{
				uint64_t m_limbs[LimbCount];
				uint64_t m_nId;     // zigzag
			};

			TZOrderKeyEncoder(TAllocsSetPtr pAllocsSet, TCompressorParamsBasePtr pParams, ECompressParams type)
			{
				for (size_t i = 0; i < StreamCount; ++i)
					m_encoders[i].reset(new CUnsignedNumLenEncoder(pAllocsSet, pParams, type));
			}

			static SDiff Diff(const TKey& cur, const TKey& prev)
			{
				uint64_t a[LimbCount], b[LimbCount];
				TLimbs::Get(cur.m_z, a);
				TLimbs::Get(prev.m_z, b);

				SDiff d;
				uint64_t borrow = 0;
				for (size_t i = 0; i < LimbCount; ++i)
				{
					uint64_t s = b[i] + borrow;
					uint64_t nextBorrow = (s < borrow) || (a[i] < s) ? 1 : 0;
					d.m_limbs[i] = a[i] - s;
					borrow = nextBorrow;
				}
				int64_t nId = (int64_t)((uint64_t)cur.m_nId - (uint64_t)prev.m_nId);
				d.m_nId = ((uint64_t)nId << 1) ^ (uint64_t)(nId >> 63);
				return d;
			}

			static TKey Apply(const TKey& prev, const SDiff& d)
			{
				uint64_t a[LimbCount], r[LimbCount];
				TLimbs::Get(prev.m_z, a);
				uint64_t carry = 0;
				for (size_t i = 0; i < LimbCount; ++i)
				{
					uint64_t s = a[i] + d.m_limbs[i];
					uint64_t c1 = s < a[i] ? 1 : 0;
					r[i] = s + carry;
					uint64_t c2 = r[i] < s ? 1 : 0;
					carry = c1 | c2;
				}
				TKey key;
				TLimbs::Set(key.m_z, r);
				int64_t nId = (int64_t)((d.m_nId >> 1) ^ (~(d.m_nId & 1) + 1));
				key.m_nId = (int64_t)((uint64_t)prev.m_nId + (uint64_t)nId);
				return key;
			}

			// statistics, same bookkeeping as TBaseValueDiffEncoder
			void AddSymbol(uint32_t nSize, uint32_t nIndex, const TKey& key, const TValueMemSet& vecKeys)
			{
				if (nSize > 1)
				{
					if (nIndex == 0)
						AddDiff(Diff(vecKeys[nIndex + 1], key));
					else
					{
						const TKey& prev = vecKeys[nIndex - 1];
						if (nIndex == nSize - 1)
							AddDiff(Diff(key, prev));
						else
						{
							const TKey& next = vecKeys[nIndex + 1];
							RemoveDiff(Diff(next, prev));
							AddDiff(Diff(key, prev));
							AddDiff(Diff(next, key));
						}
					}
				}
			}

			void RemoveSymbol(uint32_t nSize, int nIndex, const TKey& key, const TValueMemSet& vecKeys)
			{
				if (vecKeys.size() > 1)
				{
					if (nIndex == 0)
						RemoveDiff(Diff(vecKeys[nIndex + 1], key));
					else if (nIndex == (int)vecKeys.size() - 1)
						RemoveDiff(Diff(key, vecKeys[nIndex - 1]));
					else
					{
						const TKey& prev = vecKeys[nIndex - 1];
						const TKey& next = vecKeys[nIndex + 1];
						AddDiff(Diff(next, prev));
						RemoveDiff(Diff(key, prev));
						RemoveDiff(Diff(next, key));
					}
				}
			}

			void UpdateSymbol(uint32_t nIndex, TKey& newKey, const TKey& oldKey, const TValueMemSet& vecKeys)
			{
				RemoveSymbol((uint32_t)vecKeys.size(), nIndex, oldKey, vecKeys);
				AddSymbol((uint32_t)vecKeys.size(), nIndex, newKey, vecKeys);
			}

			uint32_t GetCompressSize() const
			{
				uint32_t nSize = FirstSize;
				for (size_t i = 0; i < StreamCount; ++i)
					nSize += (uint32_t)sizeof(uint32_t) + m_encoders[i]->GetCompressSize();
				return nSize;
			}

			void BeginEncoding(const TValueMemSet& vecKeys)
			{
			}

			uint32_t Encode(const TValueMemSet& vecKeys, CommonLib::IMemoryWriteStream* pStream, uint32_t maxCompSize, CBPTreeContext* pContext)
			{
				uint32_t n = (uint32_t)vecKeys.size();
				uint32_t nSplit = std::max<uint32_t>(1, n / 2);
				try
				{
					if (n == 0)
						return 0;

					size_t nStart = pStream->Pos();
					uint64_t limbs[LimbCount];
					TLimbs::Get(vecKeys[0].m_z, limbs);
					for (size_t i = 0; i < LimbCount; ++i)
						if (!pStream->WriteSafe(limbs[i]))
							return nSplit;
					if (!pStream->WriteSafe(vecKeys[0].m_nId))
						return nSplit;

					// the statistics from scratch: the streams must have exactly the symbols that are encoded
					m_diffs.resize(n);
					Clear();
					for (uint32_t i = 1; i < n; ++i)
					{
						m_diffs[i] = Diff(vecKeys[i], vecKeys[i - 1]);
						AddDiff(m_diffs[i]);
					}

					for (size_t s = 0; s < StreamCount; ++s)
					{
						size_t nSizePos = pStream->Pos();
						if (!pStream->WriteSafe((uint32_t)0))
							return nSplit;
						size_t nStreamStart = pStream->Pos();
						if (n > 1)
						{
							CUnsignedNumLenEncoder& enc = *m_encoders[s];
							if (!enc.BeginEncoding(pStream))
								return nSplit;
							for (uint32_t i = 1; i < n; ++i)
							{
								if (!enc.EncodeSymbol(Symbol(m_diffs[i], s)))
									return nSplit;
							}
							if (!enc.FinishEncoding(pStream))
								return nSplit;
						}
						size_t nEnd = pStream->Pos();
						pStream->Seek(nSizePos, CommonLib::soFromBegin);
						pStream->Write((uint32_t)(nEnd - nStreamStart));
						pStream->Seek(nEnd, CommonLib::soFromBegin);
					}

					if (pStream->Pos() - nStart > maxCompSize)
						return nSplit;
					return 0;
				}
				catch (std::exception& exc)
				{
					CommonLib::CExcBase::RegenExcT("[TZOrderKeyEncoder] failed to encode, count: {0}", n, exc);
					throw;
				}
			}

			void Decode(uint32_t nCount, TValueMemSet& vecKeys, CommonLib::IMemoryReadStream* pStream, uint32_t nCompSize, CBPTreeContext* pContext)
			{
				try
				{
					if (nCount == 0)
						return;

					TKey first;
					uint64_t limbs[LimbCount];
					for (size_t i = 0; i < LimbCount; ++i)
						limbs[i] = pStream->ReadIntu64();
					TLimbs::Set(first.m_z, limbs);
					first.m_nId = pStream->ReadInt64();

					m_diffs.resize(nCount);
					for (size_t s = 0; s < StreamCount; ++s)
					{
						uint32_t nSize = pStream->ReadIntu32();
						if (nCount > 1)
						{
							CommonLib::CReadMemoryStream stream;
							stream.AttachBuffer((byte_t*)pStream->Buffer() + pStream->Pos(), nSize);
							CUnsignedNumLenEncoder& dec = *m_encoders[s];
							dec.BeginDecoding(&stream);
							for (uint32_t i = 1; i < nCount; ++i)
							{
								uint64_t v = 0;
								dec.DecodeSymbol(v);
								SetSymbol(m_diffs[i], s, v);
							}
							dec.FinishDecoding(&stream);
						}
						pStream->Seek(nSize, CommonLib::soFromCurrent);
					}

					vecKeys.push_back(first);
					for (uint32_t i = 1; i < nCount; ++i)
						vecKeys.push_back(Apply(vecKeys[i - 1], m_diffs[i]));

					// statistics of the loaded keys
					Clear();
					for (uint32_t i = 1; i < nCount; ++i)
						AddDiff(m_diffs[i]);
				}
				catch (std::exception& exc)
				{
					CommonLib::CExcBase::RegenExcT("[TZOrderKeyEncoder] failed to decode, count: {0}", nCount, exc);
					throw;
				}
			}

			void Clear()
			{
				for (size_t i = 0; i < StreamCount; ++i)
					m_encoders[i]->Clear();
			}

		private:
			static uint64_t Symbol(const SDiff& d, size_t s)
			{
				return s < LimbCount ? d.m_limbs[s] : d.m_nId;
			}

			static void SetSymbol(SDiff& d, size_t s, uint64_t v)
			{
				if (s < LimbCount)
					d.m_limbs[s] = v;
				else
					d.m_nId = v;
			}

			void AddDiff(const SDiff& d)
			{
				for (size_t s = 0; s < StreamCount; ++s)
					m_encoders[s]->AddSymbol(Symbol(d, s));
			}

			void RemoveDiff(const SDiff& d)
			{
				for (size_t s = 0; s < StreamCount; ++s)
					m_encoders[s]->RemoveSymbol(Symbol(d, s));
			}

		private:
			std::unique_ptr<CUnsignedNumLenEncoder> m_encoders[StreamCount];
			std::vector<SDiff> m_diffs;
		};

		// value encoder that writes nothing: the map of the index has no values (the id is in the key)
		template <class _TValue>
		class TNullValueEncoder
		{
		public:
			typedef _TValue TValue;
			typedef CommonLib::STLAllocator<TValue> TAlloc;
			typedef TDeVector<TValue, TAlloc> TValueMemSet;

			TNullValueEncoder(TAllocsSetPtr, TCompressorParamsBasePtr, ECompressParams) {}

			void AddSymbol(uint32_t, int, const TValue&, const TValueMemSet&) {}
			void RemoveSymbol(uint32_t, int, const TValue&, const TValueMemSet&) {}
			void UpdateSymbol(int, TValue&, const TValue&, const TValueMemSet&) {}
			uint32_t GetCompressSize() const { return 0; }
			void BeginEncoding(const TValueMemSet&) {}
			uint32_t Encode(const TValueMemSet&, CommonLib::IMemoryWriteStream*, uint32_t, CBPTreeContext*) { return 0; }
			void Decode(uint32_t nCount, TValueMemSet& vecValues, CommonLib::IMemoryReadStream*, uint32_t, CBPTreeContext*)
			{
				for (uint32_t i = 0; i < nCount; ++i)
					vecValues.push_back(TValue());
			}
			void Clear() {}
		};
	}
	}
}
