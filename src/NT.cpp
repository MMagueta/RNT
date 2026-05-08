#include "NT.h"

#ifdef NT_CONSOLE_APP
#include <iostream>
#endif

namespace nt
{
    bool NT::IsRunning() const
    {
        return true;
    }
}

#ifdef NT_CONSOLE_APP
int main()
{
    const nt::NT runtime;
    std::cout << "RNT running: " << (runtime.IsRunning() ? "yes" : "no") << '\n';
    const int exit_code = runtime.IsRunning() ? 0 : 1;

#ifdef NT_WAIT_ON_EXIT
    std::cout << "Press Enter to exit...";
    std::cin.get();
#endif

    return exit_code;
}
#endif
