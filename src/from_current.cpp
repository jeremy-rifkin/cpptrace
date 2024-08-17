#include <cpptrace/cpptrace.hpp>
#define CPPTRACE_DONT_PREPARE_UNWIND_INTERCEPTOR_ON
#include <cpptrace/from_current.hpp>

#include <system_error>
#include <typeinfo>

#include "utils/common.hpp"
#include "utils/microfmt.hpp"
#include "utils/utils.hpp"

#ifndef _MSC_VER
 #include <array>
 #include <string.h>
 #ifndef _WIN32
  #include <sys/mman.h>
  #include <unistd.h>
 #else
  #include <windows.h>
 #endif
#endif

namespace cpptrace {
    namespace detail {
        thread_local lazy_trace_holder current_exception_trace;

        #ifndef _MSC_VER
        CPPTRACE_FORCE_NO_INLINE
        bool intercept_unwind(const std::type_info*, const std::type_info*, void**, unsigned) {
            current_exception_trace = lazy_trace_holder(cpptrace::generate_raw_trace(1));
            return false;
        }

        unwind_interceptor::~unwind_interceptor() = default;

        #if defined(__GLIBCXX__) || defined(__GLIBCPP__)
            constexpr size_t vtable_size = 11;
        #elif defined(_LIBCPP_VERSION)
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
        void mprotect_page(void* page, int page_size, int protections) {
            DWORD old_protections;
            if(!VirtualProtect(page, page_size, protections, &old_protections)) {
                throw std::runtime_error(
                    microfmt::format(
                        "VirtualProtect call failed: {}",
                        std::system_error(GetLastError(), std::system_category()).what()
                    )
                );
            }
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
            return getpagesize();
        }
        constexpr auto memory_readonly = PROT_READ;
        constexpr auto memory_readwrite = PROT_READ | PROT_WRITE;
        void mprotect_page(void* page, int page_size, int protections) {
            if(mprotect(page, page_size, protections) != 0) {
                throw std::runtime_error(microfmt::format("mprotect call failed: {}", strerror(errno)));
            }
        }
        void* allocate_page(int page_size) {
            auto page = mmap(nullptr, page_size, memory_readwrite, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            if(page == MAP_FAILED) {
                throw std::runtime_error(microfmt::format("mmap call failed: {}", strerror(errno)));
            }
            return page;
        }
        #endif

        // allocated below, cleaned up by OS after exit
        void* new_vtable_page = nullptr;

        void perform_typeinfo_surgery(const std::type_info& info) {
            if(vtable_size == 0) { // set to zero if we don't know what standard library we're working with
                return;
            }
            void* type_info_pointer = const_cast<void*>(static_cast<const void*>(&info));
            void* type_info_vtable_pointer = *static_cast<void**>(type_info_pointer);
            // the type info vtable pointer points to two pointers inside the vtable, adjust it back
            type_info_vtable_pointer = static_cast<void*>(static_cast<void**>(type_info_vtable_pointer) - 2);

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

            // probably 4096 but out of an abundance of caution
            auto page_size = get_page_size();
            if(page_size <= 0 && (page_size & (page_size - 1)) != 0) {
                throw std::runtime_error(
                    microfmt::format("getpagesize() is not a power of 2 greater than zero (was {})", page_size)
                );
            }

            // allocate a page for the new vtable so it can be made read-only later
            new_vtable_page = allocate_page(page_size);
            // make our own copy of the vtable
            memcpy(new_vtable_page, type_info_vtable_pointer, vtable_size * sizeof(void*));
            // ninja in the custom __do_catch interceptor
            auto new_vtable = static_cast<void**>(new_vtable_page);
            new_vtable[6] = reinterpret_cast<void*>(intercept_unwind);
            // make the page read-only
            mprotect_page(new_vtable_page, page_size, memory_readonly);

            // make the vtable pointer for unwind_interceptor's type_info point to the new vtable
            auto type_info_addr = reinterpret_cast<uintptr_t>(type_info_pointer);
            auto page_addr = type_info_addr & ~(page_size - 1);
            // make sure the memory we're going to set is within the page
            if(type_info_addr - page_addr + sizeof(void*) > static_cast<unsigned>(page_size)) {
                throw std::runtime_error("pointer crosses page boundaries");
            }
            // TODO: Check perms of page
            mprotect_page(reinterpret_cast<void*>(page_addr), page_size, memory_readwrite);
            *static_cast<void**>(type_info_pointer) = static_cast<void*>(new_vtable + 2);
            mprotect_page(reinterpret_cast<void*>(page_addr), page_size, memory_readonly);
        }

        void do_prepare_unwind_interceptor() {
            static bool did_prepare = false;
            if(!did_prepare) {
                try {
                    perform_typeinfo_surgery(typeid(cpptrace::detail::unwind_interceptor));
                } catch(std::exception& e) {
                    std::fprintf(
                        stderr,
                        "Cpptrace: Exception occurred while preparing from_current support: %s\n",
                        e.what()
                    );
                } catch(...) {
                    std::fprintf(stderr, "Cpptrace: Unknown exception occurred while preparing from_current support\n");
                }
                did_prepare = true;
            }
        }
        #else
        CPPTRACE_FORCE_NO_INLINE int exception_filter() {
            current_exception_trace = lazy_trace_holder(cpptrace::generate_raw_trace(1));
            return EXCEPTION_CONTINUE_SEARCH;
        }
        #endif
    }

    const raw_trace& raw_trace_from_current_exception() {
        return detail::current_exception_trace.get_raw_trace();
    }

    const stacktrace& from_current_exception() {
        return detail::current_exception_trace.get_resolved_trace();
    }
}
