#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>

#include <atomic>
#include <exception>
#include <system_error>
#include <typeinfo>

#include "platform/platform.hpp"
#include "utils/microfmt.hpp"
#include "utils/utils.hpp"
#include "logging.hpp"

#ifdef _MSC_VER
 #include <ehdata.h>
#else
 #include <string.h>
 #if IS_WINDOWS
  #ifndef WIN32_LEAN_AND_MEAN
   #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
 #else
  #include <sys/mman.h>
  #include <unistd.h>
  #if IS_APPLE
   #include <mach/mach.h>
   #ifdef CPPTRACE_HAS_MACH_VM
    #include <mach/mach_vm.h>
   #endif
  #else
   #include <fstream>
   #include <ios>
  #endif
 #endif
#endif

namespace cpptrace {
namespace internal {
    thread_local detail::lazy_trace_holder current_exception_trace;
    thread_local detail::lazy_trace_holder saved_rethrow_trace;

    bool& get_rethrow_switch() {
        static thread_local bool rethrow_switch = false;
        return rethrow_switch;
    }

    CPPTRACE_FORCE_NO_INLINE void collect_current_trace(std::size_t skip) {
        microfmt::print("collect_current_trace\n");
        auto trace = cpptrace::generate_raw_trace(skip + 1);
        if(internal::get_rethrow_switch()) {
            internal::saved_rethrow_trace = detail::lazy_trace_holder(std::move(trace));
        } else {
            internal::current_exception_trace = detail::lazy_trace_holder(std::move(trace));
            internal::saved_rethrow_trace = detail::lazy_trace_holder();
        }
    }

    #ifdef _MSC_VER
    // https://www.youtube.com/watch?v=COEv2kq_Ht8
    // https://github.com/tpn/pdfs/blob/master/2018%20CppCon%20Unwinding%20the%20Stack%20-%20Exploring%20how%20C%2B%2B%20Exceptions%20work%20on%20Windows%20-%20James%20McNellis.pdf
    // https://github.com/ecatmur/stacktrace-from-exception/blob/main/stacktrace-from-exception.cpp
    // https://github.com/wine-mirror/wine/blob/7f833db11ffea4f3f4fa07be31d30559aff9c5fb/dlls/msvcrt/except.c#L371
    using catchable_type_array_t = decltype(ThrowInfo::pCatchableTypeArray);

    class catchable_type_info {
        HMODULE module_pointer = nullptr;
        const _CatchableTypeArray* catchable_types = nullptr;
    public:
        catchable_type_info(const HMODULE module_pointer, catchable_type_array_t catchable_type_array)
            : module_pointer(module_pointer) {
            catchable_types = rtti_rva<const _CatchableTypeArray*>(catchable_type_array);
        }

        class iterator {
            const catchable_type_info& info;
            std::size_t i;
        public:
            iterator(const catchable_type_info& info, std::size_t i) : info(info), i(i) {}
            const std::type_info& operator*() const {
                return info.get_type_info(i);
            }
            bool operator!=(const iterator& other) const {
                return i != other.i;
            }
            iterator& operator++() {
                i++;
                return *this;
            }
        };
        using const_iterator = iterator;

        const_iterator begin() const {
            return {*this, 0};
        }
        const_iterator end() const {
            return {*this, catchable_types ? catchable_types->nCatchableTypes : 0};
        }

    private:
        template<typename T, typename A>
        T rtti_rva(A address) const {
            #ifdef _WIN64
             return reinterpret_cast<T>((uintptr_t)module_pointer + (uintptr_t)address);
            #else
             return reinterpret_cast<T>(address);
            #endif
        }

        const std::type_info& get_type_info(std::size_t i) const {
            return *rtti_rva<const std::type_info*>(get_catchable_type(i)->pType);
        }

        const CatchableType* get_catchable_type(std::size_t i) const {
            return rtti_rva<const CatchableType*>(
                reinterpret_cast<const std::int32_t*>(catchable_types->arrayOfCatchableTypes)[i]
            );
        }
    };

