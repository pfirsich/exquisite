#include "editor.hpp"

#include <cassert>
#include <cmath>
#include <string_view>

#include <unistd.h>

#include "config.hpp"
#include "control.hpp"
#include "debug.hpp"
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
        if (ch > 0 && ch < 32)
            return lut[ch];
        if (ch == 127)
            return "DEL"sv;
        die("Unhandled control character");
    }

    template <typename Value = size_t>
    class LazyTerminalState {
    public:
        LazyTerminalState(const std::vector<std::string>& values, Value initial = {})
            : values_(values)
            , current_(initial)
        {
        }

        void set(Value v)
        {
            if (current_ != v)
                terminal::bufferWrite(values_[static_cast<size_t>(v)]);
            current_ = v;
        }

    private:
        std::vector<std::string> values_;
        size_t current_ = 0;
    };

    StatusMessage statusMessage;
    std::unique_ptr<Prompt> currentPrompt;
}

Buffer buffer;

Vec drawBuffer(Buffer& buf, const Vec& pos, const Vec& size, const Config& config)
{
    terminal::bufferWrite(control::sgr::reset);

    // It kinda sucks to scroll in a draw function, but only here do we know the actual view size
    // This is the only reason the buffer reference is not const!
    buf.scroll(size.y);

    const auto& text = buf.getText();
    const auto lineCount = buf.getText().getLineCount();
    const auto firstLine = buf.getScroll();
    const auto lastLine = firstLine + std::min(static_cast<size_t>(size.y), lineCount);
    assert(firstLine <= lineCount - 1);

    LazyTerminalState<bool> faint(
        { std::string(control::sgr::resetIntensity), std::string(control::sgr::faint) });
    LazyTerminalState<bool> invert(
        { std::string(control::sgr::resetInvert), std::string(control::sgr::invert) });

    // Always make space for at least 3 digits
    const auto lineNumDigits = std::max(3, static_cast<int>(std::log10(lineCount) + 1));
    // 1 space margin left and right
    const size_t lineNumWidth = config.showLineNumbers ? lineNumDigits + 2 : 0;
    const auto textWidth = subClamp(size.x, lineNumWidth);

    terminal::bufferWrite(control::moveCursor(pos));
    const auto cursor = buf.getCursor().start;
    auto drawCursor = Vec { lineNumWidth + pos.x, pos.y + cursor.y - buf.getScroll() };

    const auto selection = buf.getSelection();

    buf.updateHighlighting();
    const auto highlighting = buf.getHighlighting();
    const auto startOffset = text.getLine(firstLine).offset;
    const auto endOffset = text.getLine(std::min(text.getLineCount() - 1, lastLine)).offset;
    const auto highlights = highlighting ? highlighting->getHighlights(startOffset, endOffset)
                                         : std::vector<Highlight> {};
    size_t highlightIdx = 0;

    for (size_t l = firstLine; l < lastLine; ++l) {
        const auto line = text.getLine(l);

        const bool cursorInLine = l == cursor.y;

        // number of characters drawn in this line
        size_t lineCursor = 0;

        // reset fg color before each line
        terminal::bufferWrite(control::sgr::resetFgColor);

        if (config.highlightCurrentLine && cursorInLine) {
            terminal::bufferWrite(control::sgr::bgColorPrefix);
            terminal::bufferWrite(colorScheme["highlight.currentline"]);
        }

        if (l > firstLine && pos.x > 0) // moving by 0 would still move 1 (default)
            terminal::bufferWrite(control::moveCursorForward(pos.x));

        if (config.showLineNumbers) {
            invert.set(true);
            terminal::bufferWrite(' '); // left margin
            const auto lineStr = std::to_string(l + 1);
            for (size_t i = 0; i < lineNumDigits - lineStr.size(); ++i)
                terminal::bufferWrite(' ');
            terminal::bufferWrite(lineStr);
            terminal::bufferWrite(' '); // right margin
            invert.set(false);
        }

        const auto cursorX = buf.getCursorX(cursor);

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

            const bool selected = selection.contains(i);
            invert.set(selected);

            if (ch == ' ' && config.renderWhitespace && config.spaceChar) {
                // If whitespace is not rendered, this will fall into "else"
                faint.set(true);
                terminal::bufferWrite(*config.spaceChar);

                lineCursor++;
            } else if (ch == '\t') {
                const bool tabChars = config.tabStartChar || config.tabMidChar || config.tabEndChar;
                assert(config.tabWidth > 0);
                if (config.renderWhitespace && tabChars) {
                    if (config.tabWidth >= 2)
                        terminal::bufferWrite(*config.tabStartChar);
                    for (size_t i = 0; i < config.tabWidth - 2; ++i)
                        terminal::bufferWrite(*config.tabMidChar);
                    terminal::bufferWrite(*config.tabEndChar);
                } else {
                    for (size_t i = 0; i < config.tabWidth; ++i)
                        terminal::bufferWrite(' ');
                }

                lineCursor += config.tabWidth;
            } else if (std::iscntrl(ch)) {
                faint.set(true);
                const auto str = getControlString(ch);
                terminal::bufferWrite(str);

                lineCursor += str.size();
            } else {
                const auto len = utf8::getCodePointLength(text, text.getSize(), i);

                faint.set(false);
                for (size_t j = 0; j < len; ++j)
                    terminal::bufferWrite(text[i + j]);
                i += len - 1; // -1, because we i++ from the for loop anyway

                // Always assume each code point is one character on screen
                // This is probably wrong for a lot of stuff (emojis and such), but what can I do?
                lineCursor++;
            }

            if (lineCursor >= textWidth)
                break;
        }
        if (cursorInLine) {
            drawCursor.x += std::min(lineCursor, cursorX);
        }

        // reset fg color for after line
        terminal::bufferWrite(control::sgr::resetFgColor);

        // The index will be < size but not \n only if we didn't draw the whole line
        const bool drawNewline = config.renderWhitespace && config.newlineChar
            && lineCursor < textWidth && (i < text.getSize() && text[i] == '\n');
        if (drawNewline) {
            faint.set(true);
            terminal::bufferWrite(*config.newlineChar);
        }

        if (config.highlightCurrentLine && cursorInLine) {
            const auto numSpaces
                = subClamp(subClamp(textWidth, lineCursor), drawNewline ? 1ul : 0ul);
            for (size_t i = 0; i < numSpaces; ++i)
                terminal::bufferWrite(' ');
            terminal::bufferWrite(control::sgr::resetBgColor);
        }

        faint.set(false);
        terminal::bufferWrite(control::clearLine);
        if (l - firstLine < size.y - 1)
            terminal::bufferWrite("\r\n");
    }
    terminal::bufferWrite(control::sgr::resetFgColor);

    for (size_t y = lastLine + 1; y < size.y + 1; ++y) {
        if (pos.x > 0)
            terminal::bufferWrite(control::moveCursorForward(pos.x));
        terminal::bufferWrite("~");
        terminal::bufferWrite(control::clearLine);
        if (y < size.y)
            terminal::bufferWrite("\r\n");
    }

    return drawCursor;
}

