
#include <benchmark/benchmark.h>
#include <iostream>

#ifdef _WIN32
#include <Windows.h>
#endif

using namespace std;


int main(int argc, char** argv)
{
    #ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    SetThreadAffinityMask(GetCurrentThread(), (1 << 2));
    #endif

#ifdef NDEBUG
    cout << "Running in release mode." << endl;
#else
    cout << "Running in debug mode." << endl;
#endif
    char arg0_default[] = "benchmark";
    char* args_default = arg0_default;
    if (!argv)
    {
        argc = 1;
        argv = &args_default;
    }
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();

    cout << "All Done" << endl;
    return 0;
}

