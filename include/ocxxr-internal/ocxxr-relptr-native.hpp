#ifndef OCXXR_RELPTR_NATIVE_HPP_
#define OCXXR_RELPTR_NATIVE_HPP_

/**
 * Faux relative pointers
 * (they're really native pointers in disguise)
 * used for testing/debugging in shared memory
 * (this makes for a nice performance baseline)
 */
namespace ocxxr {

template <typename T>
class BasedPtr;

template <typename T>
class RelPtr {
    static_assert(false, "NOT IMPLEMENTED");
};

template <typename T>
class BasedPtr {
    static_assert(false, "NOT IMPLEMENTED");
};

}  // namespace ocxxr

#endif  // OCXXR_RELPTR_NATIVE_HPP_
