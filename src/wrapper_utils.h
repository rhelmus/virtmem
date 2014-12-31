#ifndef WRAPPER_UTILS_H
#define WRAPPER_UTILS_H

#include "wrapper.h"

#include <string.h>

// Generic NULL type
class CNILL
{
public:
    template <typename T> inline operator T*(void) const { return 0; }
    template <typename T, typename A> inline operator CVirtPtr<T, A>(void) const { return CVirtPtr<T, A>(); }
    inline operator CVirtPtrBase(void) const { return CVirtPtrBase(); }
    template <typename T, typename A> inline operator typename CVirtPtr<T, A>::CValueWrapper(void) const { return CVirtPtr<T, A>::CValueWrapper(0); }

} extern const NILL;

template <typename TV> class CPtrWrapLock
{
    TV ptrWrap;
    bool readOnly;

public:
    CPtrWrapLock(const TV &w, bool ro=false) : ptrWrap(w) { lock(ro); }
    ~CPtrWrapLock(void) { unlock(); }
    // add extra lock on copying
    CPtrWrapLock(const CPtrWrapLock &other) : ptrWrap(other.ptrWrap), readOnly(other.readOnly) { lock(readOnly); }

    void lock(bool ro=false)
    {
        if (!ptrWrap.isWrapped())
            TV::getAlloc()->lock(ptrWrap.ptr, ro);
        readOnly = ro;
    }
    void unlock(void)
    {
        if (!ptrWrap.isWrapped())
            TV::getAlloc()->unlock(ptrWrap.ptr);
    }
    typename TV::TPtr operator *(void) { return ptrWrap.read(); }
};

// Shortcut
template <typename T> CPtrWrapLock<T> makeVirtPtrLock(const T &w, bool ro=false) { return CPtrWrapLock<T>(w, ro); }

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
// for regular pointers
template <typename C, typename M> inline M *getMembrPtr(const C *c, const M C::*m) { return (M *)((char *)c + private_utils::getMembrOffset(m)); }
template <typename C, typename M, typename NC, typename NM> inline NM *getMembrPtr(const C *c, M const C::*m, const NM NC::*m2)
{ return (M *)((char *)c + private_utils::getMembrOffset(m) + private_utils::getMembrOffset(m2)); }

#include "utils.hpp"

#endif // WRAPPER_UTILS_H
