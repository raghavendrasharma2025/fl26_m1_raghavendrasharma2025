#include "aiws/processing_core.hpp"

#include "aiws/chunker.hpp"
#include "aiws/context_builder.hpp"
#include "aiws/corpus_index.hpp"
#include "aiws/retrieval_engine.hpp"
#include "aiws/text_processor.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace aiws {

namespace {

std::string single_normalized_term(
    const std::string& term) {

    std::vector<std::string> terms =
        TextProcessor::terms(term);

    if (terms.empty()) {
        return {};
    }

    if (terms.size() > 1) {
        throw std::invalid_argument(
            "term must normalize to exactly one token");
    }

    return terms[0];
}

}  // namespace

struct ProcessingCore::Impl {
    Impl()
        : chunker(
              ChunkingPolicy{
                  ProcessingCore::kMaxChunkTokens,
                  ProcessingCore::kChunkOverlap,
                  ProcessingCore::
                      kParagraphPreferenceWindow}) {
    }

    Chunker chunker;
    std::vector<Chunk> chunks;
    CorpusIndex index;
    RetrievalEngine retrieval;
    ContextBuilder context_builder;
};

ProcessingCore::ProcessingCore()
    : impl_(std::make_unique<Impl>()) {
}

ProcessingCore::~ProcessingCore() = default;

ProcessingCore::ProcessingCore(
    ProcessingCore&&) noexcept = default;

ProcessingCore&
ProcessingCore::operator=(
    ProcessingCore&&) noexcept = default;

std::string ProcessingCore::normalize(
    const std::string& text) {

    return TextProcessor::normalize(text);
}

void ProcessingCore::rebuild(
    const Workspace& workspace) {

    // Build all replacement state separately first.
    // This guarantees the existing corpus remains intact
    // if rebuilding fails.
    std::vector<Chunk> new_chunks;

    std::unordered_set<std::string>
        document_ids;

    const std::vector<Document>& documents =
        workspace.documents();

    for (std::size_t order = 0;
         order < documents.size();
         ++order) {

        const Document& document =
            documents[order];

        if (!document_ids.insert(
                document.id()).second) {

            throw std::invalid_argument(
                "duplicate document ID");
        }

        std::vector<Chunk> produced =
            impl_->chunker.chunk(
                document,
                order);

        for (Chunk& chunk : produced) {
            new_chunks.push_back(
                std::move(chunk));
        }
    }

    // Build a brand-new index before modifying
    // the currently valid state.
    CorpusIndex new_index(new_chunks);

    // Only commit once every operation succeeds.
    impl_->chunks = std::move(new_chunks);
    impl_->index = std::move(new_index);
}

const std::vector<Chunk>&
ProcessingCore::chunks() const noexcept {

    return impl_->chunks;
}

std::size_t
ProcessingCore::chunk_count() const noexcept {

    return impl_->chunks.size();
}

std::size_t
ProcessingCore::document_frequency(
    const std::string& term) const {

    std::string normalized =
        single_normalized_term(term);

    if (normalized.empty()) {
        return 0;
    }

    return impl_->index.document_frequency(
        normalized);
}

std::size_t
ProcessingCore::term_frequency(
    const std::string& term,
    const std::string& chunk_id) const {

    std::string normalized =
        single_normalized_term(term);

    if (normalized.empty()) {
        return 0;
    }

    return impl_->index.term_frequency(
        normalized,
        chunk_id);
}

std::vector<SearchResult>
ProcessingCore::search(
    const std::string& query,
    int k) const {

    return impl_->retrieval.search(
        query,
        k,
        impl_->chunks,
        impl_->index);
}

std::vector<ContextItem>
ProcessingCore::build_context(
    const std::string& query,
    int k,
    std::size_t token_budget) const {

    std::vector<SearchResult> ranked =
        impl_->retrieval.search(
            query,
            k,
            impl_->chunks,
            impl_->index);

    return impl_->context_builder.build(
        ranked,
        token_budget);
}

}  // namespace aiws