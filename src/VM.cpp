#include "VM.h"

namespace nt
{
    VM::VM(CursorManager& cursors) : cursors_(cursors) {}

    Tuple* VM::Next(PlanNode* node)
    {
        if (node == nullptr)
            return nullptr;

        switch (node->op)
        {
            case PlanNode::Op::SCAN:
                return cursors_.Next(node->scan_cursor);
        }

        return nullptr;
    }
}
