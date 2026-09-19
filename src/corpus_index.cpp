#include "aiws/corpus_index.hpp"

#include "aiws/text_processor.hpp"

#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace aiws {

CorpusIndex::CorpusIndex(
    const std::vector<Chunk>& chunks) {

    build(chunks);
}

void CorpusIndex::build(
    const std::vector<Chunk>& chunks) {

    std::unordered_map<
        std::string,
        std::vector<Posting>> new_postings;

    std::unordered_map<
        std::string,
        std::size_t> new_chunk_by_id;

    for (std::size_t i = 0;
         i < chunks.size();
         ++i) {

        const Chunk& chunk = chunks[i];

        auto inserted =
            new_chunk_by_id.emplace(chunk.id, i);

        if (!inserted.second) {
            throw std::invalid_argument(
                "duplicate chunk ID");
        }

        std::vector<std::string> terms =
            TextProcessor::terms(chunk.text);

        std::unordered_map<
            std::string,
            std::size_t> frequencies;

        for (const std::string& term : terms) {
            ++frequencies[term];
        }

        for (const auto& entry : frequencies) {
            new_postings[entry.first].push_back(
                Posting{i, entry.second});
        }
    }

    postings_ = std::move(new_postings);
    chunk_by_id_ = std::move(new_chunk_by_id);
}

std::size_t CorpusIndex::document_frequency(
    const std::string& normalized_term) const noexcept {

    auto it = postings_.find(normalized_term);

    if (it == postings_.end()) {
        return 0;
    }

    return it->second.size();
}

std::size_t CorpusIndex::term_frequency(
    const std::string& normalized_term,
    const std::string& chunk_id) const noexcept {

    auto chunk_it = chunk_by_id_.find(chunk_id);

    if (chunk_it == chunk_by_id_.end()) {
        return 0;
    }

    auto posting_it =
        postings_.find(normalized_term);

    if (posting_it == postings_.end()) {
        return 0;
    }

    std::size_t wanted_index =
        chunk_it->second;

    for (const Posting& posting :
         posting_it->second) {

        if (posting.chunk_index ==
            wanted_index) {

            return posting.frequency;
        }
    }

    return 0;
}

const std::vector<CorpusIndex::Posting>*
CorpusIndex::postings(
    const std::string& normalized_term) const noexcept {

    auto it = postings_.find(normalized_term);

    if (it == postings_.end()) {
        return nullptr;
    }

    return &it->second;
}

const Chunk* CorpusIndex::find_chunk(
    const std::vector<Chunk>& chunks,
    const std::string& chunk_id) const noexcept {

    auto it = chunk_by_id_.find(chunk_id);

    if (it == chunk_by_id_.end()) {
        return nullptr;
    }

    if (it->second >= chunks.size()) {
        return nullptr;
    }

    return &chunks[it->second];
}

std::size_t CorpusIndex::chunk_index(
    const std::string& chunk_id) const {

    auto it = chunk_by_id_.find(chunk_id);

    if (it == chunk_by_id_.end()) {
        throw std::out_of_range(
            "unknown chunk ID");
    }

    return it->second;
}

}  // namespace aiws