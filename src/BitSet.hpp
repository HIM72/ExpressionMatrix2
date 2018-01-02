#ifndef CZI_EXPRESSION_MATRIX2_BIT_SET_HPP
#define CZI_EXPRESSION_MATRIX2_BIT_SET_HPP


// A bare bone bitset class with just the functionality needed for operations
// on LSH signatures. Similar to boost::dynamic_bitset.

#include "CZI_ASSERT.hpp"
#include "string.hpp"
#include "vector.hpp"

namespace ChanZuckerberg {
    namespace ExpressionMatrix2 {
        class BitSet;           // Owns the memory.
        class BitSetInMemory;   // Does not own the memory.
        class BitSetsInMemory;  // Many bit sets, stored contiguously in owned memory.
        uint64_t countMismatches(const BitSet&, const BitSet&);
        uint64_t countMismatches(size_t wordCount, const BitSetInMemory&, const BitSetInMemory&);
    }
}


// Bit set that does not own its memory.
class ChanZuckerberg::ExpressionMatrix2::BitSetInMemory {
public:

    BitSetInMemory(uint64_t* data, size_t wordCount) : data(data), wordCount(wordCount)
    {
    }

    // The assignment operator copies the data.
    BitSetInMemory& operator=(const BitSetInMemory& that)
    {
        CZI_ASSERT(wordCount == that.wordCount);
        copy(that.data, that.data+wordCount, data);
        return *this;
    }

    // Get the bit at a given position.
    bool get(uint64_t bitPosition) const
        {
        // Find the word containing the bit.
        const uint64_t wordIndex = bitPosition >> 6ULL;
        const uint64_t& word = data[wordIndex];

        // Find the position of this bit in the word.
        // The first bit is in the most significant position,
        // so sorting the bitset using integer compariso
        // does a lexicographic ordering.
        const uint64_t bitPositionInWord = 63ULL - (bitPosition & 63ULL);

        // Test the bit.
        const uint64_t mask = 1ULL << bitPositionInWord;
        return (word & mask) != 0ULL;

    }

    // Set a bit at a given position.
    void set(uint64_t bitPosition)
    {
        // Find the word containing the bit.
        const uint64_t wordIndex = bitPosition >> 6ULL;
        uint64_t& word = data[wordIndex];

        // Find the position of this bit in the word.
        // The first bit is in the most significant position,
        // so sorting the bitset using integer compariso
        // does a lexicographic ordering.
        const uint64_t bitPositionInWord = 63ULL - (bitPosition & 63ULL);

        // Set the bit.
        word |= 1ULL << bitPositionInWord;

    }
#if 0
    // Get a uint64_t containing the bits in a specified bit range.
    // This range must be entirely contained in one word (it cannot cross word boundaries).
    uint64_t getBits(uint64_t firstBitPosition, uint64_t bitCount) const
        {
        // Find the word containing the bit.
        const uint64_t firstBitWordIndex = firstBitPosition >> 6ULL;
        const uint64_t& word = data[firstBitWordIndex];

        // Find the position of the first bit in the word.
        const uint64_t firstBitPositionInWord = firstBitPosition & 63ULL;

        // Check that the entire range is contained in this word.
        CZI_ASSERT(firstBitPositionInWord + bitCount <= 64ULL);

        // Extract the bits.
        if(bitCount == 64) {
            return word;
        } else {
            const uint64_t bitMask = (1ULL << bitCount) - 1ULL;
            return (word >> firstBitPositionInWord) & bitMask;
        }
    }
#endif

    // Get a uint64_t containing bits at specified positions.
    // The bits are specified in a vector.
    // The last specified bit goes in the least significant position
    // of the return value.
    uint64_t getBits(const vector<size_t>& bitPositions) const
    {
        uint64_t bits = 0;
        for(const size_t bitPosition: bitPositions) {
            bits <<= 1;
            bits += get(bitPosition);
        }
        return bits;
    }

    string getString(uint64_t bitCount) const
    {
        string s;
        for(uint64_t i=0; i<bitCount; i++) {
            if(get(i)) {
                s += 'x';
            } else {
                s += '_';
            }
        }
        return s;

    }

    void permuteBits(
        const vector<int>& permutation,
        BitSetInMemory& that) const
    {
        that.clear();
        for(size_t i=0; i<permutation.size(); i++) {
            if(get(permutation[i])) {
                that.set(i);
            }
        }
    }

    void clear()
    {
        fill(data, data+wordCount, 0ULL);
    }

    uint64_t* data;
    size_t wordCount;

};



// Bit set that owns its memory.
class ChanZuckerberg::ExpressionMatrix2::BitSet {
public:

    BitSet(uint64_t bitCount)
    {
        CZI_ASSERT(bitCount > 0);
        data.resize(((bitCount - 1ULL) >> 6ULL) + 1ULL, 0ULL);
    }

    BitSet(BitSetInMemory bitSetInMemory, uint64_t bitCount)
    {
        CZI_ASSERT(bitCount > 0);
        const uint64_t wordCount = ((bitCount - 1ULL) >> 6ULL) + 1ULL;
        data.resize(wordCount);
        copy(bitSetInMemory.data, bitSetInMemory.data+wordCount, data.begin());
    }