    catchable_type_info get_catchable_types(const EXCEPTION_RECORD* exception_record) {
        static_assert(EXCEPTION_MAXIMUM_PARAMETERS >= 4);
        // ExceptionInformation will contain
        // [0] EH_MAGIC_NUMBER1
        // [1] ExceptionObject
        // [2] ThrowInfo
        HMODULE module_pointer = nullptr;
        catchable_type_array_t catchable_type_array{}; // will be either an int or pointer
        if(
            exception_record->ExceptionInformation[0] == EH_MAGIC_NUMBER1
            && exception_record->NumberParameters >= 3
        ) {
            if(exception_record->NumberParameters >= 4) {
                module_pointer = reinterpret_cast<HMODULE>(exception_record->ExceptionInformation[3]);
            }
            auto throw_info = reinterpret_cast<const ThrowInfo*>(exception_record->ExceptionInformation[2]);
            if (throw_info) {
                catchable_type_array = throw_info->pCatchableTypeArray;
            }
        }
        return {module_pointer, catchable_type_array};
    }

    bool matches_exception(EXCEPTION_RECORD* exception_record, const std::type_info& type_info) {
        if (type_info == typeid(void)) {
            return true;
        }
        for (const auto& catchable_type : get_catchable_types(exception_record)) {
            if (catchable_type == type_info) {
                return true;
            }
        }
        return false;
    }
    #endif

    #ifndef _MSC_VER
    #if IS_LIBSTDCXX
        constexpr size_t vtable_size = 11;
    #elif IS_LIBCXX
        constexpr size_t vtable_size = 10;
    #else
        #warning "Cpptrace from_current: Unrecognized C++ standard library, from_current() won't be supported"
        constexpr size_t vtable_size = 0;
    #endif

