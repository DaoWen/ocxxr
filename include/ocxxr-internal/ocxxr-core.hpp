#ifndef OCXXR_CORE_HPP_
#define OCXXR_CORE_HPP_
/// @file

#include <iterator>

namespace ocxxr {

/// @brief Base class for all OCR objects which can carry data.
/// @see DatablockHandle, Event
template <typename T>
class DataHandle : public ObjectHandle {
 protected:
    explicit DataHandle(ocrGuid_t guid) : ObjectHandle(guid) {}
};

static_assert(internal::IsLegalHandle<DataHandle<int>>::value,
              "DataHandle must be castable to/from ocrGuid_t.");

/// @brief Base class for Datablock objects with acquired data pointers.
///
/// A datablock that has been acquired by a task consists
/// of both its handle (GUID), as well as a pointer to
/// the datablock's current base memory address.
/// @see Datablock, Arena
class AcquiredData {};

/// @brief The null placeholder object.
///
/// Represents the absence of an OCR object.
/// Corresponds with the NULL_GUID type in OCR.
///
/// A NullHandle can be automatically converted to any other ObjectHandle type,
/// allowing its use as a "null" placeholder for any OCR object type.
class NullHandle : public ObjectHandle {
 public:
    NullHandle() : ObjectHandle(NULL_GUID) {}

    // auto-convert NullHandle to any ObjectHandle type
    template <typename T, internal::EnableIfBaseOf<ObjectHandle, T> = 0>
    operator T() const {
        static_assert(internal::IsLegalHandle<T>::value,
                      "Only use NullHandle for simple handle types.");
        // Note: this is a bit ugly, but I think it does what the user
        // would expect it to do, i.e., this NullHandle is actually
        // converted to whatever T-type handle they wanted.
        return *reinterpret_cast<T *>(const_cast<NullHandle *>(this));
    }

    // auto-convert NullHandle to Datablock, etc
    template <typename T, internal::EnableIfBaseOf<AcquiredData, T> = 0>
    operator T() const {
        return T(nullptr);
    }
};

static_assert(internal::IsLegalHandle<NullHandle>::value,
              "NullHandle must be castable to/from ocrGuid_t.");

/// Shut down OCR.
inline void Shutdown() { ocrShutdown(); }

/// Abort OCR execution with an error code.
inline void Abort(u8 error_code) { ocrAbort(error_code); }

/// Handle for an OCR datablock object.
template <typename T>
class DatablockHandle : public DataHandle<T> {
 public:
    explicit DatablockHandle(ocrGuid_t guid = NULL_GUID)
            : DataHandle<T>(guid) {}

    DatablockHandle(T **data_ptr, u64 count, const DatablockHint *hint)
            : DataHandle<T>(Init(data_ptr, sizeof(T) * count, true, hint)) {}

    /// @brief Create a datablock, but don't acquire it.
    /// @param[in] count Number of elements of type `T`
    ///                  that this datablock can hold.
    static DatablockHandle<T> Create(u64 count = 1) {
        return DatablockHandle<T>(count);
    }

    /// Destroy this datablock.
    void Destroy() const { internal::OK(ocrDbDestroy(this->guid())); }

 protected:
    explicit DatablockHandle(u64 count)
            : DataHandle<T>(Init(sizeof(T) * count, false, nullptr)) {}

    DatablockHandle(u64 count, const DatablockHint &hint)
            : DataHandle<T>(Init(sizeof(T) * count, false, &hint)) {}

    static ocrGuid_t Init(u64 bytes, const DatablockHint *hint) {
        T **data_ptr;
        return Init(&data_ptr, bytes, false, hint);
    }

    static ocrGuid_t Init(T **data_ptr, u64 bytes, bool acquire,
                          const DatablockHint *hint) {
        ocrGuid_t guid;
        const u16 flags = acquire ? DB_PROP_NONE : DB_PROP_NO_ACQUIRE;
        // TODO - open bug for adding const qualifiers in OCR C API.
        // E.g., "const ocrHint_t *hint" in ocrDbCreate.
        ocrHint_t *raw_hint = const_cast<ocrHint_t *>(hint->internal());
        void **raw_data_ptr = reinterpret_cast<void **>(data_ptr);
        internal::OK(ocrDbCreate(&guid, raw_data_ptr, bytes, flags, raw_hint,
                                 NO_ALLOC));
        if (acquire) {
            internal::bookkeeping::AddDatablock(guid, *raw_data_ptr);
        }
        return guid;
    }
};

static_assert(internal::IsLegalHandle<DatablockHandle<int>>::value,
              "DatablockHandle must be castable to/from ocrGuid_t.");

/// @brief An acquired OCR datablock.
///
/// An acquired datablock consists of both the datablock's global handle
/// and the current base address of its data.
///
/// Datablocks can be automatically converted to the corresponding
/// DatablockHandle type, allowing Datablocks to be used directly
/// to satisfy events or set up task dependencies.
template <typename T>
class Datablock : public AcquiredData {
 public:
    // default constructor: creates null datablock
    explicit Datablock(std::nullptr_t np = nullptr)
            : handle_(NULL_GUID), data_(np) {}

