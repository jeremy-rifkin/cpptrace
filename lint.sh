#!/bin/bash

status=0
while read f
do
    echo checking $f
    flags="-DCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE -DCPPTRACE_FULL_TRACE_WITH_STACKTRACE -DCPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE -DCPPTRACE_GET_SYMBOLS_WITH_LIBDL -DCPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE -DCPPTRACE_GET_SYMBOLS_WITH_NOTHING -DCPPTRACE_UNWIND_WITH_EXECINFO -DCPPTRACE_UNWIND_WITH_UNWIND -DCPPTRACE_UNWIND_WITH_NOTHING -DCPPTRACE_DEMANGLE_WITH_CXXABI -DCPPTRACE_DEMANGLE_WITH_NOTHING -DCPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH"
    clang-tidy $f -- -std=c++11 -Iinclude $@ $flags
    ret=$?
    if [ $ret -ne 0 ]; then
        status=1
    fi
done <<< $(find include src -name "*.hpp" -o -name "*.cpp" -not -path "test/speedtest.cpp" -not -path "src/full/full_trace_with_stacktrace.cpp")
exit $status
