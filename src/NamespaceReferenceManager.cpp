#include "NamespaceReferenceManager.h"

namespace nt
{
    NamespaceReferenceManager::NamespaceReferenceManager(ObjectManager& objects)
        : objects_(objects)
    {}

    std::vector<std::string> NamespaceReferenceManager::Resolve(
        std::vector<std::string> path) const
    {
        constexpr int kMaxReparseDepth = 30;

        for (int depth = 0; depth < kMaxReparseDepth; ++depth)
        {
            // /system/branches/<name>/<sub>... → /system/snapshots/<target>/<sub>...
            // Only rewrite when there is actually a sub-path beyond the branch
            // name; otherwise the caller is opening the branch object itself.
            if (path.size() > 3
                && path[0] == "system"
                && path[1] == "branches")
            {
                const std::vector<std::string> branch_path{
                    "system", "branches", path[2]
                };
                auto* entry = objects_.Find(branch_path);
                if (!entry || !entry->object) return path;

                auto* branch =
                    dynamic_cast<ObjectManager::Branch*>(entry->object.get());
                if (!branch) return path;
                if (branch->target_hash.empty()) return path;

                std::vector<std::string> rewritten{
                    "system", "snapshots", branch->target_hash
                };
                for (size_t i = 3; i < path.size(); ++i)
                    rewritten.push_back(path[i]);

                path = std::move(rewritten);
                continue;
            }

            return path;
        }

        // Cycle guard tripped — surface as a lookup miss.
        return {};
    }
}