    // this constructor gets called from the task setup code
    explicit Datablock(ocrEdtDep_t dep)
            : handle_(dep.guid), data_(static_cast<T *>(dep.ptr)) {
        internal::bookkeeping::AddDatablock(dep.guid, dep.ptr);
    }

    /// @brief Create and acquire a datablock.
    /// @param[in] count Number of elements of type `T`
    ///                  that this datablock can hold.
    static Datablock<T> Create(u64 count = 1) { return Datablock<T>(count); }

    /// Get a reference to this datablock's internal data.
    template <typename U = T, internal::EnableIfNotVoid<U> = 0>
    U &data() const {
        // The template type U is only here to get enable_if to work.
        static_assert(std::is_same<T, U>::value, "Template types must match.");
        ASSERT(data_ != nullptr);
        return *data_;
    }

    /// Synonym for #data().
    template <typename U = T, internal::EnableIfNotVoid<U> = 0>
    U &operator*() const {
        return data<U>();
    }

    /// Get the datablock's current base address pointer.
    T *data_ptr() const { return data_; }

    /// Shorthand access to members of #data_ptr().
    T *operator->() const { return data_ptr(); }

    /// Null datablock predicate.
    bool is_null() const { return data_ == nullptr; }

    /// Get this datablock's global handle.
    DatablockHandle<T> handle() const { return handle_; }

    /// @brief Release the datablock.
    ///
    /// The datablock release operation is a key concept
    /// in the OCR memory model. A datablock must be released
    /// in order to guarantee that any previous writes to the datablock
    /// are made visible before any subsequent synchronization operations.
    void Release() const {
        internal::OK(ocrDbRelease(handle_.guid()));
        internal::bookkeeping::RemoveDatablock(handle_.guid());
    }

    // automatic type conversion to DatablockHandle
    operator DatablockHandle<T>() const { return handle_; }

 private:
    explicit Datablock(u64 count) : Datablock(nullptr, count, nullptr) {}

    Datablock(u64 count, const DatablockHint &hint)
            : Datablock(nullptr, count, &hint) {}

    Datablock(T *tmp, u64 count, const DatablockHint *hint)
            : handle_(&tmp, count, hint), data_(tmp) {}

    const DatablockHandle<T> handle_;
    T *const data_;
};

template <typename T>
class DataHandleList {
    // FIXME - implement!
};

template <typename T>
class DatablockList : public AcquiredData {
    // FIXME - implement!
};

struct Properties {
    static constexpr u16 kLabeled = GUID_PROP_IS_LABELED;
    static constexpr u16 kChecked = GUID_PROP_IS_LABELED | GUID_PROP_CHECK;
    static constexpr u16 kBlocking = GUID_PROP_BLOCK;
};

/// @brief Base class for OCR events.
///
/// To create an event, use one of the derived event sub-types
/// (e.g., OnceEvent or StickyEvent) rather than this abstract type.
/// However, when an event is stored in a struct, or passed as an argument,
/// the specific type of the event is often not needed, and this abstract
/// Event type can be used instead.
template <typename T>
class Event : public DataHandle<T> {
 public:
    explicit Event(ocrGuid_t guid = NULL_GUID) : DataHandle<T>(guid) {}

    /// Destroy this event.
    void Destroy() const { internal::OK(ocrEventDestroy(this->guid())); }

    /// @brief Satisfy this event (with nothing).
    ///
    /// Satisfy the event, passing a null handle (NULL_GUID) as the payload.
    /// This function is useful for control-only events (i.e., Event<void>).
    void Satisfy() const {
        internal::OK(ocrEventSatisfy(this->guid(), NULL_GUID));
    }

