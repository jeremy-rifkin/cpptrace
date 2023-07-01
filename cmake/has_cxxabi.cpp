#include <execinfo.h>

int main() {
    void* frames[10];
    int size = backtrace(frames, 10);
}
