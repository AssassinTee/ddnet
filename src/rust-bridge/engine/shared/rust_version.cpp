#include "base/rust.h"
#include "engine/console.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#endif // __clang__

extern "C" {
void CXXBRIDGE_SYMBOL(RustVersionPrint)(::IConsole const &console) noexcept;

void CXXBRIDGE_SYMBOL(RustVersionRegister)(::IConsole &console) noexcept;
} // extern "C"

void RustVersionPrint(::IConsole const &console) noexcept
{
	CXXBRIDGE_SYMBOL(RustVersionPrint)(console);
}

void RustVersionRegister(::IConsole &console) noexcept
{
	CXXBRIDGE_SYMBOL(RustVersionRegister)(console);
}
