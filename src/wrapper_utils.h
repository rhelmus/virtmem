#ifndef VIRTMEM_WRAPPER_UTILS_H
#define VIRTMEM_WRAPPER_UTILS_H

#include "wrapper.h"

#include <string.h>

namespace virtmem {

// Generic NULL type
class CNILL
{
public:
    template <typename T> inline operator T*(void) const { return 0; }
    template <typename T, typename A> inline operator CVirtPtr<T, A>(void) const { return CVirtPtr<T, A>(); }
    inline operator CVirtPtrBase(void) const { return CVirtPtrBase(); }
    template <typename T, typename A> inline operator typename CVirtPtr<T, A>::CValueWrapper(void) const { return CVirtPtr<T, A>::CValueWrapper(0); }

} extern const NILL;

template <typename TV> class CVirtPtrLock
{
    typedef typename TV::TPtr TPtr;

    TV virtPtr;
    TPtr data;
    TVirtPageSize lockSize;
    bool readOnly;

public:
    CVirtPtrLock(const TV &v, TVirtPageSize s, bool ro=false) :
        virtPtr(v), lockSize(s), readOnly(ro) { lock(); }
    CVirtPtrLock(void) : data(0), lockSize(0), readOnly(false) { }
    ~CVirtPtrLock(void) { unlock(); }
    // add extra lock on copying
    CVirtPtrLock(const CVirtPtrLock &other) :
        virtPtr(other.virtPtr), lockSize(other.lockSize), readOnly(other.readOnly) { lock(); }

    void lock(void)
    {
#ifdef VIRTMEM_WRAP_CPOINTERS
        if (virtPtr.isWrapped())
            data = virtPtr.unwrap();
        else
#endif
            data = static_cast<TPtr>(TV::getAlloc()->makeFittingLock(virtPtr.ptr, lockSize, readOnly));
    }

    void lock(const TV &v, TVirtPageSize s, bool ro=false)
    {
        virtPtr = v; lockSize = s; readOnly = ro;
        lock();
    }

    void unlock(void)
    {
#ifdef VIRTMEM_WRAP_CPOINTERS
        if (!virtPtr.isWrapped() && data)
#else
        if (data)
#endif
        {
            TV::getAlloc()->releaseLock(virtPtr.ptr);
            data = 0;
        }
    }

    TPtr operator *(void) { return data; }
    TVirtPageSize getLockSize(void) const { return lockSize; }
};

// Shortcut
template <typename T> CVirtPtrLock<T> makeVirtPtrLock(const T &w, TVirtPageSize s, bool ro=false)
{ return CVirtPtrLock<T>(w, s, ro); }

namespace private_utils {
// Ugly hack from http://stackoverflow.com/a/12141673
// a null pointer of T is used to get the offset of m. The char & is to avoid any dereference operator overloads and the
// char * subtraction to get the actual offset.
template <typename C, typename M> ptrdiff_t getMembrOffset(const M C::*m)
{ return ((char *)&((char &)((C *)(0)->*m)) - (char *)(0)); }
}

template <typename C, typename M, typename A> inline CVirtPtr<M, A> getMembrPtr(const CVirtPtr<C, A> &c, const M C::*m)
{ CVirtPtr<M, A> ret; ret.setRawNum(c.getRawNum() + private_utils::getMembrOffset(m)); return ret; }
// for nested member
template <typename C, typename M, typename NC, typename NM, typename A> inline CVirtPtr<NM, A> getMembrPtr(const CVirtPtr<C, A> &c, const M C::*m, const NM NC::*nm)
{ CVirtPtr<NM, A> ret = static_cast<CVirtPtr<NM, A> >(static_cast<CVirtPtr<char, A> >(getMembrPtr(c, m)) + private_utils::getMembrOffset(nm)); return ret; }

}

#include "utils.hpp"

#endif // VIRTMEM_WRAPPER_UTILS_H
