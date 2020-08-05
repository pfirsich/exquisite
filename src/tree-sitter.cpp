#include "tree-sitter.hpp"

namespace ts {
Tree::Tree(TSTree* tree)
    : tree_(tree)
{
}

Tree::Tree(Tree&& other)
    : tree_(other.release())
{
}

Tree& Tree::operator=(Tree&& other)
{
    tree_ = other.release();
    return *this;
}

Tree::~Tree()
{
    if (tree_)
        ts_tree_delete(tree_);
}

const TSTree* Tree::get() const
{
    return tree_;
}

std::unique_ptr<Tree> Tree::copy() const
{
    return std::make_unique<Tree>(ts_tree_copy(tree_));
}

TSNode Tree::getRootNode() const // TODO: Node!
{
    return ts_tree_root_node(tree_);
}

const TSLanguage* Tree::getLanguage() const
{
    return ts_tree_language(tree_);
}

void Tree::edit(const TSInputEdit& edit)
{
    ts_tree_edit(tree_, &edit);
}

TSTree* Tree::release()
{
    const auto tree = tree_;
    tree_ = nullptr;
    return tree;
}

Query::Query(const TSLanguage* language, std::string_view source)
    : query_(ts_query_new(language, source.data(), source.size(), &errorOffset_, &queryError_))
{
}

Query::Query(Query&& other)
    : query_(other.release())
{
}

Query& Query::operator=(Query&& other)
{
    query_ = other.release();
    return *this;
}

Query::~Query()
{
    if (query_)
        ts_query_delete(query_);
}

TSQuery* Query::release()
{
    auto query = query_;
    query_ = nullptr;
    return query;
}

const TSQuery* Query::get() const
{
    return query_;
}

std::pair<size_t, QueryError> Query::getError() const
{
    return std::pair(errorOffset_, static_cast<QueryError>(queryError_));
}

size_t Query::getPatternCount() const
{
    return ts_query_pattern_count(query_);
}

size_t Query::getCaptureCount() const
{
    return ts_query_capture_count(query_);
}

size_t Query::getStringCount() const
{
    return ts_query_string_count(query_);
}

std::string_view Query::getCaptureNameForId(size_t id) const
{
    uint32_t length;
    const auto ptr = ts_query_capture_name_for_id(query_, id, &length);
    return std::string_view(ptr, length);
}

QueryCursor::QueryCursor()
    : cursor_(ts_query_cursor_new())
{
}

QueryCursor::QueryCursor(QueryCursor&& other)
    : cursor_(other.release())
{
}

QueryCursor& QueryCursor::operator=(QueryCursor&& other)
{
    cursor_ = other.release();
    return *this;
}

QueryCursor::~QueryCursor()
{
    if (cursor_)
        ts_query_cursor_delete(cursor_);
}

TSQueryCursor* QueryCursor::release()
{
    auto cursor = cursor_;
    cursor_ = nullptr;
    return cursor;
}

void QueryCursor::exec(const Query& query, TSNode node)
{
    ts_query_cursor_exec(cursor_, query.get(), node);
}

void QueryCursor::setByteRange(size_t start, size_t end)
{
    ts_query_cursor_set_byte_range(cursor_, start, end);
}

bool QueryCursor::getNextMatch(TSQueryMatch* match)
{
    return ts_query_cursor_next_match(cursor_, match);
}

Parser::Parser()
    : parser_(ts_parser_new())
{
}

Parser::Parser(Parser&& other)
    : parser_(other.release())
{
}

Parser& Parser::operator=(Parser&& other)
{
    parser_ = other.release();
    return *this;
}

Parser::~Parser()
{
    if (parser_)
        ts_parser_delete(parser_);
}

TSParser* Parser::release()
{
    auto parser = parser_;
    parser_ = nullptr;
    return parser;
}

bool Parser::setLanguage(const TSLanguage* language)
{
    return ts_parser_set_language(parser_, language);
}

bool Parser::setIncludedRange(const TSRange* ranges, size_t length)
{
    return ts_parser_set_included_ranges(parser_, ranges, length);
}

std::pair<const TSRange*, size_t> Parser::getIncludedRanges() const
{
    uint32_t length;
    const auto ranges = ts_parser_included_ranges(parser_, &length);
    return std::pair(ranges, length);
}

std::unique_ptr<Tree> Parser::parse(
    const Tree* oldTree, std::function<ReadCallback> callback, InputEncoding encoding)
{
    readCallback_ = callback;
    return std::make_unique<Tree>(ts_parser_parse(parser_, oldTree ? oldTree->get() : nullptr,
        TSInput { this, readCallback, static_cast<TSInputEncoding>(encoding) }));
}

std::unique_ptr<Tree> Parser::parseString(const Tree* oldTree, std::string_view string)
{
    return std::make_unique<Tree>(ts_parser_parse_string(
        parser_, oldTree ? oldTree->get() : nullptr, string.data(), string.size()));
}

std::unique_ptr<Tree> Parser::parseString(
    const Tree* oldTree, std::string_view string, InputEncoding encoding)
{
    return std::make_unique<Tree>(
        ts_parser_parse_string_encoding(parser_, oldTree ? oldTree->get() : nullptr, string.data(),
            string.size(), static_cast<TSInputEncoding>(encoding)));
}

void Parser::reset()
{
    ts_parser_reset(parser_);
}

void Parser::setTimeoutMicros(uint64_t timeout)
{
    ts_parser_set_timeout_micros(parser_, timeout);
}

uint64_t Parser::getTimeoutMicros() const
{
    return ts_parser_timeout_micros(parser_);
}

void Parser::setCancellationFlag(const size_t* flag)
{
    ts_parser_set_cancellation_flag(parser_, flag);
}

const size_t* Parser::getCancellationFlag() const
{
    return ts_parser_cancellation_flag(parser_);
}

void Parser::setLogger(std::function<LogCallback> callback)
{
    logCallback_ = callback;
    ts_parser_set_logger(parser_, TSLogger { this, logCallback });
}

std::function<Parser::LogCallback> Parser::getLogger() const
{
    return logCallback_;
}

const char* Parser::readCallback(
    void* payload, uint32_t byteIndex, TSPoint position, uint32_t* bytesRead)
{
    const auto res = reinterpret_cast<Parser*>(payload)->readCallback_(byteIndex, position);
    *bytesRead = res.second;
    return res.first;
}

void Parser::logCallback(void* payload, TSLogType logType, const char* message)
{
    reinterpret_cast<Parser*>(payload)->logCallback_(static_cast<LogType>(logType), message);
}
}
