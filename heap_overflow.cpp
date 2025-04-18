#include "heap_overflow.h"
#include <cstring>   // std::memset
#include <cstdlib>   // std::malloc
#include <iostream>

void run_heap_overflow(std::size_t alloc_sz,
                       std::size_t overrun_sz,
                       bool verbose)
{
    if (verbose)
        std::cout << "[heap_overflow] malloc " << alloc_sz
                  << " bytes, then write +" << overrun_sz << '\n';

    char* buf = static_cast<char*>(std::malloc(alloc_sz));
    if (!buf) {
        std::perror("malloc");
        std::exit(EXIT_FAILURE);
    }

    std::cout << "[heap_overflow] Memory allocated at: " << static_cast<void*>(buf) << '\n';
    std::memset(buf, 0, alloc_sz);
    for (std::size_t i = 0; i < overrun_sz; ++i) {
        buf[alloc_sz + i] = 'X';  // this will trigger an SEH/SIGSEGV
    }

    // We intentionally corrupt the block’s metadata, so calling free()
    // would abort immediately instead of faulting on the guard page.
    std::cout << "[heap_overflow] Memory write completed. Guard page likely triggered.\n";
}

void print_heap_overflow_help(std::string_view prog)
{
    std::cout <<
        "\nHeap‑overflow test\n"
        "  " << prog << " --mode heap [--alloc N] [--overrun N]\n\n"
        "Options:\n"
        "  --alloc N     Allocation size in bytes   (default 16)\n"
        "  --overrun N   Bytes to write past end    (default 1024)\n";
}
