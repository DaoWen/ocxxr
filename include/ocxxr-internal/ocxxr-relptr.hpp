#ifndef OCXXR_RELPTR_HPP_
#define OCXXR_RELPTR_HPP_

namespace ocxxr {

// FIXME - should also support conversion to superclass pointer types
// e.g., BasedPtr<Y> y = ...; BasedPtr<X> x = y; should work if Y extends X

template <typename T>
class RelPtr;

namespace internal {

template <typename T, bool embedded>
class BasedPtrImpl {
 public:
    constexpr BasedPtrImpl() : target_guid_(ERROR_GUID), offset_(0) {}

    BasedPtrImpl(ocrGuid_t target, ptrdiff_t offset)
            : target_guid_(target), offset_(offset) {}

    template <typename U = T, EnableIf<embedded && std::is_same<T, U>::value> = 0>
    BasedPtrImpl(const BasedPtrImpl &other) { set(other); }

    // Should be auto-generated if above version is disabled
    // BasedPtrImpl(const BasedPtrImpl &) = default;

    BasedPtrImpl(const T *other) { set(other); }

    // TODO - ensure that default assignment operator still works correctly
    // must handle special cases too
    BasedPtrImpl &operator=(const T *other) {
        set(other);
        return *this;
    }

    template <typename U = T, EnableIf<embedded && std::is_same<T, U>::value> = 0>
    BasedPtrImpl &operator=(const BasedPtrImpl &other) {
        set(other);
        return *this;
    }

    // Should be auto-generated if above version is disabled
    // BasedPtrImpl &operator=(const BasedPtrImpl &) = default;

    T &operator*() const { return *get(); }

    T *operator->() const { return get(); }

    T &operator[](const int index) const { return get()[index]; }

    operator T *() const { return get(); }

    operator RelPtr<T>() const { return get(); }

    // Allow conversion from optimized "embedded" case to general case
    operator BasedPtrImpl<T, false>() const { return get(); }

    bool operator!() const { return ocrGuidIsNull(target_guid_); }

    bool operator==(const BasedPtrImpl &other) const {
        return ocrGuidIsEq(target_guid_, other.target_guid_) &&
               offset_ == other.offset_;
    }

    ocrGuid_t target_guid() const { return target_guid_; }

    ArenaHandle<void> target_handle() const {
        return ArenaHandle<void>(target_guid_);
    }

    Arena<void> target_arena() const {
        void *ptr = reinterpret_cast<void *>(
                internal::AddressForGuid(target_guid_));
        ocrEdtDep_t dep = {.guid = target_guid_, .ptr = ptr};
        return Arena<void>(dep);
    }

    // TODO - implement math operators, like increment and decrement

 private:
    ocrGuid_t target_guid_;
    ptrdiff_t offset_;

 protected:
    ptrdiff_t base_ptr() const { return reinterpret_cast<ptrdiff_t>(this); }

    void set(const BasedPtrImpl &other) {
        if (embedded && ocrGuidIsUninitialized(other.target_guid_)) {
            set(other.get());
        } else {
            target_guid_ = other.target_guid_;
            offset_ = other.offset_;
        }
    }

    void set(const T *other) {
        internal::GuidOffsetForAddress(other, this, &target_guid_, &offset_,
                                       embedded);
    }

    T *get() const {
        assert(!ocrGuidIsError(target_guid_));
        if (ocrGuidIsNull(target_guid_)) {
            return nullptr;
        } else if (embedded && ocrGuidIsUninitialized(target_guid_)) {
            // optimized case: treat as intra-datablock RelPtr
            ptrdiff_t target = internal::CombineBaseOffset(base_ptr(), offset_);
            return reinterpret_cast<T *>(target);
        } else {
// normal case: inter-datablock pointer
#if OCXXR_USE_NATIVE_POINTERS
            constexpr ptrdiff_t base = 0;
#else   // !OCXXR_USE_NATIVE_POINTERS
            ptrdiff_t base = internal::AddressForGuid(target_guid_);
#endif  // OCXXR_USE_NATIVE_POINTERS
            ptrdiff_t target = internal::CombineBaseOffset(base, offset_);
            return reinterpret_cast<T *>(target);
        }
    }
};

}  // namespace internal

/**
 * This is our "based pointer" class.
 * You should be able to use it pretty much just like a normal pointer.
 * This class is safer than RelPtr, but not as efficient.
 */
template <typename T>
using BasedPtr = internal::BasedPtrImpl<T, false>;

/**
 * This is our "based datablock pointer" class.
 * Behaves like a BasedPtr in most cases, but can be optimized
 * to behave like a RelPtr if this and the target object
 * are in the same datablock. As a consequence, these pointer
 * objects may only be allocated within an acquired datablock.
 */
template <typename T>
using BasedDbPtr = internal::BasedPtrImpl<T, true>;

/**
 * This is our "relative pointer" class.
 * You should be able to use it pretty much just like a normal pointer.
 * However, you still need to be careful to only point to memory within
 * the same datablock. Nothing keeps you from creating a "relative pointer"
 * into another datablock, or even into the stack.
 */
template <typename T>
class RelPtr {
 public:
    // offset of 1 is impossible since this is larger than 1 byte
    constexpr RelPtr() : offset_(1) {}

    RelPtr(const RelPtr &other) { set(other); }

    RelPtr(const T *other) { set(other); }

    // TODO - ensure that default assignment operator still works correctly
    RelPtr<T> &operator=(const RelPtr &other) {
        set(other);
        return *this;
    }

    RelPtr<T> &operator=(const T *other) {
        set(other);
        return *this;
    }

    T &operator*() const { return *get(); }

    T *operator->() const { return get(); }

    T &operator[](const int index) const { return get()[index]; }

    operator T *() const { return get(); }

    operator BasedPtr<T>() const { return get(); }

    bool operator!() const { return offset_ == 0; }

    bool operator==(const RelPtr &other) const { return get() == other.get(); }

    // TODO - implement math operators, like increment and decrement

 private:
    ptrdiff_t offset_;

    ptrdiff_t base_ptr() const { return reinterpret_cast<ptrdiff_t>(this); }

    void set(const RelPtr &other) { set(other.get()); }

    void set(const T *other) {
        if (other == nullptr) {
            offset_ = 0;
        } else {
            ptrdiff_t ptr = reinterpret_cast<ptrdiff_t>(other);
            offset_ = internal::CombineBaseOffset(-base_ptr(), ptr);
        }
    }

    T *get() const {
        assert(offset_ != 1);
        if (offset_ == 0) {
            return nullptr;
        } else {
            ptrdiff_t target = internal::CombineBaseOffset(base_ptr(), offset_);
            return reinterpret_cast<T *>(target);
        }
    }
};

}  // namespace ocxxr

#endif  // OCXXR_RELPTR_HPP_
