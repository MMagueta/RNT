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
            // /system/sessions/<X>/branches/<name>/<sub>... — per-session override
            // takes precedence; absent override falls back to the global branch.
            // Only rewrite when a real sub-path exists beyond <name>.
            if (path.size() > 5
                && path[0] == "system"
                && path[1] == "sessions"
                && path[3] == "branches")
            {
                const std::vector<std::string> session_path{
                    "system", "sessions", path[2]
                };
                auto* session_entry = objects_.Find(session_path);

                std::string target_hash;
                if (session_entry && session_entry->object)
                {
                    auto* session = dynamic_cast<ObjectManager::Session*>(
                        session_entry->object.get());
                    if (session)
                    {
                        auto it = session->branch_overrides.find(path[4]);
                        if (it != session->branch_overrides.end())
                            target_hash = it->second;
                    }
                }

                // No session-level override — defer to the global branch's HEAD.
                if (target_hash.empty())
                {
                    const std::vector<std::string> branch_path{
                        "system", "branches", path[4]
                    };
                    auto* bentry = objects_.Find(branch_path);
                    if (bentry && bentry->object)
                    {
                        auto* branch = dynamic_cast<ObjectManager::Branch*>(
                            bentry->object.get());
                        if (branch) target_hash = branch->target_hash;
                    }
                }

                if (target_hash.empty()) return path;

                std::vector<std::string> rewritten{
                    "system", "snapshots", target_hash
                };
                for (size_t i = 5; i < path.size(); ++i)
                    rewritten.push_back(path[i]);

                path = std::move(rewritten);
                continue;
            }

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