    // Get the bit at a given position.
    bool get(uint64_t bitPosition) const
        {
        // Find the word containing the bit.
        const uint64_t wordIndex = bitPosition >> 6ULL;
        const uint64_t& word = data[wordIndex];

        // Find the position of this bit in the word.
        // The first bit is in the most significant position,
        // so sorting the bitset using integer compariso
        // does a lexicographic ordering.
        const uint64_t bitPositionInWord = 63ULL - (bitPosition & 63ULL);

        // Test the bit.
        const uint64_t mask = 1ULL << bitPositionInWord;
        return (word & mask) != 0ULL;

    }

    // Set a bit at a given position.
    void set(uint64_t bitPosition)
    {
        // Find the word containing the bit.
        const uint64_t wordIndex = bitPosition >> 6ULL;
        uint64_t& word = data[wordIndex];

        // Find the position of this bit in the word.
        // The first bit is in the most significant position,
        // so sorting the bitset using integer comparison
        // does a lexicographic ordering.
        const uint64_t bitPositionInWord = 63ULL - (bitPosition & 63ULL);

        // Set the bit.
        word |= 1ULL << bitPositionInWord;

    }

#if 0
    // Get a uint64_t containing the bits in a specified bit range.
    // This range must be entirely contained in one word (it cannot cross word boundaries).
    uint64_t getBits(uint64_t firstBitPosition, uint64_t bitCount) const
        {
        // Find the word containing the bit.
        const uint64_t firstBitWordIndex = firstBitPosition >> 6ULL;
        const uint64_t& word = data[firstBitWordIndex];

        // Find the position of the first bit in the word.
        const uint64_t firstBitPositionInWord = firstBitPosition & 63ULL;

        // Check that the entire range is contained in this word.
        CZI_ASSERT(firstBitPositionInWord + bitCount <= 64ULL);

        // Extract the bits.
        if(bitCount == 64) {
            return word;
        } else {
            const uint64_t bitMask = (1ULL << bitCount) - 1ULL;
            return (word >> firstBitPositionInWord) & bitMask;
        }
    }
#endif

    // Get a uint64_t containing bits at specified positions.
    // The bits are specified in a vector.
    // The last specified bit goes in the least significant position
    // of the return value.
    uint64_t getBits(const vector<size_t>& bitPositions) const
    {
        uint64_t bits = 0;
        for(const size_t bitPosition: bitPositions) {
            bits <<= 1;
            bits += get(bitPosition);
        }
        return bits;
    }

    bool operator<(const BitSet& that) const
    {
        return data < that.data;
    }

    string getString(uint64_t bitCount) const
    {
        string s;
        for(uint64_t i=0; i<bitCount; i++) {
            if(get(i)) {
                s += 'x';
            } else {
                s += '_';
            }
        }
        return s;

    }

    vector<uint64_t> data;

    // Create a BitSetInMemory for this data.
    BitSetInMemory get()
    {
        return BitSetInMemory(data.data(), data.size());
    }

};



// Class to represent many bit sets, stored contiguously in owned memory.
class ChanZuckerberg::ExpressionMatrix2::BitSetsInMemory {
public:
    BitSetsInMemory(
        size_t bitSetCount,             // The number of bit sets.
        size_t wordCount,               // The number of 64-bit words in each bit set.
        const uint64_t* inputData       // Data are copied from here.
        ) :
        bitSetCount(bitSetCount),
        wordCount(wordCount)
        {
            const size_t n = bitSetCount*wordCount;
            data.resize(n);
            std::copy(inputData, inputData + n, data.begin());
        }

    size_t bitSetCount;   // The number of bit sets.
    size_t wordCount;     // The number of 64-bit words in each bit set.
    vector<uint64_t> data;  // Of size n*m.

    // Get the i-th BitVector in memory.
    BitSetInMemory get(size_t i)
    {
        CZI_ASSERT(i < bitSetCount);
        return BitSetInMemory(data.data() + i*wordCount, wordCount);
    }

};



// Count the number of mismatching bits between two bit vectors.
// The two bit vectors should have the same length, but we don't check this for performance.
inline uint64_t ChanZuckerberg::ExpressionMatrix2::countMismatches(const BitSet& x, const BitSet& y)
{
    uint64_t mismatchCount = 0;
    const uint64_t blockCount = x.data.size();
    for(uint64_t i = 0; i < blockCount; i++) {
        mismatchCount += __builtin_popcountll(x.data[i] ^ y.data[i]);
    }
    return mismatchCount;
}
inline uint64_t ChanZuckerberg::ExpressionMatrix2::countMismatches(
    size_t wordCount,
    const BitSetInMemory& x,
    const BitSetInMemory& y)
{
    uint64_t mismatchCount = 0;
    for(uint64_t i = 0; i < wordCount; i++) {
        mismatchCount += __builtin_popcountll(x.data[i] ^ y.data[i]);
    }
    return mismatchCount;
}


#endif
