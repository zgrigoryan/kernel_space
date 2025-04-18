#ifdef _WIN32
#   define _CRT_SECURE_NO_WARNINGS            // silence MSVC CRT warnings
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

#if defined(_WIN32)                // ── setjmp/longjmp on Windows
    #include <setjmp.h>
    static jmp_buf JUMP_BUF;
    #define SETJMP(env)   setjmp(env)
    #define LONGJMP(env,v) longjmp(env,v)
#else                              // ── sigsetjmp/siglongjmp elsewhere
    #include <csetjmp>
    static sigjmp_buf JUMP_BUF;
    #define SETJMP(env)   sigsetjmp(env,1)
    #define LONGJMP(env,v) siglongjmp(env,v)
#endif
#ifdef _WIN32
#   define _CRT_SECURE_NO_WARNINGS
#   include <windows.h>
#   include <eh.h>          // _set_se_translator
#endif
...
#if defined(_WIN32)         // MSVC : jmp_buf branch unchanged
    #include <setjmp.h>
    static jmp_buf JUMP_BUF;
    #define SETJMP(env)   setjmp(env)
    #define LONGJMP(env,v) longjmp(env,v)
#else
    ...
#endif
...
RunResult run_with_guard(const std::function<void()>& fn)
{
#ifdef _WIN32
    // Convert SEH to C++ so we can 'catch' access violations
    _set_se_translator([](unsigned code, EXCEPTION_POINTERS*) {
        throw code;                     // re‑throw as unsigned int
    });
#endif
    zen::timer t;
#if !defined(_WIN32)                   // POSIX branch keeps the sig handler
    std::signal(SIGSEGV, segv_handler);
#endif
    RunResult r;
    try {
        if (SETJMP(JUMP_BUF) == 0) {   // (on Windows just returns 0)
            t.start(); fn(); t.stop();
            r.crashed = false;
        } else {
            t.stop(); r.crashed = true;    // POSIX long‑jmp comeback
        }
    }
#ifdef _WIN32
    catch (unsigned) {                 // caught SEH: AV, guard‑page, …
        t.stop(); r.crashed = true;
    }
#endif
    r.ns = t.duration<zen::timer::nsec>().count();
#if !defined(_WIN32)
    std::signal(SIGSEGV, SIG_DFL);
#endif
    return r;
}

/* ------------------------------------------------------------------------- */
struct RunResult { bool crashed{}; long long ns{}; };

void segv_handler(int){ LONGJMP(JUMP_BUF,1); }

RunResult run_with_guard(const std::function<void()>& fn)
{
    zen::timer t;
    std::signal(SIGSEGV, segv_handler);

    RunResult r;
    if (SETJMP(JUMP_BUF) == 0) {          // normal path
        t.start(); fn(); t.stop();
        r.crashed = false;
    } else {                              // came back via LONGJMP
        t.stop();  r.crashed = true;
    }
    r.ns = t.duration<zen::timer::nsec>().count();
    std::signal(SIGSEGV, SIG_DFL);
    return r;
}
/* ------------------------------------------------------------------------- */
struct Opt {
    enum class Which { Heap, Kernel, Both } test = Which::Both;
    int trials = 3; std::size_t alloc = 16, over = 1024;
    std::uint64_t addr = 0xFFFF000000000000ULL;
};

Opt parse(int argc,char** argv)
{
    zen::cmd_args a(argv, argc); Opt o;
    if (a.is_present("--help")||a.is_present("-h")){
        std::cout<<"Usage: "<<argv[0]<<" --test [heap|kernel|both] "
                   "[--trials N] [--alloc N] [--overrun N] [--addr HEX]\n";
        std::exit(0);}
    if (a.is_present("--test")){
        std::string t=a.get_options("--test")[0];
        o.test=(t=="heap")?Opt::Which::Heap:(t=="kernel")?Opt::Which::Kernel:Opt::Which::Both;}
    if (a.is_present("--trials"))  o.trials=std::stoi(a.get_options("--trials")[0]);
    if (a.is_present("--alloc"))   o.alloc =std::stoull(a.get_options("--alloc")[0]);
    if (a.is_present("--overrun")) o.over  =std::stoull(a.get_options("--overrun")[0]);
    if (a.is_present("--addr"))    o.addr  =std::stoull(a.get_options("--addr")[0],nullptr,16);
    return o;
}
/* ------------------------------------------------------------------------- */
int main(int argc,char** argv)
{
    Opt opt=parse(argc,argv);
    std::ofstream csv("mem_crash_results.csv");
    csv<<"Trial,Test,Time_ns,SegFaulted\n";

    std::vector<RunResult> heap_res,kern_res;
    auto heap_fn=[&]{run_heap_overflow(opt.alloc,opt.over);};
    auto kern_fn=[&]{run_kernel_access(opt.addr);};

    for(int t=1;t<=opt.trials;++t){
        if(opt.test==Opt::Which::Heap||opt.test==Opt::Which::Both){
            auto r=run_with_guard(heap_fn); heap_res.push_back(r);
            csv<<t<<",Heap,"<<r.ns<<','<<r.crashed<<'\n';}
        if(opt.test==Opt::Which::Kernel||opt.test==Opt::Which::Both){
            auto r=run_with_guard(kern_fn); kern_res.push_back(r);
            csv<<t<<",Kernel,"<<r.ns<<','<<r.crashed<<'\n';}
    }
    csv.close();

    auto summary=[&](const std::vector<RunResult>& v){
        long long tot=0;int c=0;for(auto&x:v){tot+=x.ns;c+=x.crashed;}
        return std::pair<long long,int>{v.empty()?0:tot/(long long)v.size(),c};};

    auto [h_avg,h_segv]=summary(heap_res);
    auto [k_avg,k_segv]=summary(kern_res);

    std::stringstream out;
    out<<"\n| Test   | Avg time (ns) | Trials | SIGSEGVs |\n"
       <<"|--------|--------------:|-------:|---------:|\n";
    if(!heap_res.empty()) out<<"| Heap   | "<<std::setw(12)<<h_avg<<" | "
                              <<heap_res.size()<<" | "<<h_segv<<" |\n";
    if(!kern_res.empty()) out<<"| Kernel | "<<std::setw(12)<<k_avg<<" | "
                              <<kern_res.size()<<" | "<<k_segv<<" |\n";
    zen::print(out.str());
    return 0;
}
