#ifndef HEAP_OVERFLOW_H
#define HEAP_OVERFLOW_H

#include <cstddef>     // std::size_t
#include <string_view>

/**
 * Allocates `alloc_sz` bytes on the heap, initialises them to zero,
 * then deliberately walks `overrun_sz` bytes beyond the allocation.
 *
 * @param alloc_sz     Size of the allocation in bytes
 * @param overrun_sz   How many bytes past the end to write
 * @param verbose      If true, prints progress information
 */
void run_heap_overflow(std::size_t alloc_sz,
                       std::size_t overrun_sz,
                       bool verbose = true);

/** Command‑line helper: recognises options that belong to this test. */
void print_heap_overflow_help(std::string_view program);
#endif // HEAP_OVERFLOW_H
