# M1 DESIGN.md

Replace this template with your own concise engineering explanation.

## 1. System structure
The M1 subsystem is divided into several components. TextProcessor handles normalization and tokenization, Chunker divides documents into overlapping chunks, CorpusIndex stores term frequencies, RetrievalEngine ranks matching chunks, and ContextBuilder creates bounded context. ProcessingCore coordinates these components and provides the required public interface.

## 2. Design decisions
The implementation uses vectors to store chunks and postings, unordered maps for fast term and chunk lookups, and small classes with separate responsibilities. ProcessingCore owns the main state through its private Impl structure. This design keeps the public interface simple while separating processing, indexing, retrieval, and context construction.

## 3. Correctness and consistency
Document and query text use the same normalization rules so indexed terms match searched terms consistently. Chunk IDs and ordering remain deterministic. Rebuild creates new chunks and a new index before replacing the existing state, so duplicate document IDs or other failures cannot partially corrupt a previously valid corpus.

## 4. Testing strategy
The tests cover normalization, empty input, paragraph detection, chunk sizes, overlap, source information, term and document frequency, rebuild behavior, duplicate IDs, ranking, tie ordering, invalid inputs, and context budgets. I also included direct component tests and a multi-document end-to-end test to check interactions between subsystems.

## 5. Alternatives considered
One alternative was putting all processing logic directly inside ProcessingCore, but this would make the code harder to maintain and test. Another option was rescanning every chunk during each search instead of building an index. I chose separate components and a persistent index because they provide clearer responsibilities and more efficient retrieval.
