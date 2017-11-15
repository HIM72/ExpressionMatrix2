// Class to describe a vector of vectors stored contiguously in mapped memory.
// A table of contents (toc) contains indexes pointing to the first element of each vector.

#ifndef CZI_EXPRESSION_MATRIX2_MEMORY_MAPPED_VECTOR_OF_VECTORS_HPP
#define CZI_EXPRESSION_MATRIX2_MEMORY_MAPPED_VECTOR_OF_VECTORS_HPP

// CZI.
#include "MemoryMappedVector.hpp"
#include "MemoryAsContainer.hpp"

// Standard libraries, partially injected into the ChanZuckerberg::Rna1 namespace.
#include "algorithm.hpp"
#include "vector.hpp"

// Forward declarations.
namespace ChanZuckerberg {
    namespace ExpressionMatrix2 {
        namespace MemoryMapped {
            template<class Int, class T> class VectorOfVectors;
        }
    }
}



template<class T, class Int> class ChanZuckerberg::ExpressionMatrix2::MemoryMapped::VectorOfVectors {
public:

    void createNew(const string& name)
    {
        toc.createNew(name + ".toc");
        toc.push_back(0);
        data.createNew(name + ".data");
    }



    void accessExisting(const string& name, bool readWriteAccess)
    {
        toc.accessExisting(name + ".toc", readWriteAccess);
        data.accessExisting(name + ".data", readWriteAccess);
    }
    void accessExistingReadOnly(const string& name)
    {
        accessExisting(name, false);
    }

    void accessExistingReadWrite(const string& name, bool allowReadOnly)
    {
        if(allowReadOnly) {
            try {
                accessExisting(name, true);
            } catch(runtime_error e) {
                accessExisting(name, false);
            }
        } else {
            accessExisting(name, true);
        }
    }

    void remove()
    {
        toc.remove();
        data.remove();
    }

    size_t size() const
    {
        return toc.size() - 1;
    }
    size_t totalSize() const
    {
        return data.size();
    }
    void close()
    {
        toc.close();
        data.close();
    }
    bool empty() const
    {
        return toc.size() == 1;
    }

    T* begin()
    {
        return data.begin();
    }
    const T* begin() const
    {
        return data.begin();
    }

    T* end()
    {
        return data.end();
    }
    const T* end() const
    {
        return data.end();
    }


    // Return size/begin/end of the i-th vector.
    size_t size(size_t i) const
    {
        return toc[i+1] - toc[i];
    }
    T* begin(Int i)
    {
        return data.begin() + toc[i];
    }
    const T* begin(Int i) const
    {
        return data.begin() + toc[i];
    }
    T* end(Int i)
    {
        return data.begin() + toc[i+1];
    }
    const T* end(Int i) const
    {
        return data.begin() + toc[i+1];
    }

   // Add an empty vector at the end.
    void appendVector()
    {
        const Int tocBack = toc.back();
        toc.push_back(tocBack);
    }

    // Add a T at the end of the last vector.
    void append(const T& t)
    {
        CZI_ASSERT(!empty());
        ++toc.back();
        data.push_back(t);
    }



    // Add a non-empty vector at the end.
    template<class Iterator> void appendVector(Iterator begin, Iterator end)
    {
        // First, append an empty vector.
        appendVector();

        // Then, append all the elements to the last vector.
        for(Iterator it=begin; it!=end; ++it) {
            append(*it);
        }
    }



    // Operator[] return a MemoryAsContainer object.
    MemoryAsContainer<T> operator[](Int i)
    {
        return MemoryAsContainer<T>(begin(i), end(i));
    }
    MemoryAsContainer<const T> operator[](Int i) const
    {
        return MemoryAsContainer<const T>(begin(i), end(i));
    }


    // Function to construct the VectorOfVectors in two passes.
    // In pass 1 we count the number of entries in each of the vectors.
    // In pass 2 we store the entries.
    // This can be easily turned into multithreaded code
    // if atomic memory access primitives are used.
    vector<Int> count;
    void beginPass1(Int n);
    void incrementCount(Int index, Int m=1);  // Called during pass 1.
    void beginPass2();
    void store(Int index, const T&);            // Called during pass 2.
    void endPass2();

    // Touch the memory in order to cause the
    // supporting pages of virtual memory to be loaded in real memory.
    size_t touchMemory() const
    {
        return toc.touchMemory() + data.touchMemory();
    }


private:
    Vector<Int> toc;
    Vector<T> data;
};



template<class T, class Int>
    void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::VectorOfVectors<T, Int>::beginPass1(Int n)
{
    count.resize(n);
    fill(count.begin(), count.end(), Int(0));
}



template<class T, class Int>
    void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::VectorOfVectors<T, Int>::beginPass2()
{
    const Int n = count.size();
    toc.reserve(n+1);
    toc.resize(n+1);
    toc[0] = 0;
    for(Int i=0; i<n; i++) {
        toc[i+1] = toc[i] + count[i];
    }
    const size_t  dataSize = toc.back() - 1ULL;
    data.reserve(dataSize);
    data.resize(dataSize);
}



template<class T, class Int>
    void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::VectorOfVectors<T, Int>::endPass2()
{
    // Verify that all counts are now zero.
    const Int n = count.size();
    for(Int i=0; i<n; i++) {
        CZI_ASSERT(count[i] == 0);;
    }

    // Free the memory of the count vector.
    vector<Int> emptyVector;
    count.swap(emptyVector);
}



template<class T, class Int>
    void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::VectorOfVectors<T, Int>::incrementCount(Int index, Int m)
{
    count[index] += m;
}


template<class T, class Int>
    void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::VectorOfVectors<T, Int>::store(Int index, const T& t)
{
    (*this)[index][--count[index]] = t;
}



#endif
