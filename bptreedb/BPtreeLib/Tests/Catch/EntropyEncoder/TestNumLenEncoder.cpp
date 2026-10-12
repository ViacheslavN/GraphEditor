#include "TestCommon.h"

using namespace bptreedb;
using namespace test_utils;

namespace
{
	template<class TEncoder>
	TEncoder MakeEncoder()
	{
		return TEncoder(TAllocsSetPtr(), MakeParams(), eLeafValue);
	}

	template<class TEncoder, class TSymbol>
	void AddAll(TEncoder& enc, const std::vector<TSymbol>& symbols)
	{
		for (const TSymbol& s : symbols)
			enc.AddSymbol(s);
	}

	struct SEncoded
	{
		std::vector<byte_t> buf;
		uint32_t nEstimate{ 0 };
		size_t nWritten{ 0 };
	};

	// Encodes into a buffer larger than the estimate, so a low estimate shows up as a
	// failed CHECK instead of a failed encode.
	template<class TEncoder, class TSymbol>
	SEncoded Encode(TEncoder& enc, const std::vector<TSymbol>& symbols)
	{
		enc.Clear();
		AddAll(enc, symbols);

		SEncoded res;
		res.nEstimate = enc.GetCompressSize();

		SWriteBuf wb(res.nEstimate * 2 + 1024);
		REQUIRE(enc.BeginEncoding(&wb.stream));
		for (const TSymbol& s : symbols)
			REQUIRE(enc.EncodeSymbol(s));
		REQUIRE(enc.FinishEncoding(&wb.stream));

		res.nWritten = wb.stream.Pos();
		res.buf = wb.buf;

		// The tree splits nodes based on this estimate, it must not be lower than the real size
		CHECK(res.nWritten <= res.nEstimate);
		return res;
	}

	template<class TEncoder, class TSymbol>
	void Decode(TEncoder& dec, SEncoded& enc, const std::vector<TSymbol>& symbols)
	{
		SReadBuf rb(enc.buf);
		dec.BeginDecoding(&rb.stream);
		for (size_t i = 0; i < symbols.size(); ++i)
		{
			TSymbol value = 0;
			dec.DecodeSymbol(value);
			INFO("index " << i);
			REQUIRE(value == symbols[i]);
		}
		dec.FinishDecoding(&rb.stream);
	}

	template<class TEncoder, class TSymbol>
	SEncoded RoundTrip(const std::vector<TSymbol>& symbols)
	{
		TEncoder enc = MakeEncoder<TEncoder>();
		SEncoded res = Encode(enc, symbols);
		TEncoder dec = MakeEncoder<TEncoder>();
		Decode(dec, res, symbols);
		return res;
	}

	std::vector<uint64_t> UniformBits(size_t count, uint32_t maxBits, uint32_t seed)
	{
		std::mt19937_64 rng(seed);
		std::vector<uint64_t> out(count);
		for (uint64_t& v : out)
		{
			uint32_t bits = (uint32_t)(rng() % (maxBits + 1));
			uint64_t mask = bits == 64 ? ~uint64_t(0) : ((uint64_t(1) << bits) - 1);
			v = rng() & mask;
		}
		return out;
	}

	std::vector<uint64_t> Geometric(size_t count, double p, uint32_t seed)
	{
		std::mt19937 rng(seed);
		std::geometric_distribution<uint64_t> dist(p);
		std::vector<uint64_t> out(count);
		for (uint64_t& v : out)
			v = dist(rng);
		return out;
	}

	std::vector<int64_t> SignedMix(size_t count, double pNegative, uint32_t maxBits, uint32_t seed)
	{
		std::mt19937_64 rng(seed);
		std::bernoulli_distribution neg(pNegative);
		std::vector<uint64_t> mags = UniformBits(count, maxBits, seed + 1);
		std::vector<int64_t> out(count);
		for (size_t i = 0; i < count; ++i)
		{
			int64_t m = (int64_t)(mags[i] >> 1); // keep |v| <= 2^62
			out[i] = neg(rng) ? -m : m;
		}
		return out;
	}
}

// ---------------------------------------------------------------------------
// CUnsignedNumLenEncoder
// ---------------------------------------------------------------------------

