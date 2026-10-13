#pragma once
#include "../../DeVector.h"
#include "../../BPTreeContext.h"
#include "../../AllocsSet.h"
#include "../../OrderedKey.h"
#include "../../StringVal.h"
#include "../CompressorParams.h"
#include "../EntropyEncoder/UnsignedNumLenEncoder.h"
#include "../StringCompressor/ZLibStringEncoder.h"
#include "../../../../CommonLib/stream/MemoryStream.h"
#include <memory>
#include <vector>

namespace bptreedb
{
	inline uint64_t ZigZag(int64_t v) { return ((uint64_t)v << 1) ^ (uint64_t)(v >> 63); }
	inline int64_t UnZigZag(uint64_t v) { return (int64_t)((v >> 1) ^ (~(v & 1) + 1)); }

	// Key encoder of sorted keys (leaf and inner nodes): the first key raw, then the differences of the
	// next keys as NStreams unsigned symbols, every symbol kind in its own NumLen stream.
	// TTraits:
	//   static constexpr size_t Streams;
	//   static void Diff(const TKey& cur, const TKey& prev, uint64_t* pSymbols);
	//   static void Apply(const TKey& prev, const uint64_t* pSymbols, TKey& cur);
	//   static void WriteFirst(const TKey&, CommonLib::IMemoryWriteStream*) / bool WriteFirstSafe(...)
	//   static void ReadFirst(TKey&, CommonLib::IMemoryReadStream*); static uint32_t FirstSize();
	// The statistics follow TBaseValueDiffEncoder (AddSymbol / RemoveSymbol with the neighbours).
	//
	//   first key, per stream: uint32 size, NumLen stream
	template <class _TKey, class _TTraits>
	class TDiffStreamsEncoder
	{
	public:
		typedef _TKey TKey;
		typedef _TTraits TTraits;
		typedef CommonLib::STLAllocator<TKey> TAlloc;
		typedef TDeVector<TKey, TAlloc> TValueMemSet;
		static constexpr size_t Streams = TTraits::Streams;

		struct SDiff { uint64_t m_s[Streams]; };

		TDiffStreamsEncoder(TAllocsSetPtr pAllocsSet, TCompressorParamsBasePtr pParams, ECompressParams type)
		{
			for (size_t i = 0; i < Streams; ++i)
				m_encoders[i].reset(new CUnsignedNumLenEncoder(pAllocsSet, pParams, type));
		}

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
			uint32_t nSize = TTraits::FirstSize();
			for (size_t i = 0; i < Streams; ++i)
				nSize += (uint32_t)sizeof(uint32_t) + m_encoders[i]->GetCompressSize();
			return nSize;
		}

		void BeginEncoding(const TValueMemSet&) {}

		uint32_t Encode(const TValueMemSet& vecKeys, CommonLib::IMemoryWriteStream* pStream, uint32_t maxCompSize, CBPTreeContext* pContext)
		{
			uint32_t n = (uint32_t)vecKeys.size();
			uint32_t nSplit = (std::max)(1u, n / 2);
			try
			{
				if (n == 0)
					return 0;
				size_t nStart = pStream->Pos();
				if (!TTraits::WriteFirstSafe(vecKeys[0], pStream))
					return nSplit;

				// the statistics from scratch: the streams must have exactly the symbols that are encoded
				m_diffs.resize(n);
				Clear();
				for (uint32_t i = 1; i < n; ++i)
				{
					m_diffs[i] = Diff(vecKeys[i], vecKeys[i - 1]);
					AddDiff(m_diffs[i]);
				}

				for (size_t s = 0; s < Streams; ++s)
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
							if (!enc.EncodeSymbol(m_diffs[i].m_s[s]))
								return nSplit;
						if (!enc.FinishEncoding(pStream))
							return nSplit;
					}
					size_t nEnd = pStream->Pos();
					pStream->Seek(nSizePos, CommonLib::soFromBegin);
					pStream->Write((uint32_t)(nEnd - nStreamStart));
					pStream->Seek(nEnd, CommonLib::soFromBegin);
				}
				return pStream->Pos() - nStart > maxCompSize ? nSplit : 0;
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExcT("[TDiffStreamsEncoder] failed to encode, count: {0}", n, exc);
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
				TTraits::ReadFirst(first, pStream);