    /// @brief Satisfy this event with a datablock.
    ///
    /// @param[in] src Handle for the datablock used to satisfy the event.
    ///
    /// Note: To chain two events together, use #AddDependence instead.
    void Satisfy(DatablockHandle<T> data) const {
        internal::OK(ocrEventSatisfy(this->guid(), data.guid()));
    }

    /// @brief Set up a dependence from this event to a data source.
    void DependOn(DataHandle<T> src) const {
        constexpr u32 slot = 0;
        constexpr ocrDbAccessMode_t mode = DB_DEFAULT_MODE;
        internal::OK(ocrAddDependence(src.guid(), this->guid(), slot, mode));
    }

 protected:
    Event(ocrEventTypes_t type, u16 flags, Event self)
            : DataHandle<T>(Init(type, flags, nullptr, self)) {}

    Event(ocrEventTypes_t type, u16 flags, ocrEventParams_t params,
          Event self = NullHandle())
            : DataHandle<T>(Init(type, flags, &params, self)) {}

 private:
    static constexpr bool kIsVoid = std::is_same<void, T>::value;
    static constexpr u16 kDefaultFlags =
            kIsVoid ? EVT_PROP_NONE : EVT_PROP_TAKES_ARG;

    static ocrGuid_t Init(ocrEventTypes_t type, u16 flags,
                          ocrEventParams_t *params, Event self) {
        ocrGuid_t guid = self.guid();
        ASSERT(self.is_null() == !(flags & Properties::kLabeled) &&
               "Provide self handle iff this is labeled event.");
        flags |= kDefaultFlags;  // Add data mode to flags
        if (params) {
            internal::OK(ocrEventCreateParams(&guid, type, flags, params));
        } else {
            internal::OK(ocrEventCreate(&guid, type, flags));
        }
        return guid;
    }
};

static_assert(internal::IsLegalHandle<Event<int>>::value,
              "Event must be castable to/from ocrGuid_t.");

template <typename T>
class OnceEvent : public Event<T> {
 public:
    /// Create a "once"-type event.
    static OnceEvent Create(u16 flags = 0, Event<T> self = NullHandle()) {
        return OnceEvent(flags, self);
    }

 private:
    OnceEvent(u16 flags, Event<T> self)
            : Event<T>(OCR_EVENT_ONCE_T, flags, self) {}
};

static_assert(internal::IsLegalHandle<OnceEvent<int>>::value,
              "OnceEvent must be castable to/from ocrGuid_t.");

template <typename T>
class IdempotentEvent : public Event<T> {
 public:
    /// Create an "idempotent"-type event.
    static IdempotentEvent Create(u16 flags = 0, Event<T> self = NullHandle()) {
        return IdempotentEvent(flags, self);
    }

 private:
    IdempotentEvent(u16 flags, Event<T> self)
            : Event<T>(OCR_EVENT_IDEM_T, flags, self) {}
};

static_assert(internal::IsLegalHandle<IdempotentEvent<int>>::value,
              "IdempotentEvent must be castable to/from ocrGuid_t.");

template <typename T>
class StickyEvent : public Event<T> {
 public:
    /// Create a "sticky"-type event.
    static StickyEvent Create(u16 flags = 0, Event<T> self = NullHandle()) {
        return StickyEvent(flags, self);
    }

 private:
    StickyEvent(u16 flags, Event<T> self)
            : Event<T>(OCR_EVENT_STICKY_T, flags, self) {}
};

static_assert(internal::IsLegalHandle<StickyEvent<int>>::value,
              "StickyEvent must be castable to/from ocrGuid_t.");

template <typename T>
class LatchEvent : public Event<T> {
 public:
    /// Create a "latch"-type event.
    static LatchEvent Create(u16 flags = 0, Event<T> self = NullHandle()) {
        return LatchEvent(flags, self);
    }

    /// @brief Create a "latch"-type event.
    ///
    /// @param[in] up_count Initial increment count of the latch.
    static LatchEvent Create(u64 up_count, u16 flags = 0,
                             Event<T> self = NullHandle()) {
        return LatchEvent(up_count, flags, self);
    }

    /// Increase increment count of this latch by 1.
    void Up() {
        internal::OK(ocrEventSatisfySlot(this->guid(), NULL_GUID,
                                         OCR_EVENT_LATCH_INCR_SLOT));
    }

    /// Increase decrement count of this latch by 1.
    void Down() {
        internal::OK(ocrEventSatisfySlot(this->guid(), NULL_GUID,
                                         OCR_EVENT_LATCH_DECR_SLOT));
    }