TEST_CASE("CUnsignedNumLenEncoder round trip", "[entropy][numlen][unsigned]")
{
	typedef CUnsignedNumLenEncoder TEnc;

	SECTION("single symbol")         { RoundTrip<TEnc>(std::vector<uint64_t>{ 12345 }); }
	SECTION("only zeros")            { RoundTrip<TEnc>(std::vector<uint64_t>(1000, 0)); }
	SECTION("only ones")             { RoundTrip<TEnc>(std::vector<uint64_t>(1000, 1)); }
	SECTION("constant value")        { RoundTrip<TEnc>(std::vector<uint64_t>(1000, 777)); }
	SECTION("small values 0..3")     { RoundTrip<TEnc>(UniformBits(5000, 2, 1)); }
	SECTION("geometric")             { RoundTrip<TEnc>(Geometric(5000, 0.05, 2)); }
	SECTION("uniform up to 32 bits") { RoundTrip<TEnc>(UniformBits(5000, 32, 3)); }
	SECTION("uniform up to 64 bits") { RoundTrip<TEnc>(UniformBits(5000, 64, 4)); }
}

TEST_CASE("CUnsignedNumLenEncoder handles the extreme 64-bit values", "[entropy][numlen][unsigned]")
{
	std::vector<uint64_t> values;
	for (uint32_t bit = 0; bit < 64; ++bit)
	{
		values.push_back(uint64_t(1) << bit);
		values.push_back((uint64_t(1) << bit) - 1);
	}
	values.push_back(std::numeric_limits<uint64_t>::max());
	values.push_back(std::numeric_limits<uint64_t>::max() - 1);

	RoundTrip<CUnsignedNumLenEncoder>(values);
}

TEST_CASE("CUnsignedNumLenEncoder frequency tables of every width", "[entropy][numlen][unsigned]")
{
	// frequencies are stored as byte / uint16 / uint32 depending on the largest one
	SECTION("byte freqs")   { RoundTrip<CUnsignedNumLenEncoder>(UniformBits(200, 20, 5)); }
	SECTION("uint16 freqs") { RoundTrip<CUnsignedNumLenEncoder>(std::vector<uint64_t>(3000, 5)); }
	SECTION("uint32 freqs")
	{
		std::vector<uint64_t> values(70000, 5);
		values[100] = 1000000;
		RoundTrip<CUnsignedNumLenEncoder>(values);
	}
}

TEST_CASE("CUnsignedNumLenEncoder RemoveSymbol undoes AddSymbol", "[entropy][numlen][unsigned]")
{
	std::vector<uint64_t> base = Geometric(3000, 0.01, 6);
	std::vector<uint64_t> extra = UniformBits(3000, 64, 7);

	CUnsignedNumLenEncoder a = MakeEncoder<CUnsignedNumLenEncoder>();
	CUnsignedNumLenEncoder b = MakeEncoder<CUnsignedNumLenEncoder>();
	AddAll(a, base);
	AddAll(a, extra);
	for (uint64_t v : extra)
		a.RemoveSymbol(v);
	AddAll(b, base);

	REQUIRE(a.GetCompressSize() == b.GetCompressSize());
}

// Regression: Clear() did not reset the count of distinct lengths, so after every
// Recalc (split / merge) the size estimate stayed inflated and nodes split too early.
TEST_CASE("CUnsignedNumLenEncoder Clear resets the size estimate", "[entropy][numlen][unsigned][regression]")
{
	CUnsignedNumLenEncoder reused = MakeEncoder<CUnsignedNumLenEncoder>();
	AddAll(reused, UniformBits(5000, 64, 8)); // many distinct lengths
	reused.Clear();

	std::vector<uint64_t> fewValues(100, 5);
	AddAll(reused, fewValues);

	CUnsignedNumLenEncoder fresh = MakeEncoder<CUnsignedNumLenEncoder>();
	AddAll(fresh, fewValues);

	REQUIRE(reused.GetCompressSize() == fresh.GetCompressSize());
}

TEST_CASE("CUnsignedNumLenEncoder can be reused for several encodings", "[entropy][numlen][unsigned]")
{
	CUnsignedNumLenEncoder enc = MakeEncoder<CUnsignedNumLenEncoder>();
	CUnsignedNumLenEncoder dec = MakeEncoder<CUnsignedNumLenEncoder>();
	for (uint32_t rep = 0; rep < 4; ++rep)
	{
		INFO("repetition " << rep);
		std::vector<uint64_t> values = UniformBits(2000 + rep * 500, 16 + rep * 16, 20 + rep);
		SEncoded res = Encode(enc, values);
		Decode(dec, res, values);
	}
}

