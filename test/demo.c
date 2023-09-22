#define CTRACE_EXCEPTIONS ON
#include <ctrace/ctrace.h>

ctrace_string X(ctrace_string *str) {
    static ctrace_string x = { };
    if(x.data == NULL) x = ctrace_make_buffered_string(64);
    if(str) ctrace_write_move_string(x, *str);
    return x;
}

CTRACE_EXCEPTION(custom_exception) {
    ctrace_string x = X(NULL);
    return ctrace_exception_string(x);
}

void trace(int n) {
    if(n == 1) ctrace_throw(cpptrace_exception);
    else if(n == 2) ctrace_throw(custom_exception);
}

void bar(int x, int n) {
    if(x == 0) {
        trace(n);
    } else {
        bar(x - 1, n);
    }
}

void foo(int n, ...) {
    bar(n, n);
}

void function_two(int, float n) {
    foo((int)n, 2, 3, 4, 5, 6, 7, 8, 9, 10);
}

void function_one(int n) {
    function_two(0, n);
}

int main(int argc, char* argv[]) {
    CTRACE_LMODE(debug);
    CTRACE_REGISTER_EXCEPTION(custom_exception);

    if(argc == 2) {
        ctrace_string str = ctrace_make_string(argv[1]);
        X(&str);
    }
    else if(argc > 2) {
        ctrace_string str = ctrace_make_string(argv[2]);
        X(&str);
    }

    CTRACE_TRY {
        function_one(argc - 1);
    } CTRACE_CATCH(custom_exception) {
        ctrace_ex_t e = ctrace_exception_ptr();
        CTRACE_INFO("X(NULL) is: %s\n", ctrace_get_cstring(e->what()));
        ctrace_trace resolved = ctrace_resolve(e->trace);
        ctrace_print_trace(resolved, ectrace_default);
        ctrace_free_trace(resolved);
        ctrace_exception_release();
    } CTRACE_CATCHALL() {
        ctrace_ex_t e = ctrace_exception_ptr();
        ctrace_fputs(e->what(), stderr);
        ctrace_exception_release();
    }

    return 0;
}
