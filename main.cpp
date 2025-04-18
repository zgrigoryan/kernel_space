// main.cpp ───────────────────────────────────────────────────────────────────
#ifdef _WIN32
#   define _CRT_SECURE_NO_WARNINGS
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

#if defined(_WIN32)
    #include <setjmp.h>
    static jmp_buf JUMP_BUF;
    #define SETJMP(env)    setjmp(env)
    #define LONGJMP(env,v) longjmp(env,v)
    #include <windows.h>
    #include <eh.h>
#else
    #include <csetjmp>
    static sigjmp_buf JUMP_BUF;
    #define SETJMP(env)    sigsetjmp(env,1)
    #define LONGJMP(env,v) siglongjmp(env,v)
#endif

struct RunResult { bool crashed{}; long long ns{}; };

void segv_handler(int) { LONGJMP(JUMP_BUF,1); }

RunResult run_with_guard(const std::function<void()>& fn)
{
    zen::timer t;
#if defined(_WIN32)
    __try {
        t.start(); fn(); t.stop();
        return {false, t.duration<zen::timer::nsec>().count()};
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        t.stop();
        return {true, t.duration<zen::timer::nsec>().count()};
    }
#else
    std::signal(SIGSEGV, segv_handler);
    if (SETJMP(JUMP_BUF) == 0) {
        t.start(); fn(); t.stop();
        std::signal(SIGSEGV, SIG_DFL);
        return {false, t.duration<zen::timer::nsec>().count()};
    } else {
        t.stop();
        std::signal(SIGSEGV, SIG_DFL);
        return {true, t.duration<zen::timer::nsec>().count()};
    }
#endif
}

struct Opt {
    enum class Which { Heap, Kernel, Both } test = Which::Both;
    int trials = 3;
    std::size_t alloc = 16, over = 1024;
    std::uint64_t addr = 0xFFFF000000000000ULL;
};

Opt parse(int argc, char** argv)
{
    zen::cmd_args a(argv,argc);
    if (a.is_present("--help")||a.is_present("-h")) {
        std::cout<<"Usage: "<<argv[0]<<" --test [heap|kernel|both] "
                 "[--trials N] [--alloc N] [--overrun N] [--addr HEX]\n";
        std::exit(0);
    }
    Opt o;
    if (a.is_present("--test")) {
        std::string t = a.get_options("--test")[0];
        if (t=="heap")      o.test = Opt::Which::Heap;
        else if (t=="kernel") o.test = Opt::Which::Kernel;
    }
    if (a.is_present("--trials"))  o.trials = std::stoi(a.get_options("--trials")[0]);
    if (a.is_present("--alloc"))   o.alloc  = std::stoull(a.get_options("--alloc")[0]);
    if (a.is_present("--overrun")) o.over   = std::stoull(a.get_options("--overrun")[0]);
    if (a.is_present("--addr"))    o.addr   = std::stoull(a.get_options("--addr")[0],nullptr,16);
    return o;
}

int main(int argc, char** argv)
{
    auto opt = parse(argc,argv);
    std::ofstream csv("mem_crash_results.csv");
    csv<<"Trial,Test,Time_ns,SegFaulted\n";

    std::vector<RunResult> heap_res, kern_res;
    auto heap_fn = [&]{ run_heap_overflow(opt.alloc, opt.over); };
    auto kern_fn = [&]{ run_kernel_access(opt.addr);           };

    for(int t = 1; t <= opt.trials; ++t) {
        // Always run heap on all platforms
        {
            auto r = run_with_guard(heap_fn);
            heap_res.push_back(r);
            csv<<t<<",Heap,"<<r.ns<<','<<r.crashed<<'\n';
        }

#if !defined(_WIN32)
        // Only run kernel‐access on non‐Windows
        if (opt.test == Opt::Which::Kernel || opt.test == Opt::Which::Both) {
            auto r = run_with_guard(kern_fn);
            kern_res.push_back(r);
            csv<<t<<",Kernel,"<<r.ns<<','<<r.crashed<<'\n';
        }
#else
        // On Windows we skip the kernel test entirely
        if (opt.test != Opt::Which::Heap) {
            std::cout << "[info] Skipping kernel‐access test on Windows\n";
        }
#endif
    }
    csv.close();

    auto summarise = [&](const std::vector<RunResult>& v){
        long long tot=0; int faults=0;
        for(auto& x:v){ tot+=x.ns; faults+=x.crashed; }
        return std::pair<long long,int>{
            v.empty()?0:tot/static_cast<long long>(v.size()), faults};
    };

    auto [h_avg, h_fault] = summarise(heap_res);
    auto [k_avg, k_fault] = summarise(kern_res);

    std::stringstream out;
    out<<"\n| Test   | Avg time (ns) | Trials | SIGSEGVs |\n"
       <<"|--------|--------------:|-------:|---------:|\n";
    if (!heap_res.empty())
        out<<"| Heap   | "<<std::setw(12)<<h_avg<<" | "
           <<heap_res.size()<<" | "<<h_fault<<" |\n";
#if !defined(_WIN32)
    if (!kern_res.empty())
        out<<"| Kernel | "<<std::setw(12)<<k_avg<<" | "
           <<kern_res.size()<<" | "<<k_fault<<" |\n";
#endif

    zen::print(out.str());
    return 0;
}
