#include "config.hpp"

#include <filesystem>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <sol/sol.hpp>

#include "util.hpp"

namespace fs = std::filesystem;

Config& Config::get()
{
    static Config config;
    return config;
}

namespace {
fs::path getHomeDirectory() {
    const auto home = ::getenv("HOME");
    if (home) {
        return fs::path(home);
    }
    const auto uid = ::geteuid();
    const auto pw = getpwuid(uid);
    if (!pw) {
        die("Could not get user directory");
    }
    return fs::path(pw->pw_dir);
}

fs::path getConfigHomeDirectory() {
    const auto configHome = ::getenv("XDG_CONFIG_HOME");
    if (configHome) {
        return fs::path(configHome);
    }
    return getHomeDirectory() / ".config";
}

fs::path getConfigDirectory() {
    return getConfigHomeDirectory() / "exquisite";
}
}

void loadConfig()
{
    const auto configFilePath = getConfigDirectory() / "config.lua";
    const fs::path localConfigPath = "exquisite.lua";
    if (!fs::exists(configFilePath) && !fs::exists(localConfigPath)) {
        return;
    }

    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::package);

    // exq.colorschemes[name] = { scope = color(8-bit num or hex) }
    // exq.config.colorscheme = "panda"
    // exq.bind("ctrl+s", function() end)
    // exq.bind("ctrl+right", exq.moveWordForward, false)
    // exq.bind("ctrl+shift+right", exq.moveWordForward, true)
    // exq.showPalette(options(list of strings), initial_input (string), function(input) end (handle))
    // exq.openBuffer(filename)
    // exq.replaceBuffer(string) # clang-format
    // exq.registerCommand("
    // exq.run({args}, {kwargs}) # stdin = buffer, stdout = buffer
    //    usage: stdout async into buffer (ninja, cmake)
    //           sync, buffer as stdin to command (clang-format)
    //           sync, no stdin, stdout to var (fdfind)

    auto& config = Config::get();

    auto exq = lua.create_named_table("exq");
    auto lconfig = exq.create_named("config");

    lconfig["tabWidth"] = config.tabWidth;
    lconfig["indentUsingSpaces"] = config.indentUsingSpaces;
    lconfig["indentWidth"] = config.indentWidth;

    lconfig["renderWhitespace"] = config.renderWhitespace;
    auto ws = lconfig.create("whitespace");
    ws["space"] = config.whitespace.space;
    ws["newline"] = config.whitespace.newline;
    ws["tabStart"] = config.whitespace.tabStart;
    ws["tabMid"] = config.whitespace.tabMid;
    ws["tabEnd"] = config.whitespace.tabEnd;

    lconfig["trimTrailingWhitespaceOnSave"] = config.trimTrailingWhitespaceOnSave;
    lconfig["showLineNumbers"] = config.showLineNumbers;
    lconfig["highlightCurrentLine"] = config.highlightCurrentLine;
    lconfig["numPromptOptions"] = config.numPromptOptions;

    if (fs::exists(configFilePath)) {
        lua.script_file(configFilePath.u8string());
    }

    if (fs::exists(localConfigPath)) {
        lua.script_file(localConfigPath.u8string());
    }

    config.tabWidth = lconfig["tabWidth"];
    config.indentUsingSpaces = lconfig["indentUsingSpaces"];
    config.indentWidth = lconfig["indentWidth"];

    config.whitespace.space = ws["space"];
    config.whitespace.newline = ws["newline"];
    config.whitespace.tabStart = ws["tabStart"];
    config.whitespace.tabMid = ws["tabMid"];
    config.whitespace.tabEnd = ws["tabEnd"];;

    config.trimTrailingWhitespaceOnSave = lconfig["trimTrailingWhitespaceOnSave"];
    config.showLineNumbers = lconfig["showLineNumbers"];
    config.highlightCurrentLine = lconfig["highlightCurrentLine"];
    config.numPromptOptions = lconfig["numPromptOptions"];
}