    #if IS_WINDOWS
    int get_page_size() {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        return info.dwPageSize;
    }
    constexpr auto memory_readonly = PAGE_READONLY;
    constexpr auto memory_readwrite = PAGE_READWRITE;
    int mprotect_page_and_return_old_protections(void* page, int page_size, int protections) {
        DWORD old_protections;
        if(!VirtualProtect(page, page_size, protections, &old_protections)) {
            throw std::runtime_error(
                microfmt::format(
                    "VirtualProtect call failed: {}",
                    std::system_error(GetLastError(), std::system_category()).what()
                )
            );
        }
        return old_protections;
    }
    void mprotect_page(void* page, int page_size, int protections) {
        mprotect_page_and_return_old_protections(page, page_size, protections);
    }
    void* allocate_page(int page_size) {
        auto page = VirtualAlloc(nullptr, page_size, MEM_COMMIT | MEM_RESERVE, memory_readwrite);
        if(!page) {
            throw std::runtime_error(
                microfmt::format(
                    "VirtualAlloc call failed: {}",
                    std::system_error(GetLastError(), std::system_category()).what()
                )
            );
        }
        return page;
    }
    #else
    int get_page_size() {
        #if defined(_SC_PAGESIZE)
            return sysconf(_SC_PAGESIZE);
        #else
            return getpagesize();
        #endif
    }
    constexpr auto memory_readonly = PROT_READ;
    constexpr auto memory_readwrite = PROT_READ | PROT_WRITE;
    #if IS_APPLE
    int get_page_protections(void* page) {
        // https://stackoverflow.com/a/12627784/15675011
        #ifdef CPPTRACE_HAS_MACH_VM
        mach_vm_size_t vmsize;
        mach_vm_address_t address = (mach_vm_address_t)page;
        #else
        vm_size_t vmsize;
        vm_address_t address = (vm_address_t)page;
        #endif
        vm_region_basic_info_data_t info;
        mach_msg_type_number_t info_count =
            sizeof(size_t) == 8 ? VM_REGION_BASIC_INFO_COUNT_64 : VM_REGION_BASIC_INFO_COUNT;
        memory_object_name_t object;
        kern_return_t status =
        #ifdef CPPTRACE_HAS_MACH_VM
        mach_vm_region
        #else
        vm_region_64
        #endif
        (
            mach_task_self(),
            &address,
            &vmsize,
            VM_REGION_BASIC_INFO,
            (vm_region_info_t)&info,
            &info_count,
            &object
        );
        if(status == KERN_INVALID_ADDRESS) {
            throw std::runtime_error("vm_region failed with KERN_INVALID_ADDRESS");
        }
        int perms = 0;
        if(info.protection & VM_PROT_READ) {
            perms |= PROT_READ;
        }
        if(info.protection & VM_PROT_WRITE) {
            perms |= PROT_WRITE;
        }
        if(info.protection & VM_PROT_EXECUTE) {
            perms |= PROT_EXEC;
        }
        return perms;
    }
    #else
    int get_page_protections(void* page) {
        auto page_addr = reinterpret_cast<uintptr_t>(page);
        std::ifstream stream("/proc/self/maps");
        stream>>std::hex;
        while(!stream.eof()) {
            uintptr_t start;
            uintptr_t stop;
            stream>>start;
            stream.ignore(1); // dash
            stream>>stop;
            if(stream.eof()) {
                break;
            }
            if(stream.fail()) {
                throw std::runtime_error("Failure reading /proc/self/maps");
            }
            if(page_addr >= start && page_addr < stop) {
                stream.ignore(1); // space
                char r, w, x; // there's a private/shared flag after these but we don't need it
                stream>>r>>w>>x;
                if(stream.fail() || stream.eof()) {
                    throw std::runtime_error("Failure reading /proc/self/maps");
                }
                int perms = 0;
                if(r == 'r') {
                    perms |= PROT_READ;
                }
                if(w == 'w') {
                    perms |= PROT_WRITE;
                }
                if(x == 'x') {
                    perms |= PROT_EXEC;
                }
                return perms;
            }
            stream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
        throw std::runtime_error("Failed to find mapping with page in /proc/self/maps");
    }
    #endif
    void mprotect_page(void* page, int page_size, int protections) {
        if(mprotect(page, page_size, protections) != 0) {
            throw std::runtime_error(microfmt::format("mprotect call failed: {}", strerror(errno)));
        }
    }
    int mprotect_page_and_return_old_protections(void* page, int page_size, int protections) {
        auto old_protections = get_page_protections(page);
        mprotect_page(page, page_size, protections);
        return old_protections;
    }
    void* allocate_page(int page_size) {
        auto page = mmap(nullptr, page_size, memory_readwrite, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if(page == MAP_FAILED) {
            throw std::runtime_error(microfmt::format("mmap call failed: {}", strerror(errno)));
        }
        return page;
    }
    #endif

    void perform_typeinfo_surgery(const std::type_info& info, bool(*do_catch_function)(const std::type_info*, const std::type_info*, void**, unsigned)) {
        if(vtable_size == 0) { // set to zero if we don't know what standard library we're working with
            return;
        }
        void* type_info_pointer = const_cast<void*>(static_cast<const void*>(&info));
        void** type_info_vtable_pointer = *static_cast<void***>(type_info_pointer);
        // the type info vtable pointer points to two pointers inside the vtable, adjust it back
        type_info_vtable_pointer -= 2;

        // for libstdc++ the class type info vtable looks like
        // 0x7ffff7f89d18 <_ZTVN10__cxxabiv117__class_type_infoE>:    0x0000000000000000  0x00007ffff7f89d00
        //                                                            [offset           ][typeinfo pointer ]
        // 0x7ffff7f89d28 <_ZTVN10__cxxabiv117__class_type_infoE+16>: 0x00007ffff7dd65a0  0x00007ffff7dd65c0
        //                                                            [base destructor  ][deleting dtor    ]
        // 0x7ffff7f89d38 <_ZTVN10__cxxabiv117__class_type_infoE+32>: 0x00007ffff7dd8f10  0x00007ffff7dd8f10
        //                                                            [__is_pointer_p   ][__is_function_p  ]
        // 0x7ffff7f89d48 <_ZTVN10__cxxabiv117__class_type_infoE+48>: 0x00007ffff7dd6640  0x00007ffff7dd6500
        //                                                            [__do_catch       ][__do_upcast      ]
        // 0x7ffff7f89d58 <_ZTVN10__cxxabiv117__class_type_infoE+64>: 0x00007ffff7dd65e0  0x00007ffff7dd66d0
        //                                                            [__do_upcast      ][__do_dyncast     ]
        // 0x7ffff7f89d68 <_ZTVN10__cxxabiv117__class_type_infoE+80>: 0x00007ffff7dd6580  0x00007ffff7f8abe8
        //                                                            [__do_find_public_src][other         ]
        // In libc++ the layout is
        //  [offset           ][typeinfo pointer ]
        //  [base destructor  ][deleting dtor    ]
        //  [noop1            ][noop2            ]
        //  [can_catch        ][search_above_dst ]
        //  [search_below_dst ][has_unambiguous_public_base]
        // Relevant documentation/implementation:
        //  https://itanium-cxx-abi.github.io/cxx-abi/abi.html
        //  libstdc++
        //   https://github.com/gcc-mirror/gcc/blob/b13e34699c7d27e561fcfe1b66ced1e50e69976f/libstdc%252B%252B-v3/libsupc%252B%252B/typeinfo
        //   https://github.com/gcc-mirror/gcc/blob/b13e34699c7d27e561fcfe1b66ced1e50e69976f/libstdc%252B%252B-v3/libsupc%252B%252B/class_type_info.cc
        //  libc++
        //   https://github.com/llvm/llvm-project/blob/648f4d0658ab00cf1e95330c8811aaea9481a274/libcxx/include/typeinfo
        //   https://github.com/llvm/llvm-project/blob/648f4d0658ab00cf1e95330c8811aaea9481a274/libcxxabi/src/private_typeinfo.h

        // shouldn't be anything other than 4096 but out of an abundance of caution
        auto page_size = get_page_size();
        if(page_size <= 0 && (page_size & (page_size - 1)) != 0) {
            throw std::runtime_error(
                microfmt::format("getpagesize() is not a power of 2 greater than zero (was {})", page_size)
            );
        }

        // allocate a page for the new vtable so it can be made read-only later
        // the OS cleans this up, no cleanup done here for it
        void* new_vtable_page = allocate_page(page_size);
        // make our own copy of the vtable
        memcpy(new_vtable_page, type_info_vtable_pointer, vtable_size * sizeof(void*));
        // ninja in the custom __do_catch interceptor
        auto new_vtable = static_cast<void**>(new_vtable_page);
        // double cast is done here because older (and some newer gcc versions) warned about it under -Wpedantic
        new_vtable[6] = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(do_catch_function));
        // make the page read-only
        mprotect_page(new_vtable_page, page_size, memory_readonly);

        // make the vtable pointer for unwind_interceptor's type_info point to the new vtable
        auto type_info_addr = reinterpret_cast<uintptr_t>(type_info_pointer);
        auto page_addr = type_info_addr & ~(page_size - 1);
        // make sure the memory we're going to set is within the page
        if(type_info_addr - page_addr + sizeof(void*) > static_cast<unsigned>(page_size)) {
            throw std::runtime_error("pointer crosses page boundaries");
        }
        auto old_protections = mprotect_page_and_return_old_protections(
            reinterpret_cast<void*>(page_addr),
            page_size,
            memory_readwrite
        );
        *static_cast<void**>(type_info_pointer) = static_cast<void*>(new_vtable + 2);
        mprotect_page(reinterpret_cast<void*>(page_addr), page_size, old_protections);
    }

    bool can_catch(
        const std::type_info* type,
        const std::type_info* throw_type,
        void** throw_obj,
        unsigned outer
    ) {
        if (*type == typeid(void)) {
            return true;
        }
        // get the vtable for the type_info and call the function pointer in the 6th slot
        // see below: perform_typeinfo_surgery
        void* type_info_pointer = const_cast<void*>(static_cast<const void*>(type));
        void** type_info_vtable_pointer = *static_cast<void***>(type_info_pointer);
        // the type info vtable pointer points to two pointers inside the vtable, adjust it back
        type_info_vtable_pointer -= 2;
        auto* can_catch_fn = reinterpret_cast<decltype(can_catch)*>(type_info_vtable_pointer[6]);
        return can_catch_fn(type, throw_type, throw_obj, outer);
    }
    #endif

    // called when unwinding starts after rethrowing, after search phase
    void rethrow_scope_cleanup() {
        internal::get_rethrow_switch() = false;
    }

    internal::scope_guard<void(&)()> setup_rethrow() {
        internal::get_rethrow_switch() = true;
        // will flip the switch back to true as soon as the search phase completes and the unwinding begins
        return internal::scope_exit<void(&)()>(rethrow_scope_cleanup);
    }
}
}

CPPTRACE_BEGIN_NAMESPACE
    namespace detail {
        CPPTRACE_FORCE_NO_INLINE void collect_current_trace(std::size_t skip) {
            internal::collect_current_trace(skip + 1);
        }

