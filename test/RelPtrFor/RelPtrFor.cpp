#include <ocxxr-main.hpp>

void ocxxr::Main(ocxxr::Datablock<ocxxr::MainTaskArgs>) {
    constexpr int kPayload = 7;
    int x = kPayload;
    ocxxr::RelPtrFor<decltype(&x)> p1 = &x;
    ocxxr::RelPtrFor<int**> p2 = &p1;
    ocxxr::RelPtrFor<int***> p3 = &p2;
    ocxxr::RelPtrFor<decltype(&p3)> p4 = &p3;
    PRINTF("x = %d\n", ****p4);
    ASSERT(****p4 == kPayload);
    ocxxr::Shutdown();
}
