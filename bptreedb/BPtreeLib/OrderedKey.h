#pragma once
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace bptreedb
{
	// Order-preserving unsigned bits of a number: a < b  <=>  ToBits(a) < ToBits(b).
	//   unsigned: the value; signed: the sign bit flipped;
	//   float / double: positive - the sign bit set, negative - all bits inverted (IEEE 754 total order:
	//   -NaN < -inf < ... < -0.0 < +0.0 < ... < +inf < +NaN).
	// Used to compress sorted float keys losslessly (differences of the bits, not of the values) and to
	// compare float keys with a total order.
	template <class T>
	struct TOrderedBits
	{
		static_assert(std::is_arithmetic<T>::value, "TOrderedBits: a number type");
		typedef typename std::conditional<sizeof(T) <= 4, uint32_t, uint64_t>::type TBits;
		static constexpr TBits SignBit = TBits(1) << (sizeof(TBits) * 8 - 1);

		static uint64_t ToBits(T v)
		{
			if constexpr (std::is_floating_point<T>::value)
			{
				TBits u;
				memcpy(&u, &v, sizeof(T));
				return (uint64_t)((u & SignBit) ? ~u : (u | SignBit));
			}
			else if constexpr (std::is_signed<T>::value)
				return (uint64_t)(TBits)((TBits)(typename std::make_unsigned<T>::type)v ^ ((TBits)1 << (sizeof(T) * 8 - 1)));
			else
				return (uint64_t)v;
		}

		static T FromBits(uint64_t b)
		{
			if constexpr (std::is_floating_point<T>::value)
			{
				TBits u = (TBits)b;
				u = (u & SignBit) ? (u & ~SignBit) : ~u;
				T v;
				memcpy(&v, &u, sizeof(T));
				return v;
			}
			else if constexpr (std::is_signed<T>::value)
				return (T)(typename std::make_unsigned<T>::type)((TBits)b ^ ((TBits)1 << (sizeof(T) * 8 - 1)));
			else
				return (T)b;
		}
	};

	// Comparator of number keys by TOrderedBits: for float / double a total order (NaN is a valid key,
	// -0.0 and +0.0 are different keys), for integers the usual order.
	template <class T>
	struct TOrderedComp
	{
		bool LE(const T& l, const T& r) const { return TOrderedBits<T>::ToBits(l) < TOrderedBits<T>::ToBits(r); }
		bool EQ(const T& l, const T& r) const { return TOrderedBits<T>::ToBits(l) == TOrderedBits<T>::ToBits(r); }
		bool operator()(const T& l, const T& r) const { return LE(l, r); }
	};

	// Key of a tree with duplicate keys (TBPMultiMap): the key and a unique sub key (the row id).
	// The tree stays unique; all entries of one key are next to each other, ordered by the sub key.
	template <class _TKey>
	struct TMultiKey
	{
		typedef _TKey TKey;

		TMultiKey() {}
		TMultiKey(const TKey& key, int64_t nSub) : m_key(key), m_nSub(nSub) {}

		TKey m_key;
		int64_t m_nSub{ 0 };
	};

	template <class _TKeyComp>
	struct TMultiKeyComp
	{
		_TKeyComp m_comp;

		template <class TMKey>
		bool LE(const TMKey& l, const TMKey& r) const
		{
			if (m_comp.LE(l.m_key, r.m_key)) return true;
			if (m_comp.LE(r.m_key, l.m_key)) return false;
			return l.m_nSub < r.m_nSub;
		}

		template <class TMKey>
		bool EQ(const TMKey& l, const TMKey& r) const
		{
			return l.m_nSub == r.m_nSub && m_comp.EQ(l.m_key, r.m_key);
		}

		template <class TMKey>
		bool operator()(const TMKey& l, const TMKey& r) const { return LE(l, r); }
	};
}