// ---------------------------------------------------------------------------
// CSignedNumLenEncoder
// ---------------------------------------------------------------------------

TEST_CASE("CSignedNumLenEncoder round trip", "[entropy][numlen][signed]")
{
	typedef CSignedNumLenEncoder TEnc;

	SECTION("all positive")            { RoundTrip<TEnc>(SignedMix(3000, 0.0, 40, 1)); }
	SECTION("all negative")            { RoundTrip<TEnc>(SignedMix(3000, 1.0, 40, 2)); }
	SECTION("balanced signs")          { RoundTrip<TEnc>(SignedMix(3000, 0.5, 40, 3)); }
	SECTION("rare negatives")          { RoundTrip<TEnc>(SignedMix(20000, 0.02, 30, 4)); }
	SECTION("rare positives")          { RoundTrip<TEnc>(SignedMix(20000, 0.98, 30, 5)); }
	SECTION("large magnitudes")        { RoundTrip<TEnc>(SignedMix(3000, 0.5, 64, 6)); }
	SECTION("zeros and small values")  { RoundTrip<TEnc>(std::vector<int64_t>{ 0, -1, 1, 0, -2, 2, 0, 0, -1 }); }
}

// Mirrors the tree's value diffs: mostly -1 with a rare large jump up, in a leaf with
// more than 32768 entries (PosBits with 16-bit positions above int16 range).
TEST_CASE("CSignedNumLenEncoder mostly decreasing values in a large leaf", "[entropy][numlen][signed][regression]")
{
	std::vector<int64_t> diffs(60000, -1);
	for (size_t i = 0; i < diffs.size(); i += 32)
		diffs[i] = 4000000 + (int64_t)i;

	RoundTrip<CSignedNumLenEncoder>(diffs);
}

TEST_CASE("CSignedNumLenEncoder RemoveSymbol undoes AddSymbol", "[entropy][numlen][signed]")
{
	std::vector<int64_t> base = SignedMix(3000, 0.1, 30, 7);
	std::vector<int64_t> extra = SignedMix(3000, 0.6, 50, 8);

	CSignedNumLenEncoder a = MakeEncoder<CSignedNumLenEncoder>();
	CSignedNumLenEncoder b = MakeEncoder<CSignedNumLenEncoder>();
	AddAll(a, base);
	AddAll(a, extra);
	for (int64_t v : extra)
		a.RemoveSymbol(v);
	AddAll(b, base);

	REQUIRE(a.GetCompressSize() == b.GetCompressSize());
}

TEST_CASE("CSignedNumLenEncoder Clear resets the size estimate", "[entropy][numlen][signed][regression]")
{
	CSignedNumLenEncoder reused = MakeEncoder<CSignedNumLenEncoder>();
	AddAll(reused, SignedMix(5000, 0.5, 60, 9));
	reused.Clear();

	std::vector<int64_t> fewValues(100, -3);
	AddAll(reused, fewValues);

	CSignedNumLenEncoder fresh = MakeEncoder<CSignedNumLenEncoder>();
	AddAll(fresh, fewValues);

	REQUIRE(reused.GetCompressSize() == fresh.GetCompressSize());
}

// Regression: reusing the encoder (as a tree node does on every save) overflowed the sign
// position stream; reusing the decoder must not keep positions from the previous page.
TEST_CASE("CSignedNumLenEncoder can be reused for several encodings", "[entropy][numlen][signed][regression]")
{
	CSignedNumLenEncoder enc = MakeEncoder<CSignedNumLenEncoder>();
	CSignedNumLenEncoder dec = MakeEncoder<CSignedNumLenEncoder>();
	for (uint32_t rep = 0; rep < 5; ++rep)
	{
		INFO("repetition " << rep);
		std::vector<int64_t> values = SignedMix(40000, rep % 2 ? 0.97 : 0.03, 24, 30 + rep);
		SEncoded res = Encode(enc, values);
		Decode(dec, res, values);
	}
}
