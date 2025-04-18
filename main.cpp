#ifdef _WIN32
#   define _CRT_SECURE_NO_WARNINGS        // silence MSVC CRT warnings
#endif

#include "heap_overflow.h"
#include "kernel_access.h"
#include "kaizen.h"

#include <csignal>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)            // -------- Windows : ISO setjmp/longjmp
    #include <setjmp.h>
    static jmp_buf JUMP_BUF;
    #define SETJMP(env)    setjmp(env)
    #define LONGJMP(env,v) longjmp(env,v)
    #include <windows.h>
    #include <eh.h>                 // _set_se_translator
#else                              // -------- POSIX : sigsetjmp/siglongjmp
    #include <csetjmp>
    static sigjmp_buf JUMP_BUF;
    #define SETJMP(env)    sigsetjmp(env,1)
    #define LONGJMP(env,v) siglongjmp(env,v)
#endif

// Run result structure
struct RunResult { bool crashed{}; long long ns{}; };

// Handler for segmentation fault signal
void segv_handler(int) { LONGJMP(JUMP_BUF, 1); }

// Function to run the tests with a guard against crashes
RunResult run_with_guard(const std::function<void()>& fn)
{
    zen::timer t;
    RunResult r;

#if defined(_WIN32)            // -------- Windows branch (SEH)
    if (SETJMP(JUMP_BUF) == 0) {
        __try {                         // catch AV/GP instantly
            t.start(); fn(); t.stop();
            r.crashed = false;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            t.stop(); r.crashed = true;
        }
    } else {                            // longjmp path (unused on Win)
        t.stop(); r.crashed = true;
    }

#else                               // -------- POSIX branch (SIGSEGV)
    std::signal(SIGSEGV, segv_handler);

    if (SETJMP(JUMP_BUF) == 0) {
        t.start(); fn(); t.stop();
        r.crashed = false;
    } else {
        t.stop(); r.crashed = true;
    }
    std::signal(SIGSEGV, SIG_DFL);
#endif

    r.ns = t.duration<zen::timer::nsec>().count();
    return r;
}

// Command-line argument parsing structure
struct Opt {
    enum class Which { Heap, Kernel, Both } test = Which::Both;
    int trials = 3;
    std::size_t alloc = 16, over = 1024;
    std::uint64_t addr = 0xFFFF000000000000ULL;
};

Opt parse(int argc, char** argv)
{
    zen::cmd_args a(argv, argc);
    Opt o;
    if (a.is_present("--help") || a.is_present("-h")) {
        std::cout << "Usage: " << argv[0] << " --test [heap|kernel|both] "
                  << "[--trials N] [--alloc N] [--overrun N] [--addr HEX]\n";
        std::exit(0);
    }
    if (a.is_present("--test")) {
        std::string t = a.get_options("--test")[0];
        if (t == "heap")   o.test = Opt::Which::Heap;
        else if (t == "kernel") o.test = Opt::Which::Kernel;
    }
    if (a.is_present("--trials"))  o.trials = std::stoi(a.get_options("--trials")[0]);
    if (a.is_present("--alloc"))   o.alloc  = std::stoull(a.get_options("--alloc")[0]);
    if (a.is_present("--overrun")) o.over   = std::stoull(a.get_options("--overrun")[0]);
    if (a.is_present("--addr"))    o.addr   = std::stoull(a.get_options("--addr")[0], nullptr, 16);
    return o;
}

// Main function to run tests
int main(int argc, char** argv)
{
    Opt opt = parse(argc, argv);

    std::ofstream csv("mem_crash_results.csv");
    csv << "Trial,Test,Time_ns,SegFaulted\n";

    std::vector<RunResult> heap_res, kern_res;
    
    std::cout << "Starting heap overflow test...\n";
    auto heap_fn = [&] { run_heap_overflow(opt.alloc, opt.over); };
    std::cout << "Heap overflow test finished.\n";
    
    std::cout << "Starting kernel access test...\n";
    auto kern_fn = [&] { run_kernel_access(opt.addr); };
    std::cout << "Kernel access test finished.\n";

    for (int t = 1; t <= opt.trials; ++t) {
        // Run heap test on all platforms (Linux/Windows)
        {
            auto r = run_with_guard(heap_fn);
            heap_res.push_back(r);
            csv << t << ",Heap," << r.ns << ',' << r.crashed << '\n';
        }

#if !defined(_WIN32)
        // Only run the kernel test on Linux/macOS, skip on Windows
        if (opt.test == Opt::Which::Kernel || opt.test == Opt::Which::Both) {
            auto r = run_with_guard(kern_fn);
            kern_res.push_back(r);
            csv << t << ",Kernel," << r.ns << ',' << r.crashed << '\n';
        }
#else
        // Skip kernel test on Windows
        if (opt.test != Opt::Which::Heap) {
            std::cout << "[info] Skipping kernel-access test on Windows\n";
        }
#endif
    }
    csv.close();

    auto summarise = [&](const std::vector<RunResult>& v) {
        long long total = 0; int faults = 0;
        for (auto& x : v) { total += x.ns; faults += x.crashed; }
        return std::pair<long long, int>{ v.empty() ? 0 : total / static_cast<long long>(v.size()), faults };
    };

    auto [h_avg, h_fault] = summarise(heap_res);
    auto [k_avg, k_fault] = summarise(kern_res);

    std::stringstream out;
    out << "\n| Test   | Avg time (ns) | Trials | SIGSEGVs |\n"
        << "|--------|--------------:|-------:|---------:|\n";
    if (!heap_res.empty())
        out << "| Heap   | " << std::setw(12) << h_avg << " | "
            << heap_res.size() << " | " << h_fault << " |\n";
#if !defined(_WIN32)
    if (!kern_res.empty())
        out << "| Kernel | " << std::setw(12) << k_avg << " | "
            << kern_res.size() << " | " << k_fault << " |\n";
#endif

    zen::print(out.str());
    return 0;
}
