#include "aiws/chunker.hpp"

#include "aiws/text_processor.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace aiws {

Chunker::Chunker(ChunkingPolicy policy)
    : policy_(policy) {

    if (policy_.max_tokens == 0 ||
        policy_.overlap >= policy_.max_tokens ||
        policy_.paragraph_window > policy_.max_tokens) {

        throw std::invalid_argument(
            "invalid chunking policy");
    }
}

std::vector<Chunk> Chunker::chunk(
    const Document& document,
    std::size_t document_order) const {

    std::vector<TokenInfo> tokens =
        TextProcessor::tokenize(document.text());

    std::vector<Chunk> result;

    if (tokens.empty()) {
        return result;
    }

    std::size_t start = 0;
    std::size_t sequence = 0;

    while (start < tokens.size()) {
        std::size_t remaining =
            tokens.size() - start;

        std::size_t end;

        if (remaining <= policy_.max_tokens) {
            end = tokens.size();
        } else {
            end = start + policy_.max_tokens;

            // Earliest preferred boundary.
            std::size_t preferred_offset =
                policy_.max_tokens -
                policy_.paragraph_window;

            // Make sure selecting a paragraph boundary can
            // still make forward progress after overlap.
            preferred_offset =
                std::max(preferred_offset,
                         policy_.overlap + 1);

            std::size_t lower =
                start + preferred_offset;

            // Choose the latest paragraph boundary inside
            // the preference window.
            for (std::size_t boundary = end;
                 boundary >= lower;
                 --boundary) {

                if (boundary > start &&
                    boundary < tokens.size() &&
                    tokens[boundary - 1].paragraph !=
                        tokens[boundary].paragraph) {

                    end = boundary;
                    break;
                }

                if (boundary == lower) {
                    break;
                }
            }
        }

        Chunk chunk;

        chunk.document_id = document.id();
        chunk.document_order = document_order;
        chunk.sequence = sequence;

        chunk.id =
            document.id() + "#" +
            std::to_string(sequence);

        chunk.text =
            TextProcessor::join(tokens, start, end);

        chunk.token_count = end - start;

        chunk.source_begin =
            tokens[start].begin;

        chunk.source_end =
            tokens[end - 1].end;

        result.push_back(std::move(chunk));

        ++sequence;

        if (end == tokens.size()) {
            break;
        }

        start = end - policy_.overlap;
    }

    return result;
}

}  // namespace aiws