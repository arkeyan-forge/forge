// Checks that a fault report can name the recompiled function it stopped in: dladdr finds a sub_XXXXXXXX symbol of
// the executable. On Linux an executable's own functions are only in its dynamic symbol table when it is linked with
// ENABLE_EXPORTS, as ArkeyanRuntime is; without it every Linux report said "none named on the host stack".
// usage: ArkeyanCrashSymbolsTest
#include <cstdio>
#include <cstring>
#include <dlfcn.h>

int sub_82000000(int value);

__attribute__((noinline)) int sub_82000000(int value) { return value * 3 + 1; }

int main()
{
    int (*volatile function)(int) = sub_82000000;
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(function), &info) == 0 || info.dli_sname == nullptr ||
        strstr(info.dli_sname, "sub_82000000") == nullptr)
    {
        fprintf(stderr, "FAIL: dladdr does not name sub_82000000 (symbol %s)\n",
            info.dli_sname != nullptr ? info.dli_sname : "none");
        return 1;
    }
    printf("ArkeyanCrashSymbolsTest: all passed (%d)\n", function(1));
    return 0;
}