				m_diffs.resize(nCount);
				for (size_t s = 0; s < Streams; ++s)
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
							m_diffs[i].m_s[s] = v;
						}
						dec.FinishDecoding(&stream);
					}
					pStream->Seek(nSize, CommonLib::soFromCurrent);
				}

				vecKeys.push_back(first);
				for (uint32_t i = 1; i < nCount; ++i)
				{
					TKey key;
					TTraits::Apply(vecKeys[i - 1], m_diffs[i].m_s, key);
					vecKeys.push_back(key);
				}

				Clear();
				for (uint32_t i = 1; i < nCount; ++i)
					AddDiff(m_diffs[i]);
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExcT("[TDiffStreamsEncoder] failed to decode, count: {0}", nCount, exc);
				throw;
			}
		}

		void Clear()
		{
			for (size_t i = 0; i < Streams; ++i)
				m_encoders[i]->Clear();
		}

	private:
		static SDiff Diff(const TKey& cur, const TKey& prev)
		{
			SDiff d;
			TTraits::Diff(cur, prev, d.m_s);
			return d;
		}

		void AddDiff(const SDiff& d)
		{
			for (size_t s = 0; s < Streams; ++s)
				m_encoders[s]->AddSymbol(d.m_s[s]);
		}

		void RemoveDiff(const SDiff& d)
		{
			for (size_t s = 0; s < Streams; ++s)
				m_encoders[s]->RemoveSymbol(d.m_s[s]);
		}

	private:
		std::unique_ptr<CUnsignedNumLenEncoder> m_encoders[Streams];
		std::vector<SDiff> m_diffs;
	};

	// sorted numbers (int, float, double): differences of TOrderedBits, modulo 2^64 (lossless in any order)
	template <class T>
	struct TOrderedDiffTraits
	{
		static constexpr size_t Streams = 1;
		static uint32_t FirstSize() { return sizeof(T); }
		static void Diff(const T& cur, const T& prev, uint64_t* s) { s[0] = TOrderedBits<T>::ToBits(cur) - TOrderedBits<T>::ToBits(prev); }
		static void Apply(const T& prev, const uint64_t* s, T& cur) { cur = TOrderedBits<T>::FromBits(TOrderedBits<T>::ToBits(prev) + s[0]); }
		static bool WriteFirstSafe(const T& v, CommonLib::IMemoryWriteStream* p) { return p->WriteSafe((const byte_t*)&v, sizeof(T)); }
		static void ReadFirst(T& v, CommonLib::IMemoryReadStream* p) { p->ReadBytes((byte_t*)&v, sizeof(T)); }
	};

	// (number, row id) keys of TBPMultiMap: the number as above, the row id as zigzag of the difference
	template <class T>
	struct TOrderedMultiDiffTraits
	{
		typedef TMultiKey<T> TKey;
		static constexpr size_t Streams = 2;
		static uint32_t FirstSize() { return sizeof(T) + sizeof(int64_t); }
		static void Diff(const TKey& cur, const TKey& prev, uint64_t* s)
		{
			s[0] = TOrderedBits<T>::ToBits(cur.m_key) - TOrderedBits<T>::ToBits(prev.m_key);
			s[1] = ZigZag((int64_t)((uint64_t)cur.m_nSub - (uint64_t)prev.m_nSub));
		}
		static void Apply(const TKey& prev, const uint64_t* s, TKey& cur)
		{
			cur.m_key = TOrderedBits<T>::FromBits(TOrderedBits<T>::ToBits(prev.m_key) + s[0]);
			cur.m_nSub = (int64_t)((uint64_t)prev.m_nSub + (uint64_t)UnZigZag(s[1]));
		}
		static bool WriteFirstSafe(const TKey& k, CommonLib::IMemoryWriteStream* p)
		{
			return p->WriteSafe((const byte_t*)&k.m_key, sizeof(T)) && p->WriteSafe(k.m_nSub);
		}
		static void ReadFirst(TKey& k, CommonLib::IMemoryReadStream* p)
		{
			p->ReadBytes((byte_t*)&k.m_key, sizeof(T));
			k.m_nSub = p->ReadInt64();
		}
	};

	// unique number keys (int / float / double) sorted by TOrderedComp
	template <class T>
	using TOrderedDiffEncoder = TDiffStreamsEncoder<T, TOrderedDiffTraits<T> >;

	// TMultiKey<number> keys
	template <class T>
	using TOrderedMultiKeyEncoder = TDiffStreamsEncoder<TMultiKey<T>, TOrderedMultiDiffTraits<T> >;

	// TMultiKey<StringValue> keys: the strings by CZlibStringComp (one zlib block), the row ids as zigzag
	// differences by NumLen.
	//   uint32 size of the string block, string block, int64 first row id, uint32 size, NumLen stream
	class TStringMultiKeyEncoder
	{
	public:
		typedef TMultiKey<StringValue> TKey;
		typedef CommonLib::STLAllocator<TKey> TAlloc;
		typedef TDeVector<TKey, TAlloc> TValueMemSet;
		typedef CZlibStringComp::TValueMemSet TStringMemSet;

		TStringMultiKeyEncoder(TAllocsSetPtr pAllocsSet, TCompressorParamsBasePtr pParams, ECompressParams type) :
			m_pAllocsSet(pAllocsSet), m_strings(pAllocsSet, pParams, type), m_subs(pAllocsSet, pParams, type)
		{
		}

		void AddSymbol(uint32_t nSize, uint32_t nIndex, const TKey& key, const TValueMemSet& vecKeys)
		{
			m_strings.AddSymbol(nSize, nIndex, key.m_key, EmptyStrings());
			if (nSize > 1)
			{
				if (nIndex == 0)
					m_subs.AddSymbol(Sub(vecKeys[nIndex + 1], key));
				else
				{
					const TKey& prev = vecKeys[nIndex - 1];
					if (nIndex == nSize - 1)
						m_subs.AddSymbol(Sub(key, prev));
					else
					{
						const TKey& next = vecKeys[nIndex + 1];
						m_subs.RemoveSymbol(Sub(next, prev));
						m_subs.AddSymbol(Sub(key, prev));
						m_subs.AddSymbol(Sub(next, key));
					}
				}
			}
		}

		void RemoveSymbol(uint32_t nSize, int nIndex, const TKey& key, const TValueMemSet& vecKeys)
		{
			m_strings.RemoveSymbol(nSize, nIndex, key.m_key, EmptyStrings());
			if (vecKeys.size() > 1)
			{
				if (nIndex == 0)
					m_subs.RemoveSymbol(Sub(vecKeys[nIndex + 1], key));
				else if (nIndex == (int)vecKeys.size() - 1)
					m_subs.RemoveSymbol(Sub(key, vecKeys[nIndex - 1]));
				else
				{
					const TKey& prev = vecKeys[nIndex - 1];
					const TKey& next = vecKeys[nIndex + 1];
					m_subs.AddSymbol(Sub(next, prev));
					m_subs.RemoveSymbol(Sub(key, prev));
					m_subs.RemoveSymbol(Sub(next, key));
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
			return (uint32_t)(2 * sizeof(uint32_t) + sizeof(int64_t)) + m_strings.GetCompressSize() + m_subs.GetCompressSize();
		}

		void BeginEncoding(const TValueMemSet&) {}

		uint32_t Encode(const TValueMemSet& vecKeys, CommonLib::IMemoryWriteStream* pStream, uint32_t maxCompSize, CBPTreeContext* pContext)
		{
			uint32_t n = (uint32_t)vecKeys.size();
			uint32_t nSplit = (std::max)(1u, n / 2);
			try
			{
				if (n == 0)
					return 0;
				size_t nStart = pStream->Pos();

				TStringMemSet strings(CZlibStringComp::TAlloc(m_pAllocsSet->GetCommonAlloc()));
				for (uint32_t i = 0; i < n; ++i)
					strings.push_back(vecKeys[i].m_key);

				size_t nSizePos = pStream->Pos();
				if (!pStream->WriteSafe((uint32_t)0))
					return nSplit;
				size_t nBlockStart = pStream->Pos();
				uint32_t nRes = m_strings.Encode(strings, pStream, maxCompSize, pContext);
				if (nRes != 0)
					return nRes;
				size_t nEnd = pStream->Pos();
				pStream->Seek(nSizePos, CommonLib::soFromBegin);
				pStream->Write((uint32_t)(nEnd - nBlockStart));
				pStream->Seek(nEnd, CommonLib::soFromBegin);

				if (!pStream->WriteSafe(vecKeys[0].m_nSub))
					return nSplit;

				m_subs.Clear();
				m_subSymbols.resize(n);
				for (uint32_t i = 1; i < n; ++i)
				{
					m_subSymbols[i] = Sub(vecKeys[i], vecKeys[i - 1]);
					m_subs.AddSymbol(m_subSymbols[i]);
				}
				nSizePos = pStream->Pos();
				if (!pStream->WriteSafe((uint32_t)0))
					return nSplit;
				size_t nStreamStart = pStream->Pos();
				if (n > 1)
				{
					if (!m_subs.BeginEncoding(pStream))
						return nSplit;
					for (uint32_t i = 1; i < n; ++i)
						if (!m_subs.EncodeSymbol(m_subSymbols[i]))
							return nSplit;
					if (!m_subs.FinishEncoding(pStream))
						return nSplit;
				}
				nEnd = pStream->Pos();
				pStream->Seek(nSizePos, CommonLib::soFromBegin);
				pStream->Write((uint32_t)(nEnd - nStreamStart));
				pStream->Seek(nEnd, CommonLib::soFromBegin);

				return pStream->Pos() - nStart > maxCompSize ? nSplit : 0;
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExcT("[TStringMultiKeyEncoder] failed to encode, count: {0}", n, exc);
				throw;
			}
		}

		void Decode(uint32_t nCount, TValueMemSet& vecKeys, CommonLib::IMemoryReadStream* pStream, uint32_t nCompSize, CBPTreeContext* pContext)
		{
			try
			{
				if (nCount == 0)
					return;

				uint32_t nBlockSize = pStream->ReadIntu32();
				TStringMemSet strings(CZlibStringComp::TAlloc(m_pAllocsSet->GetCommonAlloc()));
				m_strings.Decode(nCount, strings, pStream, nBlockSize, pContext);

				int64_t nSub = pStream->ReadInt64();
				uint32_t nSize = pStream->ReadIntu32();
				m_subSymbols.assign(nCount, 0);
				if (nCount > 1)
				{
					CommonLib::CReadMemoryStream stream;
					stream.AttachBuffer((byte_t*)pStream->Buffer() + pStream->Pos(), nSize);
					m_subs.BeginDecoding(&stream);
					for (uint32_t i = 1; i < nCount; ++i)
						m_subs.DecodeSymbol(m_subSymbols[i]);
					m_subs.FinishDecoding(&stream);
				}
				pStream->Seek(nSize, CommonLib::soFromCurrent);

				m_subs.Clear();
				for (uint32_t i = 0; i < nCount; ++i)
				{
					if (i)
					{
						nSub = (int64_t)((uint64_t)nSub + (uint64_t)UnZigZag(m_subSymbols[i]));
						m_subs.AddSymbol(m_subSymbols[i]);
					}
					vecKeys.push_back(TKey(std::move(strings[i]), nSub));
				}
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExcT("[TStringMultiKeyEncoder] failed to decode, count: {0}", nCount, exc);
				throw;
			}
		}

		void Clear()
		{
			m_strings.Clear();
			m_subs.Clear();
		}

	private:
		static uint64_t Sub(const TKey& cur, const TKey& prev)
		{
			return ZigZag((int64_t)((uint64_t)cur.m_nSub - (uint64_t)prev.m_nSub));
		}

		const TStringMemSet& EmptyStrings() const
		{
			if (!m_pEmpty)
				m_pEmpty.reset(new TStringMemSet(CZlibStringComp::TAlloc(m_pAllocsSet->GetCommonAlloc())));
			return *m_pEmpty;
		}

	private:
		TAllocsSetPtr m_pAllocsSet;
		CZlibStringComp m_strings;
		CUnsignedNumLenEncoder m_subs;
		std::vector<uint64_t> m_subSymbols;
		mutable std::unique_ptr<TStringMemSet> m_pEmpty;
	};

	// value encoder that writes nothing (TBPMultiMap without values: the row id is in the key)
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
