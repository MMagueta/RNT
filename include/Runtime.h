#pragma once

#include "Api.h"

namespace nt
{
    class NT_API NT
    {
    public:
        bool IsRunning() const;
        void SimulateEntryCall();
    };
}