 private:
    LatchEvent(u16 flags, Event<T> self)
            : Event<T>(OCR_EVENT_LATCH_T, flags, self) {}

    LatchEvent(u64 up_count, u16 flags, Event<T> self)
            : Event<T>(OCR_EVENT_LATCH_T, flags, MakeParams(up_count), self) {}

    static ocrEventParams_t MakeParams(u64 count) {
        ocrEventParams_t params;
        params.EVENT_LATCH.counter = count;
        return params;
    }
};

static_assert(internal::IsLegalHandle<LatchEvent<int>>::value,
              "LatchEvent must be castable to/from ocrGuid_t.");

/// @brief The dependence-placeholder handle.
///
/// Used only for placing "holes" in a task's dependence list.
/// @see ObjectHandle#is_uninitialized
template <typename T>
class UnknownDependence : public DataHandle<T> {
 public:
    /// Create a dependence-placeholder handle.
    UnknownDependence() : DataHandle<T>(UNINITIALIZED_GUID) {}
};

static_assert(internal::IsLegalHandle<UnknownDependence<int>>::value,
              "UnknownDependence must be castable to/from ocrGuid_t.");

namespace internal {

template <typename T>
struct Unpack {
    static_assert(std::is_convertible<T, AcquiredData>::value,
                  "Expected an OCR data container type.");
};

template <template <typename> class T, typename U>
struct Unpack<T<U>> {
    typedef U Parameter;
    static_assert(std::is_convertible<T<U>, DataHandle<U>>::value,
                  "Expected an OCR data container type.");
};
template <>
struct Unpack<NullHandle> {
    typedef void Parameter;
};
template <>
struct Unpack<void> {
    typedef void Parameter;
};

template <typename AllArgs, typename... ArgsAcc>
struct VarArgsInfoHelp;

template <typename T, typename... ArgsAcc>
struct VarArgsInfoHelp<void(DatablockList<T>), ArgsAcc...> {
    static constexpr bool kHasVarArgs = true;
    typedef void(DepsFn)(ArgsAcc...);
    typedef void(VarArgsFn)(DatablockList<T>);
    typedef void(VarArgsHandleFn)(DataHandleList<T>);
};

template <typename... ArgsAcc>
struct VarArgsInfoHelp<void(), ArgsAcc...> {
    static constexpr bool kHasVarArgs = false;
    typedef void(DepsFn)(ArgsAcc...);
    typedef void(VarArgsFn)();
    typedef void(VarArgsHandleFn)();
};

template <typename Arg, typename... AllArgs, typename... ArgsAcc>
struct VarArgsInfoHelp<void(Arg, AllArgs...), ArgsAcc...> {
 protected:
    // this checks that Arg is a valid dependence argument
    typedef typename Unpack<Arg>::Parameter P;
    typedef VarArgsInfoHelp<void(AllArgs...), ArgsAcc..., Arg> Recur;

