#ifndef OCXXR_NPRELPTR_HPP_
#define OCXXR_NPRELPTR_HPP_

namespace ocxxr {

/**
 * This is our "relative pointer" class implemented by native pointer.
 * You should be able to use it pretty much just like a normal pointer.
 * However, you still need to be careful to only point to memory within
 * the same datablock. Nothing keeps you from creating a "relative pointer"
 * into another datablock, or even into the stack.
 */

template <typename T>
class NativePointerWrapper {
 public:

 	NativePointerWrapper() { set(nullptr); }

 	NativePointerWrapper(const NativePointerWrapper &other) { set(other);}

	NativePointerWrapper(const T *other) { set(other); }

	T &operator*() const { return *get(); }

	T *operator->() const {return get(); };

	T &operator[](const int index) const { return get()[index]; }

	operator T *() const { return get(); }

 private:

 	T *nativePtr;
	
	ocrGuid_t guid;

 protected:
	void set(const NativePointerWrapper &other) {
		nativePtr = const_cast<T*>(other.nativePtr);
		guid = other.guid;
	}

	void set(const T *other) {
		nativePtr = const_cast<T*>(other);
		guid = ERROR_GUID;
	}

	T *get() const { return nativePtr; }

};

template <typename T>
class RelPtr : public NativePointerWrapper<T> {
 public:
    RelPtr() : NativePointerWrapper<T>() {} 

    RelPtr(const RelPtr &other) : NativePointerWrapper<T>(other) {}

    RelPtr(const T *other) : NativePointerWrapper<T>(other) {}

    // TODO - ensure that default assignment operator still works correctly
    RelPtr<T> &operator=(const RelPtr &other) {
        NativePointerWrapper<T>::set(other);
        return *this;
    }

    RelPtr<T> &operator=(const T *other) {
        NativePointerWrapper<T>::set(other);
        return *this;
    }


};

/**
 * This is our "based pointer" class implemented by native pointer.
 * You should be able to use it pretty much just like a normal pointer.
 * This class is safer than RelPtr, but not as efficient.
 */
template <typename T>
class BasedPtr : public NativePointerWrapper<T> {
 public:
    BasedPtr() : NativePointerWrapper<T>() {}

    BasedPtr(const BasedPtr &other) : NativePointerWrapper<T>(other) {}

    BasedPtr(const T *other) : NativePointerWrapper<T>(other) {}

    // TODO - ensure that default assignment operator still works correctly
    // must handle special cases too
    BasedPtr &operator=(const T *other) {
        NativePointerWrapper<T>::set(other);
        return *this;
    }

    BasedPtr &operator=(const BasedPtr &other) {
        NativePointerWrapper<T>::set(other);
        return *this;
    }

};

namespace internal {

template <typename T, unsigned N, template <typename> class P = RelPtr>
struct PointerNester {
    typedef P<typename PointerNester<T, N - 1, P>::Type> Type;
};

template <typename T, template <typename> class P>
struct PointerNester<T, 0, P> {
    typedef T Type;
};

template <typename T, template <typename> class P = RelPtr>
struct PointerConvertor;

template <typename T, template <typename> class P>
struct PointerConvertor<T *, P> {
    typedef P<T> Type;
};

template <typename T, template <typename> class P>
struct PointerConvertor<T **, P> {
    typedef P<typename PointerConvertor<T *, P>::Type> Type;
};

}  // namespace internal

template <typename T, unsigned N>
using NestedRelPtr = typename internal::PointerNester<T, N>::Type;

template <typename T>
using RelPtrFor = typename internal::PointerConvertor<T>::Type;

template <typename T, unsigned N>
using NestedBasedPtr = typename internal::PointerNester<T, N, BasedPtr>::Type;

template <typename T>
using BasedPtrFor = typename internal::PointerConvertor<T, BasedPtr>::Type;

}  // namespace ocxxr

#endif  // OCXXR_NPRELPTR_HPP_
