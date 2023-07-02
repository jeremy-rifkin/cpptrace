#include <stacktrace>

int main() {
    std::stacktrace trace = std::stacktrace::current();
    for(const auto entry : trace) {
        (void)entry;
    }
}
