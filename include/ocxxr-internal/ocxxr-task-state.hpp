#ifndef OCXXR_TASK_STATE_HPP_
#define OCXXR_TASK_STATE_HPP_

#ifdef __APPLE__
// For some reason they don't support the standard C++ thread_local,
// but they *do* support the non-standard __thread extension for C.
#define OCXXR_THREAD_LOCAL __thread
#else
#define OCXXR_THREAD_LOCAL thread_local
#endif /* __APPLE__ */

namespace ocxxr {
namespace internal {

//===============================================
// Task-local state
//===============================================

struct TaskLocalState {
    dballoc::DatablockAllocator arena_allocator;
    TaskLocalState *parent;
};

extern OCXXR_THREAD_LOCAL TaskLocalState *_task_local_state;

/* Note: The push/pop task state functions are currently necessary because the
 * Traleika Glacier OCR implementation uses a "work-shifting" strategy to try
 * to do some useful work while an EDT is blocked. This behavior is triggered
 * by legacy blocking constructs, as well as for remote operations in x86-mpi.
 */

inline void PushTaskState() {
    TaskLocalState *parent_state = _task_local_state;
    // FIXME - use a portable "task-local-alloc" function in place of "new"
    _task_local_state = new TaskLocalState{};
    _task_local_state->parent = parent_state;
}

inline void PopTaskState() {
    TaskLocalState *child_state = _task_local_state;
    _task_local_state = _task_local_state->parent;
    // FIXME - use a portable function in place of "delete"
    delete child_state;
}

//===============================================
// Arena support
//===============================================

namespace dballoc {

/// Get the current implicit arena allocator
inline DatablockAllocator &AllocatorGet(void) {
    return _task_local_state->arena_allocator;
}

/// Set the implicit arena allocator
inline void AllocatorSetDb(void *dbPtr) {
    new (&_task_local_state->arena_allocator) DatablockAllocator(dbPtr);
}

}  // namespace dballoc
}  // namespace internal
}  // namespace ocxxr

#endif  // OCXXR_TASK_STATE_HPP_
