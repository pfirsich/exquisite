#include "editor.hpp"

#include <cassert>
#include <cmath>
#include <string_view>

#include <unistd.h>

#include "config.hpp"
#include "control.hpp"
#include "debug.hpp"
#include "eventhandler.hpp"
#include "fuzzy.hpp"
#include "terminal.hpp"
#include "utf8.hpp"
#include "util.hpp"

using namespace std::literals;

namespace editor {
namespace {
    std::string_view getControlString(char ch)
    {
        assert(std::iscntrl(ch));
        static constexpr std::array<std::string_view, 32> lut { "NUL"sv, "SOH"sv, "STX"sv, "ETX"sv,
            "EOT"sv, "ENQ"sv, "ACK"sv, "BEL"sv, "BS"sv, "TAB"sv, "LF"sv, "VT"sv, "FF"sv, "CR"sv,
            "SO"sv, "SI"sv, "DLE"sv, "DC1"sv, "DC2"sv, "DC3"sv, "DC4"sv, "NAK"sv, "SYN"sv, "ETB"sv,
            "CAN"sv, "EM"sv, "SUB"sv, "ESC"sv, "FS"sv, "GS"sv, "RS"sv, "US"sv };
        if (ch >= 0 && ch < 32)
            return lut[ch];
        if (ch == 127)
            return "DEL"sv;
        die(fmt::format("Unhandled control character: {}", static_cast<int>(ch)));
    }

    template <typename Value = size_t>
    class LazyTerminalState {
    public:
        LazyTerminalState(const std::vector<std::string>& values, Value initial = {})
            : values_(values)
            , current_(initial)
        {
            set(current_, true);
        }

        void set(Value v, bool override = false)
        {
            if (current_ != v || override)
                terminal::bufferWrite(values_[static_cast<size_t>(v)]);
            current_ = v;
        }

        Value get() const
        {
            return current_;
        }

    private:
        std::vector<std::string> values_;
        Value current_ = 0;
    };

