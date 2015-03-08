#ifndef VIRTMEM_WRAPPER_H
#define VIRTMEM_WRAPPER_H

#include "alloc.h"
#include "base_wrapper.h"
#include "utils.h"

#include <stddef.h>

//#include <stdio.h> // UNDONE

template <typename T, typename TA> class CVirtPtr;

namespace private_utils {

template <typename T> struct TDereferenced { typedef T type; }; // dummy for non pointers
template <typename T> struct TDereferenced<T *(*)()> { typedef T *type; }; // dummy for function pointers. // UNDONE: only handles 1 arg
template <typename T> struct TDereferenced<T *> { typedef T type; };
template <typename T, typename A> struct TDereferenced<CVirtPtr<T, A> > { typedef T type; };

// get pointer to variable, use char & cast to override any operator & overload
template <typename T> T *pointerTo(const T &val) { return (T *)&(char &)val; }
}

template <typename T, typename TA> class CVirtPtr : public CVirtPtrBase
{
public:
    typedef T *TPtr;
    typedef TA TAllocator;

private:
    typedef CVirtPtr<T, TAllocator> TVirtPtr;

    static T *read(TPtrNum p)
    {
        if (!p)
            return 0;
        if (isWrapped(p))
            return static_cast<T *>(CVirtPtrBase::unwrap(p));
        return static_cast<T *>(getAlloc()->read(p, sizeof(T)));
    }
    T *read(void) const { return read(ptr); }

    static void write(TPtrNum p, const T *d)
    {
        if (isWrapped(p))
            *(static_cast<T *>(CVirtPtrBase::unwrap(p))) = *d;
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

//        CValueWrapper &operator=(const CValueWrapper &v)
        // HACK: 'allow' non const assignment of types. In reality this makes sure we don't define
        // the assignment operator twice for const types (where T == const T)
        CValueWrapper &operator=(const typename CVirtPtr<typename private_utils::TAntiConst<T>::type, TA>::CValueWrapper &v)
        {
            ASSERT(ptr != 0);
            if (ptr != v.ptr)
            {
                const T val = *read(v.ptr);
                write(ptr, &val);
            }
            return *this;
        }
        // const conversion
        CValueWrapper &operator=(const typename CVirtPtr<const T, TA>::CValueWrapper &v)
        {
            ASSERT(ptr != 0);
            if (ptr != v.ptr)
            {
                const T val = *read(v.ptr);
                write(ptr, &val);
            }
            return *this;
        }

        inline CValueWrapper &operator=(const T &v) { write(ptr, &v); return *this; }
        inline TVirtPtr operator&(void) { TVirtPtr ret; ret.ptr = ptr; return ret; }

        // allow pointer to pointer access
        // UNDONE: use member wrapper here?
        inline T operator->(void) { return operator T(); }
        inline const T operator->(void) const { return operator T(); }
        inline typename CVirtPtr<typename private_utils::TDereferenced<T>::type, TA>::CValueWrapper operator*(void) { return *operator T(); }
        inline const typename CVirtPtr<typename private_utils::TDereferenced<T>::type, TA>::CValueWrapper operator*(void) const { return *operator T(); }

        typename CVirtPtr<typename private_utils::TDereferenced<T>::type, TA>::CValueWrapper operator[](int i)
        { return operator T()[i]; }
        const typename CVirtPtr<typename private_utils::TDereferenced<T>::type, TA>::CValueWrapper operator[](int i) const
        { return operator T()[i]; }

        template <typename T2> inline bool operator==(const T2 &v) const { return operator T() == v; }
        template <typename T2> inline bool operator!=(const T2 &v) const { return operator T() != v; }

        CValueWrapper &operator+=(int n) { T newv = operator T() + n; write(ptr, private_utils::pointerTo(newv)); return *this; }
        CValueWrapper &operator-=(int n) { return operator+=(-n); }
        CValueWrapper &operator*=(int n) { T newv = operator T() * n; write(ptr, private_utils::pointerTo(newv)); return *this; }
        CValueWrapper &operator/=(int n) { T newv = operator T() / n; write(ptr, private_utils::pointerTo(newv)); return *this; }
        CValueWrapper &operator++(void) { return operator +=(1); }
        T operator++(int) { T ret = operator T(); operator++(); return ret; }
    };

    // Based on Strousstrup's general wrapper paper (http://www.stroustrup.com/wrapper.pdf)
    class CMemberWrapper
    {
        const TPtrNum ptr;

        template <typename, typename> friend class CVirtPtr;

        CMemberWrapper(TPtrNum p) : ptr(p) { }
        CMemberWrapper(const CMemberWrapper &);

        CMemberWrapper &operator=(const CMemberWrapper &); // No assignment

    public:
        ~CMemberWrapper(void) { if (!isWrapped(ptr)) getAlloc()->releaseLock(ptr); }

        T *operator->(void)
        {
            if (isWrapped(ptr))
                return static_cast<T *>(CVirtPtrBase::unwrap(ptr));
            return static_cast<T *>(getAlloc()->makeLock(getPtrNum(ptr), sizeof(T)));
        }
        const T *operator->(void) const
        {
            if (isWrapped(ptr))
                return static_cast<T *>(CVirtPtrBase::unwrap(ptr));

            return static_cast<T *>(getAlloc()->makeLock(getPtrNum(ptr), sizeof(T), true));
        }
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

    // UNDONE
#ifndef ARDUINO
    // C++ style new/delete --> call constructors (by placement new) and destructors
    static TVirtPtr newClass(TVirtPtrSize size=sizeof(T))
    {
        TVirtPtr ret = alloc(size);
        new (read(ret.ptr)) T; // UNDONE: can this be ro?
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
            new (getAlloc()->read(ret.ptr + (s * sizeof(T)), sizeof(T))) T; // UNDONE: can this be ro?
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
#endif

    static TVirtPtr wrap(const void *p)
    {
        TVirtPtr ret;
        ret.ptr = getWrapped(reinterpret_cast<TPtrNum>(p));
        return ret;
    }
    static T *unwrap(const TVirtPtr &p) { return static_cast<T *>(CVirtPtrBase::unwrap(p)); }
    T *unwrap(void) { return static_cast<T *>(CVirtPtrBase::unwrap(ptr)); }
    const T *unwrap(void) const { return static_cast<const T *>(CVirtPtrBase::unwrap(ptr)); }

    CValueWrapper operator*(void) { return CValueWrapper(ptr); }
    CMemberWrapper operator->(void) { return CMemberWrapper(ptr); }
    const CMemberWrapper operator->(void) const { return CMemberWrapper(ptr); }
    // allow double wrapped pointer
    CVirtPtr<TVirtPtr, TAllocator> operator&(void) { CVirtPtr<TVirtPtr, TAllocator> ret = ret.wrap(this); return ret; }

    // const conversion
    inline operator CVirtPtr<const T, TAllocator>(void) { CVirtPtr<const T, TAllocator> ret; ret.ptr = ptr; return ret; }
    // pointer to pointer conversion
    template <typename T2> EXPLICIT inline operator CVirtPtr<T2, TAllocator>(void) { CVirtPtr<T2, TAllocator> ret; ret.ptr = ptr; return ret; }

    const CVirtPtr *addressOf(void) const { return this; }
    CVirtPtr *addressOf(void) { return this; }

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


#endif // VIRTMEM_WRAPPER_H