void drawStatusBar(const Vec& terminalSize)
{
    static const auto pid = getpid();
    std::stringstream ss;
    const auto lang = buffer.getLanguage();
    if (lang)
        ss << lang->name << "  ";
    ss << buffer.getCursor().start.y + 1 << "/" << buffer.getText().getLineCount() << "  [" << pid
       << "]";
    const auto lines = ss.str();

    std::string status;
    status.reserve(terminalSize.x);
    status.append(" "s + (buffer.isModified() ? "*"s : ""s) + buffer.name);
    status.append(subClamp(subClamp(terminalSize.x, lines.size()), status.size()), ' ');
    status.append(lines);

    terminal::bufferWrite(control::sgr::invert);
    terminal::bufferWrite(status);
    terminal::bufferWrite(control::sgr::resetInvert);
    terminal::bufferWrite(control::clearLine);
    terminal::bufferWrite("\r\n");
}

size_t getNumPromptOptions()
{
    return std::min(currentPrompt->getNumMatchingOptions(), config.numPromptOptions);
}

Vec drawPrompt(const Vec& terminalSize)
{
    const auto numOptions = getNumPromptOptions();
    const auto& options = currentPrompt->getOptions();
    if (numOptions > 0) {
        const auto selected = currentPrompt->getSelectedOption();
        const auto skip = std::min(currentPrompt->getNumMatchingOptions() - numOptions,
            selected >= numOptions / 2 ? selected - (numOptions - 1) / 2 : 0);

        enum Background { Normal = 0, Selected, Highlight };
        LazyTerminalState<Background> background({
            std::string(control::sgr::resetBgColor),
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
            const auto bg = isSelected ? Background::Selected : Background::Normal;
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
        terminal::bufferWrite(control::sgr::resetBgColor);
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
    Config promptConfig = config;
    promptConfig.showLineNumbers = false;
    promptConfig.highlightCurrentLine = false;
    const auto pos = Vec { prompt.size(), terminalSize.y - 1 };
    const auto size = Vec { terminalSize.x - prompt.size(), 1 };
    const auto drawCursor = drawBuffer(currentPrompt->input, pos, size, promptConfig);
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
    const auto bufferSize = Vec { size.x, size.y - 2 - promptHeight };
    auto drawCursor = drawBuffer(buffer, Vec { 0, 0 }, bufferSize, config);
    terminal::bufferWrite("\r\n");

    drawStatusBar(size);

    if (currentPrompt) {
        drawCursor = drawPrompt(size);
    } else {
        switch (statusMessage.type) {
        case StatusMessage::Type::Normal:
            terminal::bufferWrite(control::sgr::reset);
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

        std::sort(options_.begin(), options_.end(),
            [](const Option& a, const Option& b) { return a.originalIndex < b.originalIndex; });

        selectedOption_ = options_.size() - 1;
    } else {
        for (auto& opt : options_)
            opt.score = fuzzyMatchScore(inputStr, opt.str, &opt.matchedCharacters);

        std::sort(options_.begin(), options_.end(),
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
            debug("score ", i, ": ", options_[i].score);
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

std::unique_ptr<Prompt>& getPrompt()
{
    return currentPrompt;
}

void setPrompt(Prompt&& prompt)
{
    currentPrompt = std::make_unique<Prompt>(std::move(prompt));
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
