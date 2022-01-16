#include <AK/Error.h>
#include <AK/Format.h>
#include <LibMain/Main.h>

static ErrorOr<void> e() { return Error::from_syscall("yaksplode", -ENOTCONN); }
static ErrorOr<void> d()
{
    TRY(e());
    return {};
}
static ErrorOr<void> c()
{
    return d();
}
static ErrorOr<void> b()
{
    TRY(c());
    return {};
}
static ErrorOr<void> a()
{
    return b();
}

ErrorOr<int> serenity_main(Main::Arguments)
{
    TRY(a());

    return 0;
}
