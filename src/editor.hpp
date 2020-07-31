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

using PromptCallback = StatusMessage(std::string_view input);

struct Prompt {
    std::string prompt;
    std::function<PromptCallback> callback;
    std::vector<std::string> options = {};
    Buffer input {};
};

extern Buffer buffer;
extern std::unique_ptr<Prompt> currentPrompt;

void redraw();
void confirmPrompt();
void setStatusMessage(
    const std::string& message, StatusMessage::Type type = StatusMessage::Type::Normal);
}
