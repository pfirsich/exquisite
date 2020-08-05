#pragma once

#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>

#include "tree_sitter/api.h"

namespace ts {
enum class InputEncoding : std::underlying_type_t<TSInputEncoding> {
    Utf8 = TSInputEncodingUTF8,
    Utf16 = TSInputEncodingUTF16,
};

enum class LogType : std::underlying_type_t<TSLogType> {
    Parse = TSLogTypeParse,
    Lex = TSLogTypeLex,
};

enum class QueryError : std::underlying_type_t<TSQueryError> {
    None = TSQueryErrorNone,
    Syntax = TSQueryErrorSyntax,
    NodeType = TSQueryErrorNodeType,
    Field = TSQueryErrorField,
    Capture = TSQueryErrorCapture,
};

class Tree {
public:
    Tree(TSTree* tree);

    Tree(const Tree&) = delete;
    Tree& operator=(const Tree&) = delete;

    Tree(Tree&& other);
    Tree& operator=(Tree&& other);

    ~Tree();

    const TSTree* get() const;

    // I am not using the copy constructor for now, because I want this to be explicit
    std::unique_ptr<Tree> copy() const;

    TSNode getRootNode() const; // TODO: Node!

    const TSLanguage* getLanguage() const;

    void edit(const TSInputEdit& edit);

    TSTree* release();

private:
    TSTree* tree_;
};

class Query {
public:
    Query(const TSLanguage* language, std::string_view source);

    Query(const Query&) = delete;
    Query& operator=(const Query&) = delete;

    Query(Query&& other);
    Query& operator=(Query&& other);

    ~Query();

    TSQuery* release();

    const TSQuery* get() const;

    std::pair<size_t, QueryError> getError() const;

    size_t getPatternCount() const;
    size_t getCaptureCount() const;
    size_t getStringCount() const;

    std::string_view getCaptureNameForId(size_t id) const;

private:
    TSQuery* query_;
    uint32_t errorOffset_;
    TSQueryError queryError_;
};

class QueryCursor {
public:
    QueryCursor();

    QueryCursor(const QueryCursor&) = delete;
    QueryCursor& operator=(const QueryCursor&) = delete;

    QueryCursor(QueryCursor&& other);

    QueryCursor& operator=(QueryCursor&& other);

    ~QueryCursor();

    TSQueryCursor* release();

    void exec(const Query& query, TSNode node);

    void setByteRange(size_t start, size_t end);

    bool getNextMatch(TSQueryMatch* match);

private:
    TSQueryCursor* cursor_;
};

class Parser {
public:
    Parser();

    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;

    Parser(Parser&& other);
    Parser& operator=(Parser&& other);

    ~Parser();

    TSParser* release();

    bool setLanguage(const TSLanguage* language);

    bool setIncludedRange(const TSRange* ranges, size_t length);
    std::pair<const TSRange*, size_t> getIncludedRanges() const;

    using ReadCallback = std::pair<const char*, size_t>(size_t byteIndex, TSPoint position);

    std::unique_ptr<Tree> parse(
        const Tree* oldTree, std::function<ReadCallback> callback, InputEncoding encoding);

    std::unique_ptr<Tree> parseString(const Tree* oldTree, std::string_view string);

    std::unique_ptr<Tree> parseString(
        const Tree* oldTree, std::string_view string, InputEncoding encoding);

    void reset();

    void setTimeoutMicros(uint64_t timeout);
    uint64_t getTimeoutMicros() const;

    void setCancellationFlag(const size_t* flag);
    const size_t* getCancellationFlag() const;

    using LogCallback = void(LogType, const char* message);

    void setLogger(std::function<LogCallback> callback);

    std::function<LogCallback> getLogger() const;

private:
    static const char* readCallback(
        void* payload, uint32_t byteIndex, TSPoint position, uint32_t* bytesRead);

    static void logCallback(void* payload, TSLogType logType, const char* message);

    TSParser* parser_;
    std::function<ReadCallback> readCallback_ = nullptr;
    std::function<LogCallback> logCallback_ = nullptr;
};
}
