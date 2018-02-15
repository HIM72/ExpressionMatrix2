// Class to describe a vector stored in a file mapped to memory.

#ifndef CZI_EXPRESSION_MATRIX2_MEMORY_MAPPED_VECTOR_HPP
#define CZI_EXPRESSION_MATRIX2_MEMORY_MAPPED_VECTOR_HPP

// CZI.
#include "CZI_ASSERT.hpp"
#include "filesystem.hpp"
#include "touchMemory.hpp"

// Boost libraries, partially injected into the ExpressionMatrix2 namespace,
#include "boost_lexical_cast.hpp"

// Standard libraries, partially injected into the ExpressionMatrix2 namespace.
#include <cstring>
#include "algorithm"
#include "cstddef.hpp"
#include "iostream.hpp"
#include "stdexcept.hpp"
#include "string.hpp"
#include "vector.hpp"

// Linux.
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "array.hpp"

// Forward declarations.
namespace ChanZuckerberg {
    namespace ExpressionMatrix2 {
        namespace MemoryMapped {
            template<class T> class Vector;
        }
        inline void testMemoryMappedVector();
    }
}



template<class T> class ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector {
public:

    // The access functions work as in std::vector.
    size_t size() const;
    bool empty() const;
    size_t capacity() const;
    T& operator[](size_t);
    const T& operator[](size_t) const;
    T& front();
    const T& front() const;
    T& back();
    const T& back() const;
    T* begin();
    const T* begin() const;
    T* end();
    const T* end() const;
    void push_back(const T&);

    // Constructor and destructor.
    Vector();
    ~Vector();

    // Disallow C++ copy and assignment
    // (see below for how to make a copy).
    Vector(const Vector&) = delete;
    Vector& operator=(const Vector&) = delete;

    // Create a new mapped vector with n objects.
    // The last argument specifies the required capacity.
    // Actual capacity will be a bit larger due to rounding up to the next page boundary.
    // The vector is stored in a memory mapped file with the specified name.
    void createNew(const string& name, size_t n=0, size_t requiredCapacity=0);

    // Open a previously created vector with read-only or read-write access.
    // If accessExistingReadWrite is called with allowReadOnly=true,
    // it attempts to open with read-write access, but if that fails falls back to
    // read-only access.
    void accessExisting(const string& name, bool readWriteAccess);
    void accessExistingReadOnly(const string& name);
    void accessExistingReadWrite(const string& name, bool allowReadOnly);

    // Sync the mapped memory to disk.
    // This guarantees that the data on disk reflect all the latest changes in memory.
    // This is automatically called by close, and therefore also by the destructor.
    void syncToDisk();

    // Sync the mapped memory to disk, then unmap it.
    // This is automatically called by the destructor.
    void close();

    // Close and remove the supporting file.
    void remove();

    // Resize works as for std::vector;
    void resize(size_t);

    // Touch a range of memory in order to cause the
    // supporting pages of virtual memory to be loaded in real memory.
    // The return value can be ignored.
    size_t touchMemory() const
    {
        return ExpressionMatrix2::touchMemory(begin(), end());
    }


    void reserve();
    void reserve(size_t capacity);

    // Make a copy of the Vector.
    void makeCopy(Vector<T>& copy, const string& newName) const;

    // Comparison operator.
    bool operator==(const Vector<T>& that) const
    {
        return
            size() == that.size() &&
            std::equal(begin(), end(), that.begin());
    }

private:

    // A mapped file is always allocated with size equal to a multiple of page size.
    // Here we assume a fixed 4KB page size.
    // This will have to be changed if we want to support large pages.
    static const size_t pageSize = 4096;

    // Compute the number of pages needed to hold n bytes.
    static size_t computePageCount(size_t n)
    {
        return (n - 1ULL ) / pageSize  + 1ULL;
    }

    // The header begins at the beginning of the mapped file.
    class Header {
        public:

        // The size of the header in bytes, including padding.
        size_t headerSize;

        // The size of each object stored in the vector, in bytes.
        size_t objectSize;

        // The number of objects currently stored in the vector.
        size_t objectCount;

        // The number of pages in the mapped file.
        // this equals exactly fileSize/pageSize.
        size_t pageCount;

        // The total number of allocated bytes in the mapped file.
        // This equals headerSize + dataSize, rounded up to the next
        // multiple of a page size.
        size_t fileSize;

        // The current capacity of the vector (number of objects that can be stored
        // in the currently allocated memory).
        size_t capacity;

        // Magic number used for sanity check.
        static const size_t constantMagicNumber =  0xa3756fd4b5d8bcc1ULL;
        size_t magicNumber;

        // Pad to 256 bytes to make sure the data are aligned with cache lines.
        array<size_t, 25> padding;



        // Constructor with a given size and capacity.
        // Actual capacity will a bit larger, rounded up to the next oage boundary.
        Header(size_t n, size_t requestedCapacity)
        {
            CZI_ASSERT(requestedCapacity >= n);
            clear();
            headerSize = sizeof(Header);
            objectSize = sizeof(T);
            objectCount = n;
            pageCount = computePageCount(headerSize + objectSize * requestedCapacity);
            fileSize = pageCount * pageSize;
            capacity = (fileSize - headerSize) / objectSize;
            magicNumber = constantMagicNumber;
        }



        // Set the header to all zero bytes.
        void clear()
        {
            std::memset(this, 0, sizeof(Header));
        }
    };
    BOOST_STATIC_ASSERT(sizeof(Header) == 256);
    Header* header;

    // The data immediately follow the header.
    T* data;

public:

    // Flags that indicate if the mapped file is open, and if so,
    // whether it is open for read-only or read-write.
    bool isOpen;
    bool isOpenWithWriteAccess;

    // The file name. If not open, this is an empty string.
    string fileName;

private:
    // Unmap the memory.
    void unmap();

    // Some private utility functions;

    // Open the given file name as new (create if not existing, truncate if existing)
    // and with write access.
    // Return the file descriptor.
    static int openNew(const string& name);

    // Open the given existing file.
    // Return the file descriptor.
    static int openExisting(const string& name, bool readWriteAccess);

    // Truncate the given file descriptor to the specified size.
    static void truncate(int fileDescriptor, size_t fileSize);

    // Map to memory the given file descriptor for the specified size.
    static void* map(int fileDescriptor, size_t fileSize, bool writeAccess);

    // Find the size of the file corresponding to an open file descriptor.
    size_t getFileSize(int fileDescriptor);
};



