#include "aiws/retrieval_engine.hpp"

#include "aiws/text_processor.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace aiws {

double RetrievalEngine::canonical_score(
    double value) {

    constexpr double scale = 1e12;

    return std::round(value * scale) / scale;
}

std::vector<SearchResult>
RetrievalEngine::search(
    const std::string& query,
    int k,
    const std::vector<Chunk>& chunks,
    const CorpusIndex& index) const {

    if (k < 0) {
        throw std::invalid_argument(
            "k cannot be negative");
    }

    if (k == 0) {
        return {};
    }

    std::vector<std::string> all_terms =
        TextProcessor::terms(query);

    if (all_terms.empty()) {
        return {};
    }

    // Keep only unique query terms while preserving
    // their original order.
    std::vector<std::string> query_terms;
    std::unordered_set<std::string> seen;

    for (const std::string& term : all_terms) {
        if (seen.insert(term).second) {
            query_terms.push_back(term);
        }
    }

    if (query_terms.empty()) {
        return {};
    }

    struct Accumulator {
        double base_score{0.0};
        std::size_t matched_terms{0};
    };

    std::vector<Accumulator> accumulators(
        chunks.size());

    const double N =
        static_cast<double>(chunks.size());

    for (const std::string& term :
         query_terms) {

        const std::vector<
            CorpusIndex::Posting>* postings =
                index.postings(term);

        if (postings == nullptr) {
            continue;
        }

        double df =
            static_cast<double>(
                postings->size());

        double idf =
            std::log((N + 1.0) /
                     (df + 1.0)) +
            1.0;

        for (const CorpusIndex::Posting&
             posting : *postings) {

            if (posting.chunk_index >=
                chunks.size()) {
                continue;
            }

            double frequency =
                static_cast<double>(
                    posting.frequency);

            double tf =
                1.0 + std::log(frequency);

            Accumulator& acc =
                accumulators[
                    posting.chunk_index];

            acc.base_score += tf * idf;
            ++acc.matched_terms;
        }
    }

    std::vector<SearchResult> results;

    const double Q =
        static_cast<double>(
            query_terms.size());

    for (std::size_t i = 0;
         i < chunks.size();
         ++i) {

        const Accumulator& acc =
            accumulators[i];

        if (acc.matched_terms == 0) {
            continue;
        }

        double matched =
            static_cast<double>(
                acc.matched_terms);

        double coverage =
            1.0 +
            0.10 * matched / Q;

        double score =
            canonical_score(
                acc.base_score *
                coverage);

        const Chunk& chunk = chunks[i];

        SearchResult result;

        result.chunk_id = chunk.id;
        result.document_id =
            chunk.document_id;
        result.chunk_sequence =
            chunk.sequence;
        result.text = chunk.text;
        result.score = score;
        result.matched_terms =
            acc.matched_terms;

        results.push_back(
            std::move(result));
    }

    std::sort(
        results.begin(),
        results.end(),
        [&chunks, &index](
            const SearchResult& a,
            const SearchResult& b) {

            if (a.score != b.score) {
                return a.score > b.score;
            }

            const Chunk* chunk_a =
                index.find_chunk(
                    chunks,
                    a.chunk_id);

            const Chunk* chunk_b =
                index.find_chunk(
                    chunks,
                    b.chunk_id);

            if (chunk_a != nullptr &&
                chunk_b != nullptr) {

                if (chunk_a->document_order !=
                    chunk_b->document_order) {

                    return
                        chunk_a->document_order <
                        chunk_b->document_order;
                }

                if (chunk_a->sequence !=
                    chunk_b->sequence) {

                    return
                        chunk_a->sequence <
                        chunk_b->sequence;
                }
            }

            // Defensive final fallback.
            return a.chunk_id < b.chunk_id;
        });

    if (results.size() >
        static_cast<std::size_t>(k)) {

        results.resize(
            static_cast<std::size_t>(k));
    }

    return results;
}

}  // namespace aiws