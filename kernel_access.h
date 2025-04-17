#ifndef KERNEL_ACCESS_H
#define KERNEL_ACCESS_H

#include <cstdint>   // std::uint64_t
#include <string_view>

/**
 * Attempts to write the 32‑bit pattern 0xDEADBEEF to the supplied
 * virtual address.  On any modern OS this lives in supervisor space,
 * so the CPU raises a page–fault and the program dies with SIGSEGV.
 *
 * @param address   Virtual address to poke (default is a canonical
 *                  kernel address on x86‑64 Linux).
 * @param verbose   If true, the function prints what it is about to do.
 */
[[noreturn]] void run_kernel_access(std::uint64_t address,
                                    bool verbose = true);

/** Command‑line helper: recognises options that belong to this test. */
void print_kernel_access_help(std::string_view program);
#endif // KERNEL_ACCESS_H