// Access functions for class Vector.
// As a result, invalid uses will result in segmentation faults.
// Note that in the non-const access functions we assert for isOpen, not isOpenWithWriteAccess.
// This is necessary to allow legimitate patterns, such as having a non-const reference
// to a Vector that is open read-only.
template<class T> inline size_t ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::size() const
{
    return isOpen ? header->objectCount : 0ULL;
}
template<class T> inline bool ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::empty() const
{
    return isOpen ? (size()==0) : 0ULL;
}
template<class T> inline size_t ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::capacity() const
{
    return isOpen ? header->capacity : 0ULL;
}

template<class T> inline T& ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::operator[](size_t i)
{
    CZI_ASSERT(isOpen);
    return data[i];
}
template<class T> inline const T& ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::operator[](size_t i) const
{
    CZI_ASSERT(isOpen);
    return data[i];
}

template<class T> inline T& ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::front()
{
    CZI_ASSERT(isOpen);
    return *data;
}
template<class T> inline const T& ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::front() const
{
    CZI_ASSERT(isOpen);
    CZI_ASSERT(size() > 0);
    return *data;
}

template<class T> inline T& ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::back()
{
    CZI_ASSERT(isOpen);
    return data[size() - 1ULL];
}
template<class T> inline const T& ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::back() const
{
    CZI_ASSERT(isOpen);
    return data[size() - 1ULL];
}
template<class T> inline T* ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::begin()
{
    CZI_ASSERT(isOpen);
    return data;
}
template<class T> inline const T* ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::begin() const
{
    CZI_ASSERT(isOpen);
    return data;
}