    StatusMessage statusMessage;
    std::unique_ptr<Prompt> currentPrompt;
    bool readOnly = false;
}

auto& getBuffers()
{
    // Maybe this should actually be a std::list?
    // I need to use a vector of unique_ptr here, because Actions use pointers to Buffers
    // (they need to be stable).
    static std::vector<std::unique_ptr<Buffer>> buffers;
    return buffers;
}

// THIS THING IS WILD
Vec drawBuffer(Buffer& buffer, const Vec& pos, const Vec& size, bool prompt = false)
{
    const auto& config = Config::get();

    terminal::bufferWrite(control::sgr::reset);

    // It kinda sucks to scroll in a draw function, but only here do we know the actual view size
    // This is the only reason the buffer reference is not const!
    buffer.scroll(size.y);

    const auto& text = buffer.getText();
    const auto lineCount = buffer.getText().getLineCount();
    const auto firstLine = buffer.getScroll();
    const auto lastLine = std::min(firstLine + static_cast<size_t>(size.y) - 1, lineCount - 1);
    assert(firstLine < lineCount);
    assert(lastLine < lineCount);

    LazyTerminalState<bool> faint(
        { std::string(control::sgr::resetIntensity), std::string(control::sgr::faint) });
    LazyTerminalState<bool> invert(
        { std::string(control::sgr::resetInvert), std::string(control::sgr::invert) });

    const auto showLineNumbers = config.showLineNumbers && !prompt;
    // Always make space for at least 3 digits
    const auto lineNumDigits = std::max(3, static_cast<int>(std::log10(lineCount) + 1));
    // 1 space margin left and right
    const size_t lineNumWidth = showLineNumbers ? lineNumDigits + 2 : 0;
    const auto textWidth = subClamp(size.x, lineNumWidth);

    terminal::bufferWrite(control::moveCursor(pos));
    const auto cursor = buffer.getCursor().start;
    auto drawCursor = Vec { lineNumWidth + pos.x, pos.y + cursor.y - buffer.getScroll() };

    const auto selection = buffer.getSelection();
    const auto selectionStr = buffer.getSelectionString();
    auto matchSelection = [&text, &selectionStr](size_t index) {
        for (size_t i = 0; i < selectionStr.size(); ++i) {
            if (i > text.getSize() || text[index + i] != selectionStr[i])
                return false;
        }
        return true;
    };
    size_t highlightSelectionUntil = 0;

    const auto highlightCurrentLine = config.highlightCurrentLine && !prompt;

    enum class Background { Normal = 0, CurrentLine, Highlight };
    LazyTerminalState<Background> background({
        std::string(control::sgr::bgColorPrefix) + colorScheme["background"],
        std::string(control::sgr::bgColorPrefix) + colorScheme["highlight.currentline"],
        std::string(control::sgr::bgColorPrefix) + colorScheme["highlight.selection"],
    });

    buffer.updateHighlighting();
    const auto highlighting = buffer.getHighlighting();
    const auto startOffset = text.getLine(firstLine).offset;
    const auto lastHighlightLine = text.getLine(std::min(text.getLineCount() - 1, lastLine));
    const auto endOffset = lastHighlightLine.offset + lastHighlightLine.length;
    const auto highlights = highlighting ? highlighting->getHighlights(startOffset, endOffset)
                                         : std::vector<Highlight> {};
    size_t highlightIdx = 0;

    for (size_t l = firstLine; l <= lastLine; ++l) {
        const auto line = text.getLine(l);

        const bool cursorInLine = l == cursor.y;

        // number of characters drawn in this line
        size_t lineCursor = 0;

        // reset fg color before each line
        terminal::bufferWrite(control::sgr::resetFgColor);

        background.set(Background::Normal);

        if (l > firstLine && pos.x > 0) // moving by 0 would still move 1 (default)
            terminal::bufferWrite(control::moveCursorForward(pos.x));

        if (showLineNumbers) {
            invert.set(true);
            terminal::bufferWrite(' '); // left margin
            const auto lineStr = std::to_string(l + 1);
            for (size_t i = 0; i < lineNumDigits - lineStr.size(); ++i)
                terminal::bufferWrite(' ');
            terminal::bufferWrite(lineStr);
            terminal::bufferWrite(' '); // right margin
        }
        invert.set(false);

        const auto lineBg
            = highlightCurrentLine && cursorInLine ? Background::CurrentLine : Background::Normal;
        background.set(lineBg);

        const auto cursorX = buffer.getCursorX(cursor);

        size_t i = line.offset;
        for (; i < line.offset + line.length; ++i) {
            const auto ch = text[i];

            if (!highlights.empty()) {
                // We know: highlights[n].start <= highlights[n+1].start
                // Just use the last highlight for an index
                while (
                    highlightIdx + 1 < highlights.size() && i >= highlights[highlightIdx + 1].start)
                    highlightIdx++;

                if (i == highlights[highlightIdx].start) {
                    terminal::bufferWrite(control::sgr::fgColorPrefix);
                    terminal::bufferWrite(highlighting->getColor(highlights[highlightIdx].id));
                } else if (i == highlights[highlightIdx].end) {
                    terminal::bufferWrite(control::sgr::resetFgColor);
                }
            }

            const bool moveCursor = cursorInLine && i - line.offset < cursorX;

            const bool selected = selection.contains(i);
            invert.set(selected);

            if (!prompt && !selected && i >= highlightSelectionUntil && matchSelection(i))
                highlightSelectionUntil = i + selectionStr.size();

            background.set(i < highlightSelectionUntil ? Background::Highlight : lineBg);

            if (ch == ' ' && config.renderWhitespace && !config.whitespace.space.empty()) {
                // If whitespace is not rendered, this will fall into "else"
                faint.set(true);
                terminal::bufferWrite(config.whitespace.space);

                lineCursor++;
                if (moveCursor)
                    drawCursor.x++;
            } else if (ch == '\t') {
                faint.set(true);
                const bool tabChars = !config.whitespace.tabStart.empty()
                    || !config.whitespace.tabMid.empty() || !config.whitespace.tabEnd.empty();
                assert(buffer.tabWidth > 0);
                std::string tabStr;
                tabStr.reserve(buffer.tabWidth);
                if (config.renderWhitespace && tabChars) {
                    if (buffer.tabWidth >= 2)
                        tabStr.append(config.whitespace.tabStart);
                    for (size_t i = 0; i < buffer.tabWidth - 2; ++i)
                        tabStr.append(config.whitespace.tabMid);
                    tabStr.append(config.whitespace.tabEnd);
                } else {
                    if (lineCursor + buffer.tabWidth > textWidth)
                        tabStr = std::string(textWidth - lineCursor, ' ');
                    else
                        tabStr = std::string(buffer.tabWidth, ' ');
                }
                terminal::bufferWrite(tabStr);

                lineCursor += tabStr.size();
                if (moveCursor)
                    drawCursor.x += tabStr.size();
            } else if (std::iscntrl(ch)) {
                faint.set(true);
                auto str = getControlString(ch);
                if (lineCursor + str.size() > textWidth)
                    str = str.substr(0, textWidth - lineCursor);
                terminal::bufferWrite(str);

                lineCursor += str.size();
                if (moveCursor)
                    drawCursor.x += str.size();
            } else {
                const auto len = utf8::getCodePointLength(text, text.getSize(), i);

                faint.set(false);
                for (size_t j = 0; j < len; ++j)
                    terminal::bufferWrite(text[i + j]);
                i += len - 1; // -1, because we i++ from the for loop anyway

                // Always assume each code point is one character on screen
                // This is probably wrong for a lot of stuff (emojis and such), but what can I do?
                lineCursor++;
                if (moveCursor)
                    drawCursor.x++;
            }

            if (lineCursor >= textWidth)
                break;
        }

        // reset fg color after line
        terminal::bufferWrite(control::sgr::resetFgColor);

        // background for newline
        invert.set(selection.contains(i));
        background.set(i < highlightSelectionUntil ? Background::Highlight : lineBg);

        // The index will be < size but not \n only if we didn't draw the whole line
        const bool drawNewline = config.renderWhitespace && !config.whitespace.newline.empty()
            && lineCursor < textWidth && (i < text.getSize() && text[i] == '\n');
        if (drawNewline) {
            faint.set(true);
            terminal::bufferWrite(config.whitespace.newline);
        }

        invert.set(false);
        background.set(lineBg);

        if (highlightCurrentLine && cursorInLine) {
            const auto numSpaces
                = subClamp(subClamp(textWidth, lineCursor), drawNewline ? 1ul : 0ul);
            for (size_t i = 0; i < numSpaces; ++i)
                terminal::bufferWrite(' ');
        }

        faint.set(false);
        terminal::bufferWrite(control::clearLine);
        if (l - firstLine < size.y - 1)
            terminal::bufferWrite("\r\n");
    }
    terminal::bufferWrite(control::sgr::resetFgColor);

    for (size_t y = lastLine + 1; y < size.y; ++y) {
        if (pos.x > 0)
            terminal::bufferWrite(control::moveCursorForward(pos.x));
        terminal::bufferWrite("~");
        if (text.getSize() == 0 && y == size.y / 2) {
            const auto str = "Scratch Buffer"sv;
            terminal::bufferWrite(std::string(size.x / 2 - str.size() / 2, ' '));
            terminal::bufferWrite(str);
        }
        terminal::bufferWrite(control::clearLine);
        if (y < size.y - 1)
            terminal::bufferWrite("\r\n");
    }

    return drawCursor;
}

void drawStatusBar(const Buffer& buffer, const Vec& terminalSize)
{
    static const auto pid = getpid();

    assert(buffer.indentation.type == Indentation::Type::Spaces
        || buffer.indentation.type == Indentation::Type::Tabs);
    const auto indent = buffer.indentation.type == Indentation::Type::Spaces
        ? fmt::format("Spaces: {}", buffer.indentation.width)
        : fmt::format("Tabs");

    const auto info = fmt::format(" {}/{}  {}  {}  [{}]", buffer.getCursor().start.y + 1,
        buffer.getText().getLineCount(), indent, buffer.getLanguage()->name, pid);
    const auto infoSize = std::min(terminalSize.x - 1, info.size());

    const auto title = buffer.getTitle();
    const auto titleSize = std::min(terminalSize.x - 1, subClamp(terminalSize.x - 1, infoSize));

    std::string status;
    status.reserve(terminalSize.x);
    status.push_back(' ');
    status.append(std::string_view(title).substr(0, titleSize));
    status.append(subClamp(subClamp(terminalSize.x - 1, status.size()), infoSize), ' ');
    status.append(std::string_view(info).substr(0, infoSize));

    terminal::bufferWrite(control::sgr::invert);
    terminal::bufferWrite(status);
    terminal::bufferWrite(control::sgr::resetInvert);
    terminal::bufferWrite(control::clearLine);
    terminal::bufferWrite("\r\n");
}

size_t getNumPromptOptions()
{
    return std::min(currentPrompt->getNumMatchingOptions(), Config::get().numPromptOptions);
}

Vec drawPrompt(const Vec& terminalSize)
{
    const auto numOptions = getNumPromptOptions();
    const auto& options = currentPrompt->getOptions();
    if (numOptions > 0) {
        const auto selected = currentPrompt->getSelectedOption();
        const auto skip = std::min(currentPrompt->getNumMatchingOptions() - numOptions,
            selected >= numOptions / 2 ? selected - (numOptions - 1) / 2 : 0);

        enum class Background { Normal = 0, CurrentLine, Highlight };
        LazyTerminalState<Background> background({
            std::string(control::sgr::bgColorPrefix) + colorScheme["background"],
            std::string(control::sgr::bgColorPrefix) + colorScheme["highlight.currentline"],
            std::string(control::sgr::bgColorPrefix) + colorScheme["highlight.match.prompt"],
        });

        auto skipToNextMatching = [](const std::vector<Prompt::Option>& opts, size_t& index) {
            index++;
            while (index < opts.size() && opts[index].score == 0)
                index++;
        };

        size_t index = 0;
        if (options[index].score == 0)
            skipToNextMatching(options, index);

        for (size_t i = 0; i < skip; ++i)
            skipToNextMatching(options, index);
        assert(index < options.size());

        for (size_t n = 0; n < numOptions; ++n) {
            assert(index < options.size());
            const bool isSelected = index == selected;
            const auto bg = isSelected ? Background::CurrentLine : Background::Normal;
            background.set(bg);

            const auto& opt = options[index];
            size_t matchIndex = 0;
            for (size_t c = 0; c < opt.str.size(); ++c) {
                if (matchIndex < opt.matchedCharacters.size()
                    && c == opt.matchedCharacters[matchIndex]) {
                    background.set(Background::Highlight);
                    matchIndex++;
                } else {
                    background.set(bg);
                }
                terminal::bufferWrite(opt.str[c]);
            }

            background.set(bg);
            terminal::bufferWrite(control::clearLine);
            terminal::bufferWrite("\r\n");

            skipToNextMatching(options, index);
        }
        terminal::bufferWrite(std::string(control::sgr::bgColorPrefix) + colorScheme["background"]);
    } else if (!options.empty()) {
        terminal::bufferWrite("No matches");
        terminal::bufferWrite(control::clearLine);
        terminal::bufferWrite("\r\n");
    } else if (!currentPrompt->getUpdateMessage().empty()) {
        terminal::bufferWrite(currentPrompt->getUpdateMessage());
        terminal::bufferWrite(control::clearLine);
        terminal::bufferWrite("\r\n");
    }

    const auto& prompt = currentPrompt->prompt;
    terminal::bufferWrite(prompt);
    assert(currentPrompt->input.getText().getLineCount() == 1);
    const auto pos = Vec { prompt.size(), terminalSize.y - 1 };
    const auto size = Vec { terminalSize.x - prompt.size(), 1 };
    const auto drawCursor = drawBuffer(currentPrompt->input, pos, size, true);
    terminal::bufferWrite(control::clearLine);
    return drawCursor;
}

void redraw()
{
    terminal::bufferWrite(control::hideCursor);
    terminal::bufferWrite(control::resetCursor);

    const auto size = terminal::getSize();
    // At least one line for "No matches" if there are options and maybe an update message
    const auto promptHeight = []() {
        if (currentPrompt) {
            if (!currentPrompt->getOptions().empty()) {
                // At least one line for "No matches" if there are options
                return std::max(1ul, getNumPromptOptions());
            }
            // Maybe an update message
            return !currentPrompt->getUpdateMessage().empty() ? 1ul : 0ul;
        }
        return 0ul;
    }();
    const auto bufferPos = Vec { 0, 0 };
    const auto bufferSize = Vec { size.x, size.y - 2 - promptHeight };
    auto drawCursor = drawBuffer(getBuffer(), bufferPos, bufferSize);
    terminal::bufferWrite("\r\n");

    terminal::write(control::moveCursor(Vec { bufferPos.x, bufferPos.y + bufferSize.y }));
    drawStatusBar(getBuffer(), size);

    if (currentPrompt) {
        drawCursor = drawPrompt(size);
    } else {
        switch (statusMessage.type) {
        case StatusMessage::Type::Normal:
            terminal::bufferWrite(control::sgr::resetFgColor);
            break;
        case StatusMessage::Type::Error:
            terminal::bufferWrite(control::sgr::fgColorPrefix);
            terminal::bufferWrite(colorScheme["error.prompt"]);
            break;
        }
        terminal::bufferWrite(statusMessage.message);
        terminal::bufferWrite(control::clearLine);
    }

    terminal::bufferWrite(control::moveCursor(drawCursor));
    terminal::bufferWrite(control::showCursor);
    terminal::flushWrite();
}

void triggerRedraw()
{
    static CustomEvent event = getEventHandler().addCustomHandler([] { editor::redraw(); }).second;
    event.emit();
}

void setReadOnly()
{
    readOnly = true;
}

bool getReadOnly()
{
    return readOnly;
}

namespace {
    bool bufferExists(const std::string& name)
    {
        for (size_t i = 0; i < editor::getBufferCount(); ++i) {
            if (name == editor::getBuffer(i).name)
                return true;
        }
        return false;
    }
}

Buffer& openBuffer()
{
    auto& buffers = getBuffers();
    // If current buffer is empty scratch buffer, don't open a new one, but effectively replace it
    if (!buffers.empty() && buffers[0]->path.empty() && buffers[0]->getText().getSize() == 0)
        return *buffers[0];

    const auto& ptr = *buffers.emplace(buffers.begin(), std::make_unique<Buffer>());
    size_t c = 0;
    while (bufferExists(fmt::format("SCRATCH {}", c)))
        c++;
    ptr->name = fmt::format("SCRATCH {}", c);

    if (readOnly)
        ptr->setReadOnly();

    return *ptr;
}

void selectBuffer(size_t index)
{
    auto& buffers = getBuffers();
    assert(index < buffers.size());
    std::swap(buffers[0], buffers[index]);
}

bool selectBuffer(const fs::path& path)
{
    auto& buffers = getBuffers();
    for (size_t i = 0; i < buffers.size(); ++i) {
        if (buffers[i]->path == path) {
            selectBuffer(i);
            return true;
        }
    }
    return false;
}

size_t getBufferCount()
{
    return getBuffers().size();
}

Buffer& getBuffer(size_t index)
{
    auto& buffers = getBuffers();
    assert(index < buffers.size());
    return *buffers[index];
}

void closeBuffer(size_t index)
{
    auto& buffers = getBuffers();
    buffers.erase(buffers.begin() + index);
    if (buffers.empty())
        openBuffer();
}

Prompt::Prompt(std::string_view prompt, std::function<ConfirmCallback> confirmCallback,
    const std::vector<std::string>& options)
    : prompt(prompt)
    , confirmCallback_(confirmCallback)
{
    for (size_t i = 0; i < options.size(); ++i)
        options_.push_back(Option { i, options[i] });
    if (!options_.empty())
        selectedOption_ = options_.size() - 1;
}

Prompt::Prompt(std::string_view prompt, std::function<ConfirmCallback> confirmCallback,
    std::function<UpdateCallback> updateCallback)
    : prompt(prompt)
    , confirmCallback_(confirmCallback)
    , updateCallback_(updateCallback)
{
}

void Prompt::update()
{
    const auto inputStr = input.getText().getString();
    if (inputStr.empty()) {
        for (auto& opt : options_)
            opt.score = 1; // so they are considered matching

        // So the order they were passed to the prompt initially is kept
        std::stable_sort(options_.begin(), options_.end(),
            [](const Option& a, const Option& b) { return a.originalIndex < b.originalIndex; });

        selectedOption_ = options_.size() - 1;
    } else {
        for (auto& opt : options_)
            opt.score = fuzzyMatchScore(inputStr, opt.str, &opt.matchedCharacters);

        std::stable_sort(options_.begin(), options_.end(),
            [](const Option& a, const Option& b) { return a.score < b.score; });

        // It's sorted, so if the last one is score = 0, they all are.
        const auto anyMatching = !options_.empty() && options_.back().score > 0;
        selectedOption_ = anyMatching ? options_.size() - 1 : 0;
    }

    if (updateCallback_)
        updateMessage_ = updateCallback_(this);
}

std::optional<StatusMessage> Prompt::confirm()
{
    if (options_.empty()) {
        return confirmCallback_(input.getText().getString());
    } else {
        if (getNumMatchingOptions() > 0) {
            assert(options_[selectedOption_].score > 0);
            return confirmCallback_(options_[selectedOption_].str);
        }
        return std::nullopt;
    }
}

void Prompt::selectUp()
{
    // This handles the care where we don't need to do anything and where there are no matching
    // options (or where options is empty)
    if (selectedOption_ > 0) {
        // the check is wrap around
        for (size_t i = selectedOption_ - 1; i < selectedOption_; --i) {
            if (options_[i].score > 0) {
                selectedOption_ = i;
                break;
            }
        }
    }
}

void Prompt::selectDown()
{
    // If the selected is not matching, none are
    if (options_.empty() || options_[selectedOption_].score == 0)
        return;

    for (size_t i = selectedOption_ + 1; i < options_.size(); ++i) {
        if (options_[i].score > 0) {
            selectedOption_ = i;
            break;
        }
    }
}

size_t Prompt::getNumMatchingOptions() const
{
    return std::count_if(
        options_.begin(), options_.end(), [](const Option& o) { return o.score > 0; });
}

const std::vector<Prompt::Option>& Prompt::getOptions() const
{
    return options_;
}

size_t Prompt::getSelectedOption() const
{
    return selectedOption_;
}

const std::string& Prompt::getUpdateMessage() const
{
    return updateMessage_;
}

Prompt* getPrompt()
{
    return currentPrompt.get();
}

void setPrompt(Prompt&& prompt)
{
    currentPrompt = std::make_unique<Prompt>(std::move(prompt));
    // We need to update, so the score is at least 1 if the input is empty and
    // if the input is non-empty update callbacks are called.
    currentPrompt->update();
}

void confirmPrompt()
{
    assert(currentPrompt);
    // Move from currentPrompt first, so callback can set another prompt
    auto prompt = std::move(currentPrompt);
    const auto msg = prompt->confirm();
    if (msg)
        setStatusMessage(msg->message, msg->type);
    else // could not confirm, restore currentPrompt
        currentPrompt = std::move(prompt);
}

void abortPrompt()
{
    assert(currentPrompt);
    currentPrompt.reset();
}

void setStatusMessage(const std::string& message, StatusMessage::Type type)
{
    statusMessage = StatusMessage { message, type };
}

void setStatusMessage(const StatusMessage& message)
{
    statusMessage = message;
}

StatusMessage getStatusMessage()
{
    return statusMessage;
}
}
