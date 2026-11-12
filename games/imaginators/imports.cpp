// Imports that Skylanders Imaginators links against and no game before SuperChargers has: SuperChargers' ten (its
// imports.cpp, compiled into this game too until they move to runtime/kernel/) and seven of its own. The console is
// offline with one local profile; every output is written and the first call of each is logged with its argument
// registers, because none of them is documented here.
#include "../superchargers/imports.cpp"

PPC_EXTERN_FUNC(__imp__XGetLanguage);

// (): the console language, the same answer as XGetLanguage (runtime/kernel/xconfig.cpp).
PPC_FUNC(__imp__XamGetLanguage)
{
    LOG_FIRST_CALL("XamGetLanguage");
    __imp__XGetLanguage(ctx, base);
}

// (caller, request handle, option, value, size): no HTTP session exists without a network; FALSE.
PPC_FUNC(__imp__NetDll_XHttpSetOption)
{
    LOG_FIRST_CALL("NetDll_XHttpSetOption");
    ctx.r3.u64 = 0;
}

// (mode): background downloads stay off; success.
PPC_FUNC(__imp__XamBackgroundDownloadSetMode)
{
    LOG_FIRST_CALL("XamBackgroundDownloadSetMode");
    ctx.r3.u64 = StatusSuccess;
}

// (user index, ...): no Live profile, no marketplace.
PPC_FUNC(__imp__XamShowMarketplaceDownloadItemsUI)
{
    LOG_FIRST_CALL("XamShowMarketplaceDownloadItemsUI");
    ctx.r3.u64 = ErrorNotLoggedOn;
}

// (...): no Live profile, so no membership tier and no online country; the argument lists are not known yet [?].
PPC_FUNC(__imp__XamUserGetMembershipTierFromXUID)
{
    LOG_FIRST_CALL("XamUserGetMembershipTierFromXUID");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamUserGetOnlineCountryFromXUID)
{
    LOG_FIRST_CALL("XamUserGetOnlineCountryFromXUID");
    ctx.r3.u64 = 0;
}

// (): no Live logon, no logon title.
PPC_FUNC(__imp__XNetLogonGetTitleID)
{
    LOG_FIRST_CALL("XNetLogonGetTitleID");
    ctx.r3.u64 = 0;
}
