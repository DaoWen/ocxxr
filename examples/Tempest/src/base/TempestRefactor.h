#ifndef _TEMPEST_REFACTOR_H
#define _TEMPEST_REFACTOR_H

#if defined(USE_OCR) || defined(USE_OCR_TEST)

#include "ocr_relative_ptr.hpp"
#include "ocr_db_alloc.hpp"
#include "ocr_vector.hpp"

#else

#include <vector>

#endif

namespace TR {

#ifdef USE_OCR_TEST

    template<typename T> using Ptr = Ocr::RelPtr<T>;
    template<typename T, size_t N=256> using Vector = Ocr::VectorN<T, N>;

#   define TR_DELETE(p) /* no-op */
#   define TR_ARRAY_DELETE(p) /* no-op */

#else

    template<typename T> using Ptr = T*;
    template<typename T> using Vector = std::vector<T>;

#   define TR_DELETE(p) delete p
#   define TR_ARRAY_DELETE(p) delete[] p

#endif

};

#endif /* _TEMPEST_REFACTOR_H */
