#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "buffer.hpp"
#include "terminal.hpp"

namespace editor {
struct StatusMessage {
    enum class Type { Normal, Error };

    std::string message;
    Type type = Type::Normal;
};

struct Prompt {
public:
    struct Option {
        size_t originalIndex;
        std::string str;
        size_t score = 0;
        std::vector<size_t> matchedCharacters = {};
    };

    using Callback = StatusMessage(std::string_view input);

    Buffer input;

    Prompt(std::string_view prompt, std::function<Callback> callback,
        const std::vector<std::string>& options = {});

    const std::string& getPrompt() const;

    size_t getNumMatchingOptions() const;
    const std::vector<Option>& getOptions() const;
    size_t getSelectedOption() const;

    void update();
    std::optional<StatusMessage> confirm();
    void selectUp();
    void selectDown();

private:
    std::string prompt_;
    std::function<Callback> callback_;
    std::vector<Option> options_;
    // This always points at a matching option. If there are no (matching) options, it's 0
    size_t selectedOption_ = 0;
};

extern Buffer buffer;

void redraw();

void setStatusMessage(
    const std::string& message, StatusMessage::Type type = StatusMessage::Type::Normal);
StatusMessage getStatusMessage();

std::unique_ptr<Prompt>& getPrompt();
void setPrompt(Prompt&& prompt);
void confirmPrompt();
void abortPrompt();
}
