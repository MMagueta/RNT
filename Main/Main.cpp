#include <iostream>

#include "Runtime/NtContext.h"

int main()
{
    nt::NtContext nt;

    std::cout << "NT context running: " << (nt.IsRunning() ? "yes" : "no") << '\n';
    return nt.IsRunning() ? 0 : 1;
}
