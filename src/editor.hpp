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

    using ConfirmCallback = StatusMessage(std::string_view input);
    using UpdateCallback = std::string(Prompt* prompt);

    Buffer input;
    std::string prompt;

    Prompt(std::string_view prompt, std::function<ConfirmCallback> confirmCallback,
        const std::vector<std::string>& options);
    Prompt(std::string_view prompt, std::function<ConfirmCallback> confirmCallback,
        std::function<UpdateCallback> updateCallback = nullptr);

    size_t getNumMatchingOptions() const;
    const std::vector<Option>& getOptions() const;
    size_t getSelectedOption() const;
    const std::string& getUpdateMessage() const;

    void update();
    std::optional<StatusMessage> confirm();
    void selectUp();
    void selectDown();

private:
    std::function<ConfirmCallback> confirmCallback_;
    std::function<UpdateCallback> updateCallback_ = nullptr;
    // A message that can be returned by the update callback and will be displayed above the prompt
    std::string updateMessage_;
    std::vector<Option> options_;
    // This always points at a matching option. If there are no (matching) options, it's 0
    size_t selectedOption_ = 0;
};

extern Buffer buffer;

void redraw();

void setStatusMessage(
    const std::string& message, StatusMessage::Type type = StatusMessage::Type::Normal);
void setStatusMessage(const StatusMessage& message);
StatusMessage getStatusMessage();

std::unique_ptr<Prompt>& getPrompt();
void setPrompt(Prompt&& prompt);
void confirmPrompt();
void abortPrompt();
}