template<class T> inline T* ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::end()
{
    CZI_ASSERT(isOpen);
    return data + size();
}

template<class T> inline const T* ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::end() const
{
    CZI_ASSERT(isOpen);
    return data + size();
}
template<class T> inline void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::push_back(const T& t)
{
    CZI_ASSERT(isOpen);
    resize(size()+1ULL);
    back() = t;

}



// Default constructor.
template<class T> inline ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::~Vector()
{
    if(isOpen) {
        close();
    }
}

// Destructor.
template<class T> inline ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::Vector() :
    header(0),
    data(0),
    isOpen(false),
    isOpenWithWriteAccess(false)
{
}



// Open the given file name as new (create if not existing, truncate if existing)
// and with write access.
// Return the file descriptor.
template<class T> inline int ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::openNew(const string& name)
{

    // The specified name is not a directory.
    // Open or create a file with this name.
    const int fileDescriptor = ::open(
            name.c_str(),
            O_CREAT | O_TRUNC | O_RDWR,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(fileDescriptor == -1) {
        throw runtime_error("Error opening " + name);
    }

    return fileDescriptor;
}

// Open the given existing file.
// Return the file descriptor.
template<class T> inline int ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::openExisting(const string& name, bool readWriteAccess)
{
    const int fileDescriptor = ::open(
        name.c_str(),
        readWriteAccess ? O_RDWR : O_RDONLY);
    if(fileDescriptor == -1) {
        throw runtime_error("Error " + lexical_cast<string>(errno)
            + " opening MemoryMapped::Vector " + name + ": " + string(strerror(errno)));
    }
    return fileDescriptor;
}

// Truncate the given file descriptor to the specified size.
template<class T> inline void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::truncate(int fileDescriptor, size_t fileSize)
{
    const int ftruncateReturnCode = ::ftruncate(fileDescriptor, fileSize);
    if(ftruncateReturnCode == -1) {
        ::close(fileDescriptor);
        throw runtime_error("Error during ftruncate.");
    }
}

// Map to memory the given file descriptor for the specified size.
template<class T> inline void* ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::map(int fileDescriptor, size_t fileSize, bool writeAccess)
{
    void* pointer = ::mmap(0, fileSize, PROT_READ | (writeAccess ? PROT_WRITE : 0), MAP_SHARED, fileDescriptor, 0);
    if(pointer == reinterpret_cast<void*>(-1LL)) {
        ::close(fileDescriptor);
        throw runtime_error("Error during mmap.");
    }
    return pointer;
}

// Find the size of the file corresponding to an open file descriptor.
template<class T> inline size_t ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::getFileSize(int fileDescriptor)
{
    struct stat fileInformation;
    const int fstatReturnCode = ::fstat(fileDescriptor, &fileInformation);
    if(fstatReturnCode == -1) {
        ::close(fileDescriptor);
        throw runtime_error("Error during fstat.");
    }
    return fileInformation.st_size;
}



// Create a new mapped vector with n objects.
// The last argument specifies the required capacity.
// Actual capacity will be a bit larger due to rounding up to the next page boundary.
// The vector is stored in a memory mapped file with the specified name.
template<class T> inline void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::createNew(
    const string& name,
    size_t n,
    size_t requiredCapacity)
{
    try {
        // If already open, should have called close first.
        CZI_ASSERT(!isOpen);

        // Create the header.
        requiredCapacity = std::max(requiredCapacity, n);
        const Header headerOnStack(n, requiredCapacity);
        const size_t fileSize = headerOnStack.fileSize;

        // Create the file.
        const int fileDescriptor = openNew(name);

        // Make it the size we want.
        truncate(fileDescriptor, fileSize);

        // Map it in memory.
        void* pointer = map(fileDescriptor, fileSize, true);

        // There is no need to keep the file descriptor open.
        // Closing the file descriptor as early as possible will make it possible to use large
        // numbers of Vector objects all at the same time without having to increase
        // the limit on the number of concurrently open descriptors.
        ::close(fileDescriptor);

        // Figure out where the data and the header go.
        header = static_cast<Header*>(pointer);
        data = reinterpret_cast<T*>(header+1);

        // Store the header.
        *header = headerOnStack;

        // Call the default constructor on the data.
        for(size_t i=0; i<n; i++) {
            new(data+i) T();
        }

        // Indicate that the mapped vector is open with write access.
        isOpen = true;
        isOpenWithWriteAccess = true;
        fileName = name;

    } catch(std::exception& e) {
        cout << e.what() << endl;
        throw runtime_error("Error creating " + name);
    }

}



// Open a previously created vector with read-only or read-write access.
template<class T> inline void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::accessExisting(const string& name, bool readWriteAccess)
{
    try {
        // If already open, should have called close first.
        CZI_ASSERT(!isOpen);

        // Create the file.
        const int fileDescriptor = openExisting(name, readWriteAccess);

        // Find the size of the file.
        const size_t fileSize = getFileSize(fileDescriptor);

        // Now map it in memory.
        void* pointer = map(fileDescriptor, fileSize, readWriteAccess);

        // There is no need to keep the file descriptor open.
        // Closing the file descriptor as early as possible will make it possible to use large
        // numbers of Vector objects all at the same time without having to increase
        // the limit on the number of concurrently open descriptors.
        ::close(fileDescriptor);

        // Figure out where the data and the header are.
        header = static_cast<Header*>(pointer);
        data = reinterpret_cast<T*>(header+1);

        // Sanity checks.
        CZI_ASSERT(header->magicNumber == Header::constantMagicNumber);
        CZI_ASSERT(header->fileSize == fileSize);
        CZI_ASSERT(header->objectSize == sizeof(T));

        // Indicate that the mapped vector is open with write access.
        isOpen = true;
        isOpenWithWriteAccess = readWriteAccess;
        fileName = name;

    } catch(std::exception& e) {
        throw runtime_error("Error accessing " + name + ": " + e.what());
    }

}
template<class T> inline void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::accessExistingReadOnly(const string& name)
{
    accessExisting(name, false);
}
template<class T> inline
    void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::accessExistingReadWrite(
        const string& name,
        bool allowReadOnly)
{
    if(allowReadOnly) {
        try {
            accessExisting(name, true);     // Try read-write access.
        } catch(runtime_error e) {
            accessExisting(name, false);    // Read-write access failed, try read-only access.
            cout << name << " was accessed with read-only access. "
                "A segmentation fault will occur if write access is attempted." << endl;
        }
    } else {
        accessExisting(name, true);
    }
}



// Sync the mapped memory to disk.
template<class T> inline void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::syncToDisk()
{
    CZI_ASSERT(isOpen);
    const int msyncReturnCode = ::msync(header, header->fileSize, MS_SYNC);
    if(msyncReturnCode == -1) {
        throw runtime_error("Error during msync for " + fileName);
    }
}

// Unmap the memory.
template<class T> inline void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::unmap()
{
    CZI_ASSERT(isOpen);

    const int munmapReturnCode = ::munmap(header, header->fileSize);
    if(munmapReturnCode == -1) {
        throw runtime_error("Error unmapping " + fileName);
    }

    // Mark it as not open.
    isOpen = false;
    isOpenWithWriteAccess = false;
    header = 0;
    data = 0;
    fileName = "";

}

// Sync the mapped memory to disk, then unmap it.
template<class T> inline void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::close()
{
    CZI_ASSERT(isOpen);
    syncToDisk();
    unmap();
}

// Close it and remove the supporting file.
template<class T> inline void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::remove()
{
    const string savedFileName = fileName;
    close();	// This forgets the fileName.
    filesystem::remove(savedFileName);
}



// Resize works as for std::vector.
template<class T> inline void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::resize(size_t newSize)
{
    CZI_ASSERT(isOpenWithWriteAccess);

    const size_t oldSize = size();
    if(newSize == oldSize) {

        // No change in length - nothing to do.

    }
    if(newSize < oldSize) {

        // The vector is shrinking.
        // Just call the destructor on the elements that go away.
        for(size_t i=newSize; i<oldSize; i++) {
            (data+i)->~T();
        }
        header->objectCount = newSize;

    } else {

        // The vector is getting longer.
        if(newSize <= capacity()) {

            // No reallocation needed.
            header->objectCount = newSize;

            // Call the constructor on the elements we added.
            for(size_t i=oldSize; i<newSize; i++) {
                new(data+i) T();
            }


        } else {

            // The vector is growing beyond the current capacity.
            // We need to resize the mapped file.
            // Note that we don't have to copy the existing vector elements.

            // Save the file name and close it.
            const string name = fileName;
            close();

            // Create a header corresponding to increased capacity.
            const Header headerOnStack(newSize, size_t(1.5*double(newSize)));

            // Resize the file as necessary.
            const int fileDescriptor = openExisting(name, true);
            truncate(fileDescriptor, headerOnStack.fileSize);

            // Remap it.
            void* pointer = map(fileDescriptor, headerOnStack.fileSize, true);
            ::close(fileDescriptor);

            // Figure out where the data and the header are.
            header = static_cast<Header*>(pointer);
            data = reinterpret_cast<T*>(header+1);

            // Store the header.
            *header = headerOnStack;

            // Indicate that the mapped vector is open with write access.
            isOpen = true;
            isOpenWithWriteAccess = true;
            fileName = name;

            // Call the constructor on the elements we added.
            for(size_t i=oldSize; i<newSize; i++) {
                new(data+i) T();
            }
        }
    }

}



template<class T> inline void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::reserve()
{
    CZI_ASSERT(isOpenWithWriteAccess);
    reserve(size());
}


template<class T> inline void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::reserve(size_t capacity)
{
    CZI_ASSERT(isOpenWithWriteAccess);
    CZI_ASSERT(capacity >= size());
    if(capacity == header->capacity) {
        return;
    }

    // Save the file name and close it.
    const string name = fileName;
    close();

    // Create a header corresponding to increased capacity.
    const Header headerOnStack(size(), capacity);

    // Resize the file as necessary.
    const int fileDescriptor = openExisting(name, true);
    truncate(fileDescriptor, headerOnStack.fileSize);

    // Remap it.
    void* pointer = map(fileDescriptor, headerOnStack.fileSize, true);
    ::close(fileDescriptor);

    // Figure out where the data and the header are.
    header = static_cast<Header*>(pointer);
    data = reinterpret_cast<T*>(header+1);

    // Store the header.
    *header = headerOnStack;

    // Indicate that the mapped vector is open with write access.
    isOpen = true;
    isOpenWithWriteAccess = true;
    fileName = name;
}



// Make a copy of the Vector.
template<class T> inline void ChanZuckerberg::ExpressionMatrix2::MemoryMapped::Vector<T>::makeCopy(
    Vector<T>& copy, const string& newName) const
    {
    copy.createNew(newName, size());
    std::copy(begin(), end(), copy.begin());
}



inline void ChanZuckerberg::ExpressionMatrix2::testMemoryMappedVector()
{
    // Test creation of a temporary vector.
    MemoryMapped::Vector<int> x;
    x.createNew("./", 5);
    x[4] = 18;
    CZI_ASSERT(x[4] == 18);
}


#endif
