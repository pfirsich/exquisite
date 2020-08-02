#include "clipboard.hpp"

#include <vector>

#include "process.hpp"

namespace {
struct ClipboardCommand {
    std::vector<std::string> set;
    std::vector<std::string> get;
};

std::optional<ClipboardCommand> getClipboardCommand()
{
    // I could use the return value of which as the command, but I would have to strip the
    // trailing newline, which I don't want to do.

    const auto xsel = which("xsel");
    if (xsel)
        return ClipboardCommand { { "xsel", "-ib" }, { "xsel", "-ob" } };

    const auto xclip = which("xclip");
    if (xclip)
        return ClipboardCommand { { "xclip", "-selection", "c" },
            { "xclip", "-selection", "c", "-o" } };

    const auto pbcopy = which("pbcopy");
    const auto pbpaste = which("pbpaste");
    if (pbcopy && pbpaste)
        return ClipboardCommand { { "pbcopy" }, { "pbpaste" } };

    return std::nullopt;
}

const std::optional<ClipboardCommand>& getClipboardCommandCached()
{
    static const auto cmd = getClipboardCommand();
    return cmd;
}
}

std::optional<std::string> getClipboardText()
{
    const auto& cmd = getClipboardCommandCached();
    if (!cmd) {
        return std::nullopt;
    }
    const auto res = Process::run(cmd->get);
    if (!res || res->status != 0)
        return std::nullopt;
    return res->out;
}

bool setClipboardText(std::string_view text)
{
    const auto& cmd = getClipboardCommandCached();
    if (!cmd) {
        return false;
    }
    const auto res = Process::run(cmd->set, text);
    return res && res->status == 0;
}