 public:
    static constexpr bool kHasVarArgs = Recur::kHasVarArgs;
    typedef typename Recur::DepsFn DepsFn;
    typedef typename Recur::VarArgsFn VarArgsFn;
    typedef typename Recur::VarArgsHandleFn VarArgsHandleFn;
};

template <bool has_param, typename T, typename... As>
struct ParamInfoHelp : public VarArgsInfoHelp<void(As...)> {
    static constexpr bool kHasParam = true;
    typedef void(ParamFn)(T);
    typedef T Type;
    typedef typename std::remove_reference<Type>::type RawType;
    static constexpr size_t kParamBytes = sizeof(RawType);
    static constexpr size_t kParamWordCount =
            (kParamBytes + sizeof(u64) - 1) / sizeof(u64);
    // This is going to get memcpy'd
    static_assert(IsTriviallyCopyable<RawType>::value,
                  "Task parameter must be trivially copyable.");
};

template <typename T, typename... As>
struct ParamInfoHelp<false, T, As...> : public VarArgsInfoHelp<void(T, As...)> {
    static constexpr bool kHasParam = false;
    typedef void Type;
    typedef void RawType;
    typedef void(ParamFn)();
    static constexpr size_t kParamBytes = 0;
    static constexpr size_t kParamWordCount = 0;
};

template <typename T>
struct ParamInfo;

// Case: nullary
template <typename R>
struct ParamInfo<R()> : public VarArgsInfoHelp<void()> {
    typedef void Type;
    static constexpr bool kHasParam = false;
    typedef void(ParamFn)();
    typedef void RawType;
    static constexpr size_t kParamBytes = 0;
    static constexpr size_t kParamWordCount = 0;
};

template <typename R, typename T, typename... As>
struct ParamInfo<R(T, As...)>
        : public ParamInfoHelp<!IsBaseOf<AcquiredData, T>, T, As...> {};

template <typename T>
struct FnInfo;

template <typename R, typename... As>
struct FnInfo<R(As...)> {
    static constexpr size_t kTotalArgCount = sizeof...(As);
    typedef R(Fn)(As...);
    typedef std::tuple<As...> Args;
    typedef R Result;
    template <size_t I>
    struct Arg {
        typedef typename std::tuple_element<I, Args>::type Type;
    };
    static constexpr bool kHasParam = ParamInfo<Fn>::kHasParam;
    static constexpr size_t kParamCount = kHasParam ? 1 : 0;
    static constexpr size_t kDepStart = kParamCount;
    static constexpr size_t kDepCount = sizeof...(As)-kDepStart;
    typedef typename ParamInfo<Fn>::ParamFn ParamFn;
    typedef typename ParamInfo<Fn>::DepsFn DepsFn;
};

template <typename F>
using ReturnTypeParameter =
        typename Unpack<typename FnInfo<F>::Result>::Parameter;

// XXX - all non-type template arguments should use variable naming style
// using short here because we shouldn't have that many arguments!
template <short arg_count, short param_count>
struct CountMissingDeps {
    static_assert(
            arg_count <= param_count,
            "Dependence argument count must not exceed task parameter count.");
    static constexpr short diff = param_count - arg_count;
    static constexpr size_t value = diff > 0 ? diff : 0;
    static decltype(MakeIndexSeq<value>()) indices() {
        return MakeIndexSeq<value>();
    }
};

template <typename D, typename A, size_t Position>
struct TaskArgTypeMatchesParamType {
    typedef Datablock<typename Unpack<D>::Parameter> DepDH;
    typedef Datablock<typename Unpack<A>::Parameter> ArgDH;
    static_assert(std::is_base_of<ArgDH, DepDH>::value,
                  "Dependence argument type must match task parameter type.");
    static constexpr bool value = true;
};

template <typename T, T *t, typename U, typename V>
class TaskImplementation;

// TODO - add static check to make sure Args have Datablock types
template <typename F, F *user_fn, typename... Params, typename... Args>
class TaskImplementation<F, user_fn, void(Params...), void(Args...)> {
 public:
    static constexpr size_t kDepc = internal::FnInfo<F>::kDepCount;
    static constexpr size_t kParamc = internal::FnInfo<F>::kParamCount;
    typedef typename internal::FnInfo<F>::Result R;

    static_assert(std::is_same<F, R(Params..., Args...)>::value,
                  "Task function must have a consistent type.");

    static ocrGuid_t InternalFn(u32 paramc, u64 paramv[], u32 depc,
                                ocrEdtDep_t depv[]) {
        ASSERT(paramc == internal::ParamInfo<F>::kParamWordCount);
        ASSERT(depc == kDepc);
        PushTaskState();
        ocrGuid_t result = Launch(paramv, depv);
        PopTaskState();
        return result;
    }

 private:
    template <typename U = R, EnableIfVoid<U> = 0>
    static ocrGuid_t Launch(u64 paramv[], ocrEdtDep_t depv[]) {
        UnpackArgs(paramv, depv, internal::MakeIndexSeq<kParamc>(),
                   internal::MakeIndexSeq<kDepc>());
        return NULL_GUID;
    }

    template <typename U = R, EnableIfNotVoid<U> = 0>
    static ocrGuid_t Launch(u64 paramv[], ocrEdtDep_t depv[]) {
        return UnpackArgs(paramv, depv, internal::MakeIndexSeq<kParamc>(),
                          internal::MakeIndexSeq<kDepc>())
                .guid();
    }

    template <typename T, typename U = typename std::remove_reference<T>::type>
    static U *UnpackParam(u64 *param) {
        return reinterpret_cast<U *>(param);
    }

