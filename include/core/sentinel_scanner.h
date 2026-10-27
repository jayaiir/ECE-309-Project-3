#pragma once
#include <string>
#include <string_view>
#include <utility>
#include <algorithm>
#include <cstddef>

class SentinelScanner {
public:
    explicit SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)) {}

    struct Out { std::string safe_text; bool sentinel_found; };

    Out feed(std::string_view chunk) {
        // Step 1: glue the held back text and the new chunk together
        std::string text = pending_;
        text += chunk;

        // Step 2: look for the stop code
        std::size_t pos = text.find(sentinel_);

        // Step 3: FOUND IT -> everything before it is safe; stop
        if (pos != std::string::npos) {
            pending_.clear();
            return { text.substr(0, pos), true };
        }

        // Step 4: NOT FOUND -> hold back the tail, release the rest
        std::size_t keep = std::min(text.size(), sentinel_.size() - 1);  // how many to hold back
        std::size_t cut  = text.size() - keep;                           // where the tail begins

        pending_ = text.substr(cut);
        return { text.substr(0, cut), false };
    }

    // Flush any remaining pending characters
    Out flush() {
        std::string out = std::move(pending_);
        pending_.clear();
        return {out, false};
    }

    std::size_t pending_size() const noexcept { return pending_.size(); } // for tests

private:
    std::string sentinel_;   // the stop code
    std::string pending_;    // the held-back characters
};