// src/tools/workspace.cpp — Workspace::resolve (spec §4.4).
//
// A purely lexical check, steps 1–6 of the spec. It never touches the
// filesystem, so it works for paths that don't exist yet. The constructor,
// root() and check_symlinks() are provided in workspace_provided.cpp.

#include "tools/workspace.h"

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
    // 1. Empty path.
    if (user_path.empty()) {
        *reason = "empty path";
        return false;
    }

    // 2. An embedded NUL would silently truncate the path at the OS level.
    if (user_path.find('\0') != std::string_view::npos) {
        *reason = "path contains a NUL byte";
        return false;
    }

    try {
        // 3. Only workspace-relative paths are allowed.
        fs::path p{std::string(user_path)};
        if (p.is_absolute() || p.has_root_path()) {
            *reason = "absolute paths are not allowed: " + std::string(user_path);
            return false;
        }

        // 4. Fold ".", "//" and "x/.." lexically.
        fs::path n = p.lexically_normal();

        // 5. A leading ".." after normalization means the path climbs above root.
        if (n.begin() != n.end() && *n.begin() == "..") {
            *reason = "path escapes the workspace: " + std::string(user_path);
            return false;
        }

        // 6. Join onto root.
        if (n.empty() || n == ".") *out = root();
        else                       *out = root() / n;
        return true;
    } catch (...) {
        // path operations may allocate; never let an exception escape.
        *reason = "could not process path";
        return false;
    }
}
