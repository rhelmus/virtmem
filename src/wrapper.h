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

    static T *unwrap(TPtrNum p) { return reinterpret_cast<T *>(getPtrNum(p)); }

    static T *read(TPtrNum p, bool ro=true)
    {
        if (isWrapped(p))
            return unwrap(p);
        return static_cast<T *>(getAlloc()->read(p, sizeof(T), ro));
    }
    T *read(bool ro=true) { return read(ptr, ro); }
    const T *readConst(void) const { return read(ptr, true); }

    static void write(TPtrNum p, const T *d)
    {
        if (isWrapped(p))
            *unwrap(p) = *d;
        else
            getAlloc()->write(p, d, sizeof(T));
    }
    void write(const T *d) { write(ptr, d); }

    TVirtPtr copy(void) const { TVirtPtr ret; ret.ptr = ptr; return ret; }
    template <typename> friend class CPtrWrapLock;

public:
    class CValueWrapper
    {
        TPtrNum ptr;

        CValueWrapper(TPtrNum p) : ptr(p) { }
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

        /*inline bool operator==(const class CNILL &) const { return getPtrNum(ptr) == 0; }
        inline bool operator!=(const class CNILL &) const { return getPtrNum(ptr) != 0; }*/
        template <typename T2> inline bool operator==(const T2 &v) const { return operator T() == v; }
        template <typename T2> inline bool operator!=(const T2 &v) const { return operator T() != v; }
        inline T operator-(const T &v) const { return operator T() - v; }
        // UNDONE: more operators
    };

    // C style malloc/free
    static TVirtPtr alloc(TVirtPtrSize size=sizeof(T))
    {
        TVirtPtr ret;
        ret.ptr = getAlloc()->alloc(size);
        return ret;
    }

    static void free(TVirtPtr &p)
    {
        getAlloc()->free(p.ptr);
        p.ptr = 0;
    }

    // C++ style new/delete --> call constructors (by placement new) and destructors
    static TVirtPtr newClass(TVirtPtrSize size=sizeof(T))
    {
        TVirtPtr ret = alloc(size);
        new (read(ret.ptr, false)) T; // UNDONE: can this be ro?
        return ret;
    }

    static void deleteClass(TVirtPtr &p)
    {
        read(p.ptr)->~T();
        free(p);
    }

    // C++ style new []/delete [] --> calls constructors and destructors
    // the size of the array (necessary for destruction) is stored at the beginning of the block.
    // A pointer offset from this is returned.
    static TVirtPtr newArray(TVirtPtrSize elements)
    {
        TVirtPtr ret = alloc(sizeof(T) * elements + sizeof(TVirtPtrSize));
        getAlloc()->write(ret.ptr, &elements, sizeof(TVirtPtrSize));
        ret.ptr += sizeof(TVirtPtrSize);
        for (TVirtPtrSize s=0; s<elements; ++s)
            new (getAlloc()->read(ret.ptr + (s * sizeof(T)), sizeof(T), false)) T; // UNDONE: can this be ro?
        return ret;
    }
    static void deleteArray(TVirtPtr &p)
    {
        const TPtrNum soffset = p.ptr - sizeof(TVirtPtrSize); // pointer to size offset
        TVirtPtrSize *size = static_cast<TVirtPtrSize *>(getAlloc()->read(soffset, sizeof(TVirtPtrSize)));
        for (TVirtPtrSize s=0; s<*size; ++s)
            (read(p.ptr + (s * sizeof(T))))->~T();
        getAlloc()->free(soffset); // soffset points at beginning of actual block
    }

    static TVirtPtr wrap(void *p)
    {
        TVirtPtr ret;
        ret.ptr = getWrapped(reinterpret_cast<TPtrNum>(p));
        return ret;
    }
    static T *unwrap(const TVirtPtr &p) { return reinterpret_cast<T *>(p.ptr); }
    T *unwrap(void) { return unwrap(ptr); }
    const T *unwrap(void) const { return unwrap(ptr); }

    CValueWrapper operator*(void) { return CValueWrapper(ptr); }
    T *operator->(void) { return read(false); }
    const T *operator->(void) const { return readConst(); }
    // allow double wrapped pointer
    CVirtPtr<TVirtPtr, TAllocator> operator&(void) { return wrap(this); }

    // const conversion
    inline operator CVirtPtr<const T, TAllocator>(void) { CVirtPtr<const T, TAllocator> ret; ret.ptr = ptr; return ret; }
    // pointer to pointer conversion
    template <typename T2> EXPLICIT inline operator CVirtPtr<T2, TAllocator>(void) { CVirtPtr<T2, TAllocator> ret; ret.ptr = ptr; return ret; }

    // NOTE: int cast is necessary to deal with negative numbers
    TVirtPtr &operator+=(int n) { ptr += (n * (int)sizeof(T)); return *this; }
    inline TVirtPtr &operator++(void) { return operator +=(1); }
    inline TVirtPtr operator++(int) { TVirtPtr ret = copy(); operator++(); return ret; }
    inline TVirtPtr &operator-=(int n) { return operator +=(-n); }
    inline TVirtPtr &operator--(void) { return operator -=(1); }
    inline TVirtPtr operator--(int) { TVirtPtr ret = copy(); operator--(); return ret; }
    inline TVirtPtr operator+(int n) const { return (copy() += n); }
    inline TVirtPtr operator-(int n) const { return (copy() -= n); }
    int operator-(const TVirtPtr &other) const { return (ptr - other.ptr) / sizeof(T); }
    inline bool operator!=(const TVirtPtr &p) const { return ptr != p.ptr; }

    const CValueWrapper operator[](int i) const { return CValueWrapper(ptr + (i * sizeof(T))); }
    CValueWrapper operator[](int i) { return CValueWrapper(ptr + (i * sizeof(T))); }

    static inline TAllocator *getAlloc(void) { return static_cast<TAllocator *>(TAllocator::getInstance()); }
};


#endif // WRAPPER_H