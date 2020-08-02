#pragma once

#include <optional>
#include <string_view>

std::optional<std::string> getClipboardText();
bool setClipboardText(std::string_view text);