        #ifdef _MSC_VER
        bool matches_exception(EXCEPTION_POINTERS* exception_ptrs, const std::type_info& type_info) {
            __try {
                auto* exception_record = exception_ptrs->ExceptionRecord;
                // Check if the SEH exception is a C++ exception
                if(exception_record->ExceptionCode == EH_EXCEPTION_NUMBER) {
                    return internal::matches_exception(exception_record, type_info);
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                // pass
            }
            return false;
        }
        #else
        bool check_can_catch(
            const std::type_info* type,
            const std::type_info* throw_type,
            void** throw_obj,
            unsigned outer
        ) {
            return internal::can_catch(type, throw_type, throw_obj, outer);
        }

        void do_prepare_unwind_interceptor(const std::type_info& type_info, bool(*can_catch)(const std::type_info*, const std::type_info*, void**, unsigned)) {
            try {
                internal::perform_typeinfo_surgery(
                    type_info,
                    can_catch
                );
            } catch(std::exception& e) {
                internal::log::error("Exception occurred while preparing from_current support: {}", e.what());
            } catch(...) {
                internal::log::error("Unknown exception occurred while preparing from_current support");
            }
        }
        #endif
    }

    const raw_trace& raw_trace_from_current_exception() {
        return internal::current_exception_trace.get_raw_trace();
    }

