#include "netplay/clock.h"
#include "netplay/scheduler.h"
#include <cstdio>

int main()
{
    if (netplay::Enabled() || netplay::Cooperative())
        return 1;
    puts("session startup: invalid prerequisites reach the window's preflight instead of exiting before main");
}