    template <size_t... I, size_t... J>
    static R UnpackArgs(u64 paramv[], ocrEdtDep_t depv[],
                        internal::IndexSeq<I...>, internal::IndexSeq<J...>) {
        static_cast<void>(paramv);  // unused if no parameters
        static_cast<void>(depv);    // unused if no deps
        return user_fn((*UnpackParam<Params>(&paramv[I]))...,
                       (Args{depv[J]})...);
    }
};

class DefaultDependence : public ObjectHandle {
 public:
    DefaultDependence() : ObjectHandle(UNINITIALIZED_GUID) {}

    explicit DefaultDependence(size_t) : DefaultDependence() {}

    template <typename T>
    operator DataHandle<T>() const {
        return UnknownDependence<T>();
    }
};

static_assert(internal::IsLegalHandle<DefaultDependence>::value,
              "DefaultDependence must be castable to/from ocrGuid_t.");

}  // namespace internal

/// @cond DOXYGEN_IGNORE

// TODO - should this be internal?
template <typename T>
using DataHandleOf = DataHandle<typename internal::Unpack<T>::Parameter>;

template <typename F>
class TaskTemplate;

template <typename T, typename U, typename V>
class TaskBuilder;

template <typename F>
class Task;

/// @endcond

/// Datablock access modes
struct AccessMode {
    static constexpr ocrDbAccessMode_t kDefault = DB_DEFAULT_MODE;
    static constexpr ocrDbAccessMode_t kExclusive = DB_MODE_EW;
    static constexpr ocrDbAccessMode_t kConstant = DB_MODE_CONST;
    static constexpr ocrDbAccessMode_t kReadWrite = DB_MODE_RW;
    static constexpr ocrDbAccessMode_t kReadOnly = DB_MODE_RO;
};

template <typename Ret, typename... Args>
class Task<Ret(Args...)> : public ObjectHandle {
 public:
    typedef Ret(F)(Args...);
    typedef typename internal::Unpack<Ret>::Parameter R;

    static constexpr size_t kDepc = internal::FnInfo<F>::kDepCount;
    static constexpr size_t kParamc = internal::FnInfo<F>::kParamCount;

    void Destroy() const { internal::OK(ocrEdtDestroy(this->guid())); }

    /// @brief Set up a dependence from one of this task's input slots to a data
    /// source.
    ///
    /// @tparam slot Index of the slot for the dependency.
    /// @param[in] src Data source for the dependency.
    /// @param[in] mode Access mode requested for the input data.
    template <u32 slot, typename U>
    const Task<F> &DependOn(
            U src, ocrDbAccessMode_t mode = AccessMode::kDefault) const {
        namespace i = internal;
        static_assert(slot < kDepc, "Slot too high.");
        constexpr u32 dep_slot = slot + i::FnInfo<F>::kDepStart;
        using Expected = typename i::FnInfo<F>::template Arg<dep_slot>::Type;
        static_assert(i::TaskArgTypeMatchesParamType<Expected, U, slot>::value,
                      "Dependence argument must match slot type.");
        ocrGuid_t src_guid = static_cast<DataHandleOf<U>>(src).guid();
        internal::OK(ocrAddDependence(src_guid, this->guid(), slot, mode));
        return *this;
    }

    /// @brief Set up dependences from a range of this task's input slots to a
    /// range of data sources.
    ///
    /// @tparam start_slot Index of the slot for the first dependency.
    /// @tparam until_slot Index one past the slot for the last dependency.
    /// @param[in] src_start Iterator-like object of the data source for the
    ///                      first dependency.
    /// @param[in] mode Access mode requested for the input data.
    template <u32 start_slot, u32 until_slot, typename U>
    const Task<F> &DependOnRange(
            U src_start, ocrDbAccessMode_t mode = AccessMode::kDefault) const {
        DependOnRangeHelper<start_slot, until_slot, Task<F>, U>::Recur(
                this, src_start, mode);
        return *this;
    }

 protected:
    // TODO - paramv support (check if first arg of F isn't a Datablock)
    template <typename T, typename U, typename V>
    friend class TaskBuilder;

    // TODO - add support for hints, output events, etc
    Task(Event<R> *out_event, ocrGuid_t task_template, u64 paramv[],
         ocrGuid_t depv[], const TaskHint *hint, u16 flags)
            : ObjectHandle(Init(out_event, task_template, paramv, depv, hint,
                                flags)) {}

