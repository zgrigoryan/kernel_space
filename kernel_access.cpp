#include "kernel_access.h"
#include <iostream>

[[noreturn]] void run_kernel_access(std::uint64_t address, bool verbose)
{
    if (verbose)
        std::cout << "[kernel_access] writing to 0x" << std::hex << address
                  << std::dec << " … expect a crash!\n";

    volatile std::uint32_t *ptr =
        reinterpret_cast<volatile std::uint32_t *>(address);

    *ptr = 0xDEADBEEF;                   // -> page‑fault in user mode
    std::cout << "Should never get here\n";
    std::exit(EXIT_FAILURE);             // placate compilers
}

void print_kernel_access_help(std::string_view prog)
{
    std::cout <<
        "\nKernel‑access test\n"
        "  " << prog << " --mode kernel [--addr HEX]\n\n"
        "Options:\n"
        "  --addr HEX   Virtual address to poke (default 0xFFFF000000000000)\n";
}
