#include "heap_overflow.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <cstddef>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#  include <sys/mman.h>
#  include <cstdio>
#endif

void run_heap_overflow(std::size_t alloc_sz,
                       std::size_t overrun_sz,
                       bool verbose)
{
    if (verbose)
        std::cout << "[heap_overflow] allocating " << alloc_sz
                  << " bytes + guard page, then writing +" << overrun_sz << '\n';

#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    size_t page = static_cast<size_t>(si.dwPageSize);
#else
    size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
#endif

    size_t rounded = ((alloc_sz + page - 1) / page) * page;

#ifdef _WIN32
    void* region = VirtualAlloc(nullptr,
                                rounded + page,
                                MEM_COMMIT | MEM_RESERVE,
                                PAGE_READWRITE);
    if (!region) {
        std::cerr << "VirtualAlloc failed: " << GetLastError() << std::endl;
        std::exit(EXIT_FAILURE);
    }
    DWORD old;
    if (!VirtualProtect(static_cast<char*>(region) + rounded,
                        page,
                        PAGE_NOACCESS,
                        &old))
    {
        std::cerr << "VirtualProtect failed: " << GetLastError() << std::endl;
        VirtualFree(region, 0, MEM_RELEASE);
        std::exit(EXIT_FAILURE);
    }
#else
    void* region = mmap(nullptr,
                        rounded + page,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        std::exit(EXIT_FAILURE);
    }
    if (mprotect(static_cast<char*>(region) + rounded,
                 page,
                 PROT_NONE) != 0)
    {
        perror("mprotect");
        munmap(region, rounded + page);
        std::exit(EXIT_FAILURE);
    }
#endif

    char* buf = static_cast<char*>(region);

    std::memset(buf, 0, alloc_sz);

    for (size_t i = 0; i < overrun_sz; ++i)
        buf[alloc_sz + i] = 'X'; 

#ifdef _WIN32
    VirtualFree(region, 0, MEM_RELEASE);
#else
    munmap(region, rounded + page);
#endif

    std::cout << "[heap_overflow] Memory write completed. Guard page triggered." << '\n';
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
