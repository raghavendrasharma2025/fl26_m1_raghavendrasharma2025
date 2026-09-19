#include "aiws/context_builder.hpp"

#include "aiws/text_processor.hpp"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace aiws {

std::vector<ContextItem>
ContextBuilder::build(
    const std::vector<SearchResult>& ranked,
    std::size_t token_budget) const {

    std::vector<ContextItem> result;

    if (token_budget == 0) {
        return result;
    }

    std::unordered_set<std::string>
        used_chunks;

    std::size_t used_tokens = 0;

    for (const SearchResult& search_result :
         ranked) {

        // Do not add the same chunk more than once.
        if (!used_chunks.insert(
                search_result.chunk_id).second) {
            continue;
        }

        std::vector<std::string> tokens =
            TextProcessor::terms(
                search_result.text);

        if (tokens.empty()) {
            continue;
        }

        std::size_t remaining =
            token_budget - used_tokens;

        if (remaining == 0) {
            break;
        }

        ContextItem item;

        item.chunk_id =
            search_result.chunk_id;

        item.document_id =
            search_result.document_id;

        item.chunk_sequence =
            search_result.chunk_sequence;

        item.score =
            search_result.score;

        if (tokens.size() <= remaining) {
            item.text =
                search_result.text;

            item.token_count =
                tokens.size();

            item.truncated = false;

            used_tokens +=
                tokens.size();

            result.push_back(
                std::move(item));
        } else {
            item.text =
                TextProcessor::join(
                    tokens,
                    0,
                    remaining);

            item.token_count =
                remaining;

            item.truncated = true;

            result.push_back(
                std::move(item));

            // The specification says once a partial chunk
            // is included, context construction stops.
            break;
        }
    }

    return result;
}

}  // namespace aiws