 private:
    static ocrGuid_t Init(Event<R> *out_event, ocrGuid_t task_template,
                          u64 paramv[], ocrGuid_t depv[], const TaskHint *hint,
                          u16 flags) {
        ocrGuid_t guid;
        ocrGuid_t *out_guid = reinterpret_cast<ocrGuid_t *>(out_event);
        ASSERT(paramv != nullptr || kParamc == 0);
        // TODO - open bug for adding const qualifiers in OCR C API.
        // E.g., "const ocrHint_t *hint" in ocrEdtCreate.
        ocrHint_t *raw_hint = const_cast<ocrHint_t *>(hint->internal());
        ocrEdtCreate(&guid, task_template, EDT_PARAM_DEF, paramv, EDT_PARAM_DEF,
                     depv, flags, raw_hint, out_guid);
        return guid;
    }

    template <u32 start_slot, u32 until_slot, typename T, typename U,
              bool stop = start_slot >= until_slot>
    struct DependOnRangeHelper {
        static void Recur(const T *task, U src_start, ocrDbAccessMode_t mode) {
            task->template DependOn<start_slot>(*src_start, mode);
            auto src_next = std::next(src_start);
            DependOnRangeHelper<start_slot + 1, until_slot, T, U>::Recur(
                    task, src_next, mode);
        }
    };

    template <u32 start_slot, u32 until_slot, typename T, typename U>
    struct DependOnRangeHelper<start_slot, until_slot, T, U, true> {
        static void Recur(const T *, U, ocrDbAccessMode_t) {}
    };
};

template <typename F>
class DelayedFuture {
 public:
    typedef internal::ReturnTypeParameter<F> R;

    DelayedFuture(Task<F> task, Event<R> event) : task_(task), event_(event) {}

    Task<F> task() const { return task_; }

    Event<R> event() const { return event_; }

 private:
    const Task<F> task_;
    const Event<R> event_;
};

template <typename F, typename... Params, typename... Args>
class TaskBuilder<F, void(Params...), void(Args...)> {
 public:
    typedef typename internal::FnInfo<F>::Result Ret;
    static_assert(std::is_same<F, Ret(Params..., Args...)>::value,
                  "Task function must have a consistent type.");
    typedef typename internal::Unpack<Ret>::Parameter R;

    TaskBuilder(ocrGuid_t template_guid, const TaskHint *hint, u16 flags)
            : template_guid_(template_guid), hint_(hint), flags_(flags) {}

    Task<F> CreateTask(Params... params, DataHandleOf<Args>... deps) {
        return HelpCreateTask<Args...>(nullptr, params..., deps...);
    }

    template <typename... Deps,
              internal::EnableIf<sizeof...(Deps) != sizeof...(Args)> = 0>
    Task<F> CreateTaskPartial(Params... params, Deps... deps) {
        if (sizeof...(Deps) == 0) {
            return HelpCreateTask<>(nullptr, params...);
        } else {
            auto missing_indices =
                    internal::CountMissingDeps<sizeof...(Deps),
                                               sizeof...(Args)>::indices();
            return PadTask(missing_indices, params..., deps...);
        }
    }

    template <typename... Deps,
              internal::EnableIf<sizeof...(Deps) != sizeof...(Args)> = 0>
    DelayedFuture<F> CreateFuturePartial(Params... params, Deps... deps) {
        auto missing_indices =
                internal::CountMissingDeps<sizeof...(Deps),
                                           sizeof...(Args)>::indices();
        return PadFuture(missing_indices, params..., deps...);
    }

 private:
    template <typename... Deps>
    Task<F> HelpCreateTask(Event<R> *out_event, Params... params,
                           DataHandleOf<Deps>... deps) {
        static_assert(
                sizeof...(Deps) == sizeof...(Args) || sizeof...(Deps) == 0,
                "Must either provide all dependence args or none.");
        ASSERT(flags_ != EDT_PROP_FINISH &&
               "Created Finish-type EDT, but not using the output event.");
        // Set params (if any)
        u64 *param_ptr[1 + Task<F>::kParamc] = {
                reinterpret_cast<u64 *>(&params)..., nullptr};
        // Set provided dependences
        ocrGuid_t depv[1 + Task<F>::kDepc] = {
                (static_cast<DataHandleOf<Deps>>(deps).guid())..., NULL_GUID};
        ocrGuid_t *depv_ptr = sizeof...(Deps) ? depv : nullptr;
        // Create the task
        return Task<F>(out_event, template_guid_, param_ptr[0], depv_ptr, hint_,
                       flags_);
    }

    template <size_t... I, typename... Deps>
    Task<F> PadTask(internal::IndexSeq<I...>, Params... params, Deps... deps) {
        static_assert(sizeof...(I) + sizeof...(Deps) == sizeof...(Args),
                      "Correct total number of arguments for task.");
        return CreateTask(params..., deps...,
                          internal::DefaultDependence(I)...);
    }

    template <size_t... I, typename... Deps>
    DelayedFuture<F> PadFuture(internal::IndexSeq<I...>, Params... params,
                               Deps... deps) {
        static_assert(sizeof...(I) + sizeof...(Deps) == sizeof...(Args),
                      "Correct total number of arguments for task.");
        return CreateFuture<Args...>(params..., deps...,
                                     internal::DefaultDependence(I)...);
    }

    // This is private because the output event is useless if all deps are
    // provided up-front (due to the data race with the corresponding task).
    template <typename... Deps>
    DelayedFuture<F> CreateFuture(Params... params,
                                  DataHandleOf<Deps>... deps) {
        Event<R> out_event;
        auto task = HelpCreateTask<Deps...>(&out_event, params..., deps...);
        return DelayedFuture<F>(task, out_event);
    }

    const ocrGuid_t template_guid_;
    const TaskHint *const hint_;
    const u16 flags_;
};

/// @brief Task template.
/// @see #OCXXR_TEMPLATE_FOR(fn_ptr)
template <typename F>
class TaskTemplate : public ObjectHandle {
 public:
    typedef typename internal::FnInfo<F>::ParamFn PF;
    typedef typename internal::FnInfo<F>::DepsFn DF;

    // TODO - ensure that function's parameters are Datablocks
    // TODO - ensure that there is only one by-value parameter
    // (and update badTask2Params with the static assert message)
    /// @brief Create a task template.
    /// The macro #OCXXR_TEMPLATE_FOR(fn_ptr) is provided as a more
    /// convenient way of invoking this function. (Invoking this function
    /// directly is a bit verbose due to the complex template parameters.)
    template <F *user_fn>
    static TaskTemplate<F> Create() {
        ocrGuid_t guid;
        ocrEdt_t internal_fn =
                internal::TaskImplementation<F, user_fn, PF, DF>::InternalFn;
        constexpr u16 depc = internal::FnInfo<F>::kDepCount;
        constexpr u16 paramc = internal::ParamInfo<F>::kParamWordCount;
        ocrEdtTemplateCreate(&guid, internal_fn, paramc, depc);
        return TaskTemplate<F>(guid);
    }

    TaskBuilder<F, PF, DF> operator()(u16 flags = EDT_PROP_NONE) const {
        return TaskBuilder<F, PF, DF>(this->guid(), nullptr, flags);
    }

    TaskBuilder<F, PF, DF> operator()(const TaskHint &hint,
                                      u16 flags = EDT_PROP_NONE) const {
        return TaskBuilder<F, PF, DF>(this->guid(), &hint, flags);
    }

    void Destroy() const { internal::OK(ocrEdtTemplateDestroy(this->guid())); }

 private:
    explicit TaskTemplate(ocrGuid_t guid) : ObjectHandle(guid) {}

    typedef typename internal::FnInfo<F>::Result Result;
    typedef typename internal::Unpack<Result>::Parameter Parameter;
    static_assert(
            std::is_same<void, Result>::value ||
                    std::is_same<NullHandle, Result>::value ||
                    std::is_same<DatablockHandle<Parameter>, Result>::value,
            "User's task function must return void, NullHandle, or "
            "DatablockHandle<?>.");
};

namespace internal {

typedef DatablockHandle<void>(DummyTaskFnType)(Datablock<int>,
                                               Datablock<double>);

typedef Task<DummyTaskFnType> DummyTaskType;

typedef TaskTemplate<DummyTaskFnType> DummyTemplateType;

static_assert(IsLegalHandle<DummyTaskType>::value,
              "Task must be castable to/from ocrGuid_t.");

static_assert(IsLegalHandle<DummyTemplateType>::value,
              "TaskTemplate must be castable to/from ocrGuid_t.");

}  // namespace internal

/// @brief Main task's argument structure
/// Wrapper class for datablock argument of the mainEdt.
class MainTaskArgs {
 public:
    u64 argc() { return getArgc(this); }
    char *argv(u64 index) { return getArgv(this, index); }
};

/// Prototype for user's Main task function
void Main(Datablock<MainTaskArgs> args);

}  // namespace ocxxr

#endif  // OCXXR_CORE_HPP_