    const stacktrace& from_current_exception() {
        return internal::current_exception_trace.get_resolved_trace();
    }

    const raw_trace& raw_trace_from_current_exception_rethrow() {
        return internal::saved_rethrow_trace.get_raw_trace();
    }

    const stacktrace& from_current_exception_rethrow() {
        return internal::saved_rethrow_trace.get_resolved_trace();
    }

    bool current_exception_was_rethrown() {
        if(internal::saved_rethrow_trace.is_resolved()) {
            return !internal::saved_rethrow_trace.get_resolved_trace().empty();
        } else {
            return !internal::saved_rethrow_trace.get_raw_trace().empty();
        }
    }

    // The non-argument overload is to serve as room for possible future optimization under Microsoft's STL
    CPPTRACE_FORCE_NO_INLINE void rethrow() {
        auto guard = internal::setup_rethrow();
        std::rethrow_exception(std::current_exception());
    }

    CPPTRACE_FORCE_NO_INLINE void rethrow(std::exception_ptr exception) {
        auto guard = internal::setup_rethrow();
        std::rethrow_exception(exception);
    }

    void clear_current_exception_traces() {
        internal::current_exception_trace = detail::lazy_trace_holder{raw_trace{}};
        internal::saved_rethrow_trace = detail::lazy_trace_holder{raw_trace{}};
    }
CPPTRACE_END_NAMESPACE
