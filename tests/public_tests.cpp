#include "aiws/chunker.hpp"
#include "aiws/context_builder.hpp"
#include "aiws/corpus_index.hpp"
#include "aiws/processing_core.hpp"
#include "aiws/retrieval_engine.hpp"
#include "aiws/text_processor.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    } else {
        std::cout << "PASS: " << message << '\n';
    }
}

std::string numbered_words(int n) {
    std::string result;

    for (int i = 0; i < n; ++i) {
        if (!result.empty()) {
            result += ' ';
        }

        result += "w" + std::to_string(i);
    }

    return result;
}

std::string word_range(int begin, int end) {
    std::string result;

    for (int i = begin; i < end; ++i) {
        if (!result.empty()) {
            result += ' ';
        }

        result += "w" + std::to_string(i);
    }

    return result;
}

bool approximately_equal(double a, double b) {
    return std::fabs(a - b) < 1e-10;
}

}  // namespace

int main() {
    using namespace aiws;

    std::cout << "\n=== TEXT PROCESSING TESTS ===\n";

    check(
        ProcessingCore::normalize("Hello,  WORLD! 2026") ==
            "hello world 2026",
        "normalization converts ASCII letters to lowercase");

    check(
        ProcessingCore::normalize("...\t---").empty(),
        "punctuation-only input becomes empty");

    check(
        ProcessingCore::normalize("R2-D2") == "r2 d2",
        "punctuation acts as separator");

    check(
        ProcessingCore::normalize("a___b...c") == "a b c",
        "consecutive separators do not create empty tokens");

    check(
        ProcessingCore::normalize("") == "",
        "empty input remains empty");

    {
        std::vector<std::string> terms =
            TextProcessor::terms("One, TWO! three3");

        check(
            terms.size() == 3 &&
                terms[0] == "one" &&
                terms[1] == "two" &&
                terms[2] == "three3",
            "TextProcessor returns normalized terms");
    }

    {
        std::vector<TokenInfo> tokens =
            TextProcessor::tokenize(
                "alpha beta\n\n"
                "gamma delta");

        check(
            tokens.size() == 4,
            "tokenize finds all tokens across paragraphs");

        check(
            tokens.size() == 4 &&
                tokens[0].paragraph == 0 &&
                tokens[1].paragraph == 0 &&
                tokens[2].paragraph == 1 &&
                tokens[3].paragraph == 1,
            "LF blank line creates new paragraph");
    }

    {
        std::vector<TokenInfo> tokens =
            TextProcessor::tokenize(
                "alpha beta\r\n\t \r\n"
                "gamma delta");

        check(
            tokens.size() == 4 &&
                tokens[0].paragraph == 0 &&
                tokens[1].paragraph == 0 &&
                tokens[2].paragraph == 1 &&
                tokens[3].paragraph == 1,
            "CRLF blank line is handled consistently");
    }


    std::cout << "\n=== CHUNKER TESTS ===\n";

    {
        Document short_doc{
            "short",
            "Short",
            "alpha beta gamma"
        };

        Chunker chunker;
        std::vector<Chunk> chunks =
            chunker.chunk(short_doc, 0);

        check(
            chunks.size() == 1,
            "short document creates one chunk");

        check(
            chunks.size() == 1 &&
                chunks[0].id == "short#0" &&
                chunks[0].document_id == "short" &&
                chunks[0].sequence == 0,
            "chunk identity and sequence are correct");

        check(
            chunks.size() == 1 &&
                chunks[0].text == "alpha beta gamma" &&
                chunks[0].token_count == 3,
            "short chunk stores normalized text and token count");

        check(
            chunks.size() == 1 &&
                chunks[0].source_begin == 0 &&
                chunks[0].source_end == short_doc.text().size(),
            "source span points into original document");
    }

    {
        Document empty_doc{
            "empty",
            "Empty",
            "... !!! ---"
        };

        Chunker chunker;
        std::vector<Chunk> chunks =
            chunker.chunk(empty_doc, 0);

        check(
            chunks.empty(),
            "effectively empty document creates no chunks");
    }

    {
        Document long_doc{
            "long",
            "Long",
            numbered_words(121)
        };

        Chunker chunker;
        std::vector<Chunk> chunks =
            chunker.chunk(long_doc, 0);

        check(
            chunks.size() == 2,
            "121-token document produces two chunks");

        check(
            chunks.size() == 2 &&
                chunks[0].token_count == 120 &&
                chunks[1].token_count == 21,
            "hard 120-token limit and 20-token overlap work");

        check(
            chunks.size() == 2 &&
                chunks[0].sequence == 0 &&
                chunks[1].sequence == 1 &&
                chunks[0].id == "long#0" &&
                chunks[1].id == "long#1",
            "chunk sequence numbers and IDs are deterministic");

        check(
            chunks.size() == 2 &&
                chunks[1].text.rfind("w100 ", 0) == 0,
            "second chunk begins with 20-token overlap");
    }

    {
        std::string paragraph_doc =
            word_range(0, 105) +
            "\n\n" +
            word_range(105, 135);

        Document doc{
            "paragraph",
            "Paragraph",
            paragraph_doc
        };

        Chunker chunker;
        std::vector<Chunk> chunks =
            chunker.chunk(doc, 0);

        check(
            chunks.size() == 2,
            "paragraph-aware long document creates expected chunks");

        check(
            chunks.size() == 2 &&
                chunks[0].token_count == 105,
            "chunk prefers latest paragraph boundary in 100-120 window");

        check(
            chunks.size() == 2 &&
                chunks[1].token_count == 50,
            "overlap is measured from selected paragraph boundary");

        check(
            chunks.size() == 2 &&
                chunks[1].text.rfind("w85 ", 0) == 0,
            "paragraph boundary chunk still preserves 20-token overlap");
    }


    std::cout << "\n=== CORPUS INDEX TESTS ===\n";

    std::vector<Chunk> index_chunks;

    {
        Document d1{
            "d1",
            "One",
            "alpha alpha beta"
        };

        Document d2{
            "d2",
            "Two",
            "alpha gamma"
        };

        Chunker chunker;

        std::vector<Chunk> a =
            chunker.chunk(d1, 0);

        std::vector<Chunk> b =
            chunker.chunk(d2, 1);

        index_chunks.insert(
            index_chunks.end(),
            a.begin(),
            a.end());

        index_chunks.insert(
            index_chunks.end(),
            b.begin(),
            b.end());
    }

    CorpusIndex direct_index(index_chunks);

    check(
        direct_index.document_frequency("alpha") == 2,
        "CorpusIndex DF counts chunks containing term");

    check(
        direct_index.document_frequency("beta") == 1,
        "CorpusIndex DF handles single-chunk term");

    check(
        direct_index.document_frequency("missing") == 0,
        "CorpusIndex DF returns zero for missing term");

    check(
        direct_index.term_frequency("alpha", "d1#0") == 2,
        "CorpusIndex preserves term frequency");

    check(
        direct_index.term_frequency("alpha", "d2#0") == 1,
        "CorpusIndex stores independent chunk frequencies");

    check(
        direct_index.term_frequency(
            "alpha",
            "does-not-exist") == 0,
        "missing chunk ID has zero term frequency");

    check(
        direct_index.find_chunk(
            index_chunks,
            "d2#0") != nullptr,
        "CorpusIndex can find indexed chunk by ID");


    std::cout << "\n=== PROCESSING CORE INDEX API TESTS ===\n";

    Workspace ws;

    ws.add_document(
        Document{
            "d1",
            "One",
            "alpha alpha beta"
        });

    ws.add_document(
        Document{
            "d2",
            "Two",
            "alpha gamma"
        });

    ProcessingCore core;
    core.rebuild(ws);

    check(
        core.chunk_count() == 2,
        "one short chunk is created per document");

    check(
        core.document_frequency("ALPHA!") == 2,
        "DF argument is normalized");

    check(
        core.term_frequency(
            "ALPHA!",
            "d1#0") == 2,
        "TF argument is normalized");

    check(
        core.document_frequency("!!!") == 0,
        "term normalizing to zero tokens returns zero");

    {
        bool threw = false;

        try {
            (void)core.document_frequency(
                "alpha beta");
        } catch (const std::invalid_argument&) {
            threw = true;
        }

        check(
            threw,
            "multi-token DF term throws invalid_argument");
    }

    {
        bool threw = false;

        try {
            (void)core.term_frequency(
                "alpha beta",
                "d1#0");
        } catch (const std::invalid_argument&) {
            threw = true;
        }

        check(
            threw,
            "multi-token TF term throws invalid_argument");
    }


    std::cout << "\n=== REBUILD TESTS ===\n";

    {
        Workspace replacement;

        replacement.add_document(
            Document{
                "new",
                "New",
                "delta epsilon"
            });

        core.rebuild(replacement);

        check(
            core.chunk_count() == 1,
            "rebuild replaces previous chunks");

        check(
            core.document_frequency("alpha") == 0,
            "rebuild removes stale term information");

        check(
            core.document_frequency("delta") == 1,
            "rebuild installs new corpus state");

        core.rebuild(replacement);

        check(
            core.chunk_count() == 1 &&
                core.document_frequency("delta") == 1,
            "repeated rebuild does not accumulate duplicate state");
    }

    {
        Workspace valid;

        valid.add_document(
            Document{
                "keep",
                "Keep",
                "preserve this corpus"
            });

        core.rebuild(valid);

        Workspace duplicate;

        duplicate.add_document(
            Document{
                "same",
                "First",
                "alpha"
            });

        duplicate.add_document(
            Document{
                "same",
                "Second",
                "beta"
            });

        bool threw = false;

        try {
            core.rebuild(duplicate);
        } catch (const std::invalid_argument&) {
            threw = true;
        }

        check(
            threw,
            "duplicate document IDs throw invalid_argument");

        check(
            core.chunk_count() == 1 &&
                core.document_frequency("preserve") == 1,
            "failed rebuild leaves previous valid corpus unchanged");

        check(
            core.document_frequency("alpha") == 0 &&
                core.document_frequency("beta") == 0,
            "failed rebuild does not partially install new state");
    }


    std::cout << "\n=== RETRIEVAL TESTS ===\n";

    Workspace search_ws;

    search_ws.add_document(
        Document{
            "first",
            "First",
            "alpha beta"
        });

    search_ws.add_document(
        Document{
            "second",
            "Second",
            "alpha gamma"
        });

    search_ws.add_document(
        Document{
            "third",
            "Third",
            "beta beta beta"
        });

    core.rebuild(search_ws);

    {
        std::vector<SearchResult> results =
            core.search("alpha beta", 10);

        check(
            results.size() == 3,
            "candidate set is union of chunks matching any query term");

        check(
            !results.empty() &&
                results[0].document_id == "first",
            "chunk matching both query terms ranks above partial matches");

        check(
            !results.empty() &&
                results[0].matched_terms == 2,
            "matched_terms stores number of distinct matched query terms");
    }

    {
        std::vector<SearchResult> once =
            core.search("alpha beta", 10);

        std::vector<SearchResult> repeated =
            core.search(
                "alpha alpha alpha beta beta",
                10);

        bool same = once.size() == repeated.size();

        if (same) {
            for (std::size_t i = 0;
                 i < once.size();
                 ++i) {

                if (once[i].chunk_id !=
                        repeated[i].chunk_id ||
                    !approximately_equal(
                        once[i].score,
                        repeated[i].score)) {

                    same = false;
                    break;
                }
            }
        }

        check(
            same,
            "repeated query terms do not change ranking or score");
    }

    {
        check(
            core.search("!!!", 10).empty(),
            "effectively empty query returns no results");

        check(
            core.search("unknownterm", 10).empty(),
            "unknown query terms return no candidates");

        check(
            core.search("alpha", 0).empty(),
            "k == 0 returns no results");

        bool negative_threw = false;

        try {
            (void)core.search("alpha", -1);
        } catch (const std::invalid_argument&) {
            negative_threw = true;
        }

        check(
            negative_threw,
            "negative k throws invalid_argument");
    }

    {
        Workspace tie_ws;

        tie_ws.add_document(
            Document{
                "earlier",
                "Earlier",
                "same"
            });

        tie_ws.add_document(
            Document{
                "later",
                "Later",
                "same"
            });

        core.rebuild(tie_ws);

        std::vector<SearchResult> results =
            core.search("same", 10);

        check(
            results.size() == 2 &&
                approximately_equal(
                    results[0].score,
                    results[1].score),
            "tie test creates equal scores");

        check(
            results.size() == 2 &&
                results[0].document_id == "earlier" &&
                results[1].document_id == "later",
            "equal scores break ties by document insertion order");
    }


    std::cout << "\n=== DIRECT RETRIEVAL ENGINE TEST ===\n";

    {
        RetrievalEngine engine;

        std::vector<SearchResult> results =
            engine.search(
                "alpha beta",
                10,
                index_chunks,
                direct_index);

        check(
            results.size() == 2,
            "RetrievalEngine works directly with CorpusIndex");

        check(
            results.size() == 2 &&
                results[0].chunk_id == "d1#0",
            "direct retrieval ranks coverage/frequency correctly");
    }


    std::cout << "\n=== CONTEXT BUILDER TESTS ===\n";

    {
        SearchResult result;

        result.chunk_id = "c#0";
        result.document_id = "c";
        result.chunk_sequence = 0;
        result.text = "one two three four";
        result.score = 3.5;
        result.matched_terms = 1;

        std::vector<SearchResult> ranked{
            result
        };

        ContextBuilder builder;

        std::vector<ContextItem> context =
            builder.build(ranked, 2);

        check(
            context.size() == 1,
            "ContextBuilder includes truncated ranked chunk");

        check(
            context.size() == 1 &&
                context[0].text == "one two" &&
                context[0].token_count == 2 &&
                context[0].truncated,
            "ContextBuilder truncates to largest fitting token prefix");

        check(
            builder.build(ranked, 0).empty(),
            "zero context budget returns no items");
    }

    {
        SearchResult a;
        a.chunk_id = "a#0";
        a.document_id = "a";
        a.chunk_sequence = 0;
        a.text = "one two";
        a.score = 5.0;

        SearchResult b;
        b.chunk_id = "b#0";
        b.document_id = "b";
        b.chunk_sequence = 0;
        b.text = "three four five";
        b.score = 4.0;

        ContextBuilder builder;

        std::vector<ContextItem> context =
            builder.build(
                std::vector<SearchResult>{a, b},
                4);

        check(
            context.size() == 2,
            "context preserves ranked order across multiple items");

        check(
            context.size() == 2 &&
                context[0].text == "one two" &&
                !context[0].truncated &&
                context[1].text == "three four" &&
                context[1].truncated,
            "context includes full chunks then truncates final fitting chunk");

        check(
            context.size() == 2 &&
                context[0].token_count +
                    context[1].token_count <= 4,
            "context never exceeds token budget");
    }


    std::cout << "\n=== END-TO-END TEST ===\n";

    {
        Workspace end_to_end;

        end_to_end.add_document(
            Document{
                "ai",
                "AI",
                "Machine learning systems use data. "
                "Retrieval systems find useful information."
            });

        end_to_end.add_document(
            Document{
                "circuits",
                "Circuits",
                "Electrical circuits contain resistors "
                "capacitors and inductors."
            });

        end_to_end.add_document(
            Document{
                "retrieval",
                "Retrieval",
                "Search and retrieval rank relevant "
                "documents for a query."
            });

        core.rebuild(end_to_end);

        std::vector<SearchResult> results =
            core.search(
                "retrieval systems",
                5);

        check(
            !results.empty(),
            "multi-document end-to-end search returns results");

        check(
            !results.empty() &&
                results[0].document_id == "ai",
            "end-to-end ranking prefers chunk matching both terms");

        std::vector<ContextItem> context =
            core.build_context(
                "retrieval systems",
                5,
                6);

        std::size_t total_tokens = 0;

        for (const ContextItem& item :
             context) {
            total_tokens += item.token_count;
        }

        check(
            total_tokens <= 6,
            "end-to-end context respects caller token budget");

        check(
            !context.empty() &&
                context[0].document_id == "ai",
            "context preserves retrieval source attribution");
    }


    std::cout << "\n============================\n";

    if (failures == 0) {
        std::cout
            << "ALL TESTS PASSED.\n";

        return 0;
    }

    std::cerr
        << failures
        << " test(s) failed.\n";

    return 1;
}