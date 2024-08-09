cc_library(
    name = "cpptrace",
    srcs = glob([
        "src/**/*.hpp",
        "src/**/*.cpp",
    ]),
    local_defines = [
        "CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF",
        "CPPTRACE_DEMANGLE_WITH_CXXABI",
        "CPPTRACE_UNWIND_WITH_UNWIND",
    ],
    hdrs = glob([
        "include/cpptrace/*.hpp",
        "include/ctrace/*.h",
    ]),
    includes = ["include"],
    deps = [
        "@libdwarf//:libdwarf",
    ],
    copts = [
        "-Wall",
        "-Wextra",
        "-Werror=return-type",
        "-Wundef",
        "-Wuninitialized",
        "-fPIC",
        "-std=c++17"
    ],
    visibility = ["//visibility:public"],
)
