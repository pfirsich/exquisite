#include "editor.hpp"

#include <cassert>
#include <cmath>
#include <string_view>

using namespace std::literals;

#include "config.hpp"
#include "control.hpp"
#include "debug.hpp"
#include "fuzzy.hpp"
#include "terminal.hpp"
#include "utf8.hpp"

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
        assert(false && "Unhandled control character");
    }

    template <typename Value = size_t>
    class TerminalState {
    public:
        TerminalState(const std::vector<std::string>& values, Value initial = {})
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

Vec drawBuffer(Buffer& buf, const Vec& size, const Config& config)
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

    TerminalState<bool> faint(
        { std::string(control::sgr::resetIntensity), std::string(control::sgr::faint) });
    TerminalState<bool> invert(
        { std::string(control::sgr::resetInvert), std::string(control::sgr::invert) });

    // Always make space for at least 3 digits
    const auto lineNumDigits = std::max(3, static_cast<int>(std::log10(lineCount) + 1));
    // 1 space margin left and right
    const auto lineNumWidth = config.showLineNumbers ? lineNumDigits + 2 : 0;

    terminal::flushWrite();
    const auto pos = terminal::getCursorPosition();
    const auto cursor = buf.getCursor().start;
    auto drawCursor = Vec { lineNumWidth + pos.x, pos.y + cursor.y - buf.getScroll() };

    const auto selection = buf.getSelection();

    for (size_t l = firstLine; l < lastLine; ++l) {
        const auto line = text.getLine(l);

        const bool cursorInLine = l == cursor.y;

        if (config.highlightCurrentLine && cursorInLine)
            terminal::bufferWrite(control::sgr::bgColor(colorScheme.highlightLine));

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

        const auto lineEndIndex = line.offset + std::min(line.length, size.x - lineNumWidth);
        for (size_t i = line.offset; i < lineEndIndex; ++i) {
            const auto ch = text[i];

            const bool charInFrontOfCursor = i < line.offset + buf.getX(cursor);
            const bool moveCursor = cursorInLine && charInFrontOfCursor;

            const bool selected = selection.contains(i);
            invert.set(selected);

            if (ch == ' ' && config.renderWhitespace && config.spaceChar) {
                // If whitespace is not rendered, this will fall into "else"
                faint.set(true);
                terminal::bufferWrite(*config.spaceChar);

                if (moveCursor)
                    drawCursor.x++;
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

                if (moveCursor)
                    drawCursor.x += config.tabWidth;
            } else if (std::iscntrl(ch)) {
                faint.set(true);
                const auto str = getControlString(ch);
                terminal::bufferWrite(str);

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
                if (moveCursor)
                    drawCursor.x++;
            }
        }

        // The index will be < size but not \n only if we didn't draw the whole line
        if (lineEndIndex < text.getSize() && text[lineEndIndex] == '\n') {
            if (config.renderWhitespace && config.newlineChar) {
                faint.set(true);
                terminal::bufferWrite(*config.newlineChar);
            }
        }

        if (config.highlightCurrentLine && cursorInLine) {
            for (size_t i = 0; i < size.x - lineNumWidth - line.length - 1; ++i)
                terminal::bufferWrite(' ');
            terminal::bufferWrite(control::sgr::resetBgColor);
        }

        faint.set(false);
        terminal::bufferWrite(control::clearLine);
        if (l - firstLine < size.y - 1)
            terminal::bufferWrite("\r\n");
    }

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
    std::stringstream ss;
    ss << buffer.getCursor().start.y + 1 << "/" << buffer.getText().getLineCount() << ' ';
    const auto lines = ss.str();

    std::string status;
    status.reserve(terminalSize.x);
    status.append(buffer.name);
    status.append(terminalSize.x - lines.size() - status.size(), ' ');
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
        TerminalState<Background> background({
            std::string(control::sgr::resetBgColor),
            control::sgr::bgColor(colorScheme.highlightLine),
            control::sgr::bgColor(colorScheme.matchHighlightColor),
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
    }

    const auto prompt = currentPrompt->getPrompt();
    terminal::bufferWrite(prompt);
    assert(currentPrompt->input.getText().getLineCount() == 1);
    Config promptConfig = config;
    promptConfig.showLineNumbers = false;
    promptConfig.highlightCurrentLine = false;
    const auto drawCursor
        = drawBuffer(currentPrompt->input, Vec { terminalSize.x - prompt.size(), 1 }, promptConfig);
    terminal::bufferWrite(control::clearLine);
    return drawCursor;
}

void redraw()
{
    terminal::bufferWrite(control::hideCursor);
    terminal::bufferWrite(control::resetCursor);

    const auto size = terminal::getSize();
    // At least one line for "No matches" if there are options
    const auto promptHeight = currentPrompt
        ? std::max(currentPrompt->getOptions().size() > 0 ? 1ul : 0ul, getNumPromptOptions())
        : 0;
    const auto bufferSize = Vec { size.x, size.y - 2 - promptHeight };
    auto drawCursor = drawBuffer(buffer, bufferSize, config);
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
            terminal::bufferWrite(control::sgr::fgColor(colorScheme.statusErrorColor));
            break;
        }
        terminal::bufferWrite(statusMessage.message);
        terminal::bufferWrite(control::clearLine);
    }

    terminal::bufferWrite(control::moveCursor(drawCursor));
    terminal::bufferWrite(control::showCursor);
    terminal::flushWrite();
}

Prompt::Prompt(std::string_view prompt, std::function<Callback> callback,
    const std::vector<std::string>& options)
    : prompt_(prompt)
    , callback_(callback)
{
    for (size_t i = 0; i < options.size(); ++i)
        options_.push_back(Option { i, options[i] });
    if (!options_.empty())
        selectedOption_ = options_.size() - 1;
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
}

std::optional<StatusMessage> Prompt::confirm()
{
    if (options_.empty()) {
        return callback_(input.getText().getString());
    } else {
        if (getNumMatchingOptions() > 0) {
            assert(options_[selectedOption_].score > 0);
            return callback_(options_[selectedOption_].str);
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

const std::string& Prompt::getPrompt() const
{
    return prompt_;
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

StatusMessage getStatusMessage()
{
    return statusMessage;
}
}
