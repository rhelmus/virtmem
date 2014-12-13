#ifndef WRAPPER_H
#define WRAPPER_H

#include "alloc.h"
#include "base_wrapper.h"

template <typename T, typename TA> class CVirtPtr : public CVirtPtrBase
{
public:
    typedef T *TPtr;
    typedef TA TAllocator;

private:
    typedef CVirtPtr<T, TAllocator> TVirtPtr;

    static T *read(TVirtPointer p, bool ro=true)
    {
        if (isWrapped(p)) // NOTE: wrapped pointer, always ro
            return *static_cast<T **>(getAlloc()->read(getPtrAddr(p), sizeof(T), true));
        return static_cast<T *>(getAlloc()->read(p, sizeof(T), ro));
    }
    T *read(bool ro=true) { return read(ptr, ro); }
    const T *readConst(void) const { return read(ptr, true); }

    static void write(TVirtPointer p, const T *d)
    {
        if (isWrapped(p))
            *read(p) = *d;
        else
            getAlloc()->write(p, d, sizeof(T));
    }

public:
    class CValueWrapper
    {
        TVirtPointer ptr;

        CValueWrapper(TVirtPointer p) : ptr(p) { }
        CValueWrapper(const CValueWrapper &);

        template <typename, typename> friend class CVirtPtr;

    public:
        inline operator T(void) const { return *read(ptr); }

        template <typename T2> inline operator T2(void) const { return static_cast<T2>(operator T()); } // UNDONE: explicit?
        CValueWrapper &operator=(const CValueWrapper &v)
        {
            if (ptr != v.ptr)
                write(ptr, read(v.ptr));
            return *this;
        }
        inline CValueWrapper &operator=(const T &v) { write(ptr, &v); return *this; }
        inline TVirtPtr operator&(void) { TVirtPtr ret; ret.ptr = ptr; return ret; }

        // allow pointer to pointer access
        inline T operator->(void) { return operator T(); }
        inline const T operator->(void) const { return operator T(); }

        inline bool operator==(const class CNILL &) const { return getPtrAddr(ptr) == 0; }
        template <typename T2> inline bool operator==(const T2 &v) const { return operator T() == v; }
        template <typename T2> inline bool operator!=(const T2 &v) const { return operator T() != v; }
        inline T operator-(const T &v) const { return operator T() - v; }
        // UNDONE: more operators
    };

    // C style malloc/free
    static TVirtPtr alloc(TVirtSizeType size=sizeof(T))
    {
        TVirtPtr ret;
        ret.ptr = getAlloc()->alloc(size);
        return ret;
    }

    static void free(TVirtPtr &p)
    {
        getAlloc()->free(p.ptr);
    }

    // C++ style new/delete --> call constructors (by placement new) and destructors
    static TVirtPtr newClass(TVirtSizeType size=sizeof(T))
    {
        TVirtPtr ret = alloc(size);
        new (read(ret.ptr, false)) T; // UNDONE: can this be ro?
        return ret;
    }

    static void deleteClass(TVirtPtr &p)
    {
        read(p.ptr)->~T();
        virtFree(p);
    }

    // C++ style new []/delete [] --> calls constructors and destructors
    // the size of the array (necessary for destruction) is stored at the beginning of the block.
    // A pointer offset from this is returned.
    static TVirtPtr newArray(TVirtSizeType elements)
    {
        TVirtPtr ret = alloc(sizeof(T) * elements + sizeof(TVirtSizeType));
        getAlloc()->write(ret.ptr, &elements, sizeof(TVirtSizeType));
        ret.ptr += sizeof(TVirtSizeType);
        for (TVirtSizeType s=0; s<elements; ++s)
            new (getAlloc()->read(ret.ptr + (s * sizeof(T)), sizeof(T), false)) T; // UNDONE: can this be ro?
        return ret;
    }
    static void deleteArray(TVirtPtr &p)
    {
        const TVirtPointer soffset = p.ptr - sizeof(TVirtSizeType); // pointer to size offset
        TVirtSizeType *size = getAlloc()->read(soffset, sizeof(TVirtSizeType));
        for (TVirtSizeType s=0; s<*size; ++s)
            (static_cast<T *>(getAlloc()->read(p.ptr, sizeof(T)) + (s * sizeof(T))))->~T();
        getAlloc()->free(soffset); // soffset points at beginning of actual block
    }

    static TVirtPtr wrap(void *p)
    {
        TVirtPtr ret = alloc(sizeof(p));
        getAlloc()->write(ret.ptr, &p, sizeof(p));
        ret.ptr = getWrapped(ret.ptr);
        return ret;
    }
    const T *unwrap(void) const { return readConst(); }

    CValueWrapper operator*(void) { return CValueWrapper(ptr); }
    T *operator->(void) { return read(false); }
    const T *operator->(void) const { return readConst(); }
    // allow double wrapped pointer
    CVirtPtr<TVirtPtr, TAllocator> operator&(void) { return wrap(this); }

    // const conversion
    inline operator CVirtPtr<const T, TAllocator>(void) { CVirtPtr<const T, TAllocator> ret; ret.ptr = ptr; return ret; }
    // pointer to pointer conversion
    template <typename T2> EXPLICIT inline operator CVirtPtr<T2, TAllocator>(void) { CVirtPtr<T2, TAllocator> ret; ret.ptr = ptr; return ret; }

    TVirtPtr &operator+=(int n) { ptr += (n * sizeof(T)); return *this; }
    TVirtPtr &operator++(void) { ptr += sizeof(T); return *this; }
    TVirtPtr operator++(int) { TVirtPtr ret; ret.ptr = ptr + sizeof(T); return ret; }
    TVirtPtr &operator--(void) { ptr -= sizeof(T); return *this; }
    TVirtPtr operator--(int) { TVirtPtr ret; ret.ptr = ptr -= sizeof(T); return ret; }
    TVirtPtr operator+(int n) const { TVirtPtr ret; ret.ptr = ptr + (n * sizeof(T)); return ret; }
    TVirtPtr operator-(int n) const { TVirtPtr ret; ret.ptr = ptr - (n * sizeof(T)); return ret; }
    int operator-(const TVirtPtr &other) const { return (ptr - other.ptr) / sizeof(T); }

    inline bool operator!=(const TVirtPtr &p) const { return ptr != p.ptr; }

    const CValueWrapper operator[](int i) const { return CValueWrapper(ptr + (i * sizeof(T))); }
    CValueWrapper operator[](int i) { return CValueWrapper(ptr + (i * sizeof(T))); }

    static inline TAllocator *getAlloc(void) { return static_cast<TAllocator *>(TAllocator::getInstance()); }
};


#endif // WRAPPER_H
