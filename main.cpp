#include "heap_overflow.h"
#include "kernel_access.h"
#include "kaizen.h"          // Zen header

#include <csignal>
#include <cstddef>
#include <cstdint>
#include <csetjmp>  // For sigjmp_buf
#include <functional>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>


#if defined(_WIN32)
    #include <setjmp.h>      // ← ISO version on Windows
    static jmp_buf JUMP_BUF;
    #define SETJMP(env)  setjmp(env)
    #define LONGJMP(env, val) longjmp(env, val)
#else
    #include <csetjmp>       // POSIX version everywhere else
    static sigjmp_buf JUMP_BUF;
    #define SETJMP(env)  sigsetjmp(env, 1)
    #define LONGJMP(env, val) siglongjmp(env, val)
#endif

/* ------------------------------------------------------------------------- */
/* Small wrapper that runs `fn`, catches SIGSEGV and measures nanoseconds    */
/* ------------------------------------------------------------------------- */
struct RunResult {
    bool crashed;               // true  ⇒ caught SIGSEGV
    long long ns;               // time until SIGSEGV or completion
};

void segv_handler(int) { siglongjmp(JUMP_BUF, 1); }

RunResult run_with_guard(const std::function<void()> &fn)
{
    zen::timer t;
    std::signal(SIGSEGV, segv_handler);

    RunResult res;
    if (sigsetjmp(JUMP_BUF, 1) == 0) {
        t.start();
        fn();                          // either returns (no crash) or SEGSEGV
        t.stop();
        res.crashed = false;
    } else {
        t.stop();                      // came back via longjmp
        res.crashed = true;
    }
    res.ns = t.duration<zen::timer::nsec>().count();

    std::signal(SIGSEGV, SIG_DFL);     // restore default action
    return res;
}

/* ------------------------------------------------------------------------- */
/* Command‑line parsing                                                       */
/* ------------------------------------------------------------------------- */
struct Opt {
    enum class Which { Heap, Kernel, Both } test = Which::Both;
    int trials         = 3;
    std::size_t alloc  = 16;
    std::size_t over   = 1024;
    std::uint64_t addr = 0xFFFF000000000000ULL;
};

Opt parse(int argc, char **argv)
{
    zen::cmd_args a(argv, argc);
    if (a.is_present("--help") || a.is_present("-h")) {
        std::cout <<
            "Usage: " << argv[0] << " --test [heap|kernel|both] "
            "[--trials N] [--alloc N] [--overrun N] [--addr HEX]\n";
        std::exit(0);
    }

    Opt o;
    if (a.is_present("--test")) {
        std::string t = a.get_options("--test")[0];
        if (t == "heap")   o.test = Opt::Which::Heap;
        else if (t == "kernel") o.test = Opt::Which::Kernel;
        else                o.test = Opt::Which::Both;
    }
    if (a.is_present("--trials"))  o.trials = std::stoi(a.get_options("--trials")[0]);
    if (a.is_present("--alloc"))   o.alloc  = std::stoull(a.get_options("--alloc")[0]);
    if (a.is_present("--overrun")) o.over   = std::stoull(a.get_options("--overrun")[0]);
    if (a.is_present("--addr"))
        o.addr = std::stoull(a.get_options("--addr")[0], nullptr, 16);
    return o;
}

/* ------------------------------------------------------------------------- */
int main(int argc, char **argv)
{
    Opt opt = parse(argc, argv);

    std::ofstream csv("mem_crash_results.csv");
    csv << "Trial,Test,Time_ns,SegFaulted\n";

    std::vector<RunResult> heap_res, kern_res;

    auto heap_fn  = [&]{ run_heap_overflow(opt.alloc, opt.over); };
    auto kern_fn  = [&]{ run_kernel_access(opt.addr);           };

    for (int t = 1; t <= opt.trials; ++t) {
        if (opt.test == Opt::Which::Heap || opt.test == Opt::Which::Both) {
            auto r = run_with_guard(heap_fn);
            heap_res.push_back(r);
            csv << t << ",Heap,"   << r.ns << ',' << r.crashed << '\n';
        }
        if (opt.test == Opt::Which::Kernel || opt.test == Opt::Which::Both) {
            auto r = run_with_guard(kern_fn);
            kern_res.push_back(r);
            csv << t << ",Kernel," << r.ns << ',' << r.crashed << '\n';
        }
    }
    csv.close();

    // Pretty summary
    auto summarise = [](const std::vector<RunResult>& v){
        long long total = 0; int crashes = 0;
        for (auto &x: v) { total += x.ns; crashes += x.crashed; }
        return std::make_pair(
            v.empty() ? 0 : total / static_cast<long long>(v.size()), crashes);
    };

    auto [heap_avg, heap_crash] = summarise(heap_res);
    auto [kern_avg, kern_crash] = summarise(kern_res);

    std::stringstream out;
    out << "\n| Test   | Avg time (ns) | Trials | SIGSEGVs |\n"
        << "|--------|--------------:|-------:|---------:|\n";
    if (!heap_res.empty())
        out << "| Heap   | " << std::setw(12) << heap_avg
            << " | " << heap_res.size()
            << " | " << heap_crash << " |\n";
    if (!kern_res.empty())
        out << "| Kernel | " << std::setw(12) << kern_avg
            << " | " << kern_res.size()
            << " | " << kern_crash << " |\n";

    zen::print(out.str());
    return 0;
}
