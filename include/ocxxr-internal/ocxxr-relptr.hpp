#ifndef OCXXR_RELPTR_HPP_
#define OCXXR_RELPTR_HPP_

namespace ocxxr {

template <typename T>
class RelPtr;

/**
 * This is our "based pointer" class.
 * You should be able to use it pretty much just like a normal pointer.
 * This class is safer than RelPtr, but not as efficient.
 */
template <typename T>
class BasedPtr {
 public:
    constexpr BasedPtr() : target_guid_(ERROR_GUID), offset_(0) {}

    BasedPtr(ocrGuid_t target, ptrdiff_t offset)
            : target_guid_(target), offset_(offset) {}

#if 0  // DISABLED
    BasedPtr(const BasedPtr &other) { set(other); }
#endif

    BasedPtr(const BasedPtr &) = default;

    BasedPtr(const T *other) { set(other); }

    // TODO - ensure that default assignment operator still works correctly
    // must handle special cases too
    BasedPtr &operator=(const T *other) {
        set(other);
        return *this;
    }

#if 0  // DISABLED
    BasedPtr &operator=(const BasedPtr &other) {
        set(other);
        return *this;
    }
#endif

    BasedPtr &operator=(const BasedPtr &) = default;

    T &operator*() const { return *get(); }

    T *operator->() const { return get(); }

    T &operator[](const int index) const { return get()[index]; }

    operator T *() const { return get(); }

    operator RelPtr<T>() const { return get(); }

    bool operator!() const { return ocrGuidIsNull(target_guid_); }

    bool operator==(const BasedPtr &other) const {
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

    void set(const BasedPtr &other, bool embedded = false) {
        if (embedded && ocrGuidIsUninitialized(other.target_guid_)) {
            set(other.get());
        } else {
            target_guid_ = other.target_guid_;
            offset_ = other.offset_;
        }
    }

    void set(const T *other, bool embedded = false) {
        internal::GuidOffsetForAddress(other, this, &target_guid_, &offset_,
                                       embedded);
    }

    T *get(bool embedded = false) const {
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
