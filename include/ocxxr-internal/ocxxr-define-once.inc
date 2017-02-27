//===============================================
// OCR main function (entry point)
//===============================================

extern "C" {
ocrGuid_t mainEdt(u32 paramc, u64 /*paramv*/[], u32 depc, ocrEdtDep_t depv[]) {
    assert(paramc == 0 && depc == 1);
    ocxxr::internal::PushTaskState();
    ocxxr::Main(ocxxr::Datablock<ocxxr::MainTaskArgs>(depv[0]));
    ocxxr::internal::PopTaskState();
    return NULL_GUID;
}
}

namespace ocxxr {
namespace internal {

OCXXR_THREAD_LOCAL TaskLocalState *_task_local_state;
#ifdef INSTRUMENT_POINTER_OP
    atomic<u64> rp_indirect_count(0), rp_arrow_count(0), rp_subscript_count(0), rp_cast_count(0), rp_equal_count(0), rp_assign_count(0), rp_negate_count(0);
	atomic<u64> bp_indirect_count(0), bp_arrow_count(0), bp_subscript_count(0), bp_cast_count(0), bp_equal_count(0), bp_assign_count(0), bp_negate_count(0);
    void outputAllCount() {
	    cout << "bp_equal_count " << bp_equal_count.load() << endl;
		cout << "bp_indirect_count " << bp_indirect_count.load() << endl;
		cout << "bp_arrow_count " << bp_arrow_count.load() << endl;
		cout << "bp_subscript_count " << bp_subscript_count.load() << endl;
		cout << "bp_cast_count " << bp_cast_count.load() << endl;
		cout << "bp_assign_count " << bp_assign_count.load() << endl;
		cout << "bp_negate_count " << bp_negate_count.load() << endl;

		cout << "rp_equal_count " << rp_equal_count.load() << endl;
		cout << "rp_indirect_count " << rp_indirect_count.load() << endl;
		cout << "rp_arrow_count " << rp_arrow_count.load() << endl;
		cout << "rp_subscript_count " << rp_subscript_count.load() << endl;
		cout << "rp_cast_count " << rp_cast_count.load() << endl;
		cout << "rp_assign_count " << rp_assign_count.load() << endl;
		cout << "rp_negate_count " << rp_negate_count.load() << endl;

	}
#endif

}  // namespace internal
}  // namespace ocxxr
