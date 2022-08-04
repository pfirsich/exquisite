#include "config.hpp"

#include <filesystem>

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <sol/sol.hpp>

#include "debug.hpp"
#include "editor.hpp"
#include "process.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

namespace {
sol::state& getLuaState()
{
    static sol::state lua;
    return lua;
}

fs::path getHomeDirectory()
{
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

fs::path getConfigHomeDirectory()
{
    const auto configHome = ::getenv("XDG_CONFIG_HOME");
    if (configHome) {
        return fs::path(configHome);
    }
    return getHomeDirectory() / ".config";
}

fs::path getConfigDirectory()
{
    return getConfigHomeDirectory() / "exquisite";
}

namespace api {
    void bind(std::string_view /*input*/, std::string_view /*commandName*/,
        std::optional<sol::table> /*args*/)
    {
    }

    void hook(std::string_view hookName, sol::function hookFunction)
    {
        auto hooks = getLuaState()["exq"]["_hooks"][hookName].get<sol::table>();
        hooks.add(hookFunction);
    }

    sol::object run(std::vector<std::string> args, sol::table kwargs)
    {
        debug("run inside");
        std::string stdin;
        if (kwargs["stdin"].valid()) {
            stdin = kwargs["stdin"];
        }
        const auto res = Process::run(args, stdin);
        if (!res) {
            debug("!res");
            return sol::lua_nil;
        }
        debug("result");
        return getLuaState().create_table_with(
            "status", res->status, "stdout", res->out, "stderr", res->err);
    }

    sol::table getCursor()
    {
        auto& lua = getLuaState();
        const auto& buffer = editor::getBuffer();
        const auto& cursor = buffer.getCursor();
        return lua.create_table_with("start",
            lua.create_table_with("x", static_cast<int>(cursor.start.x), "y", static_cast<int>(cursor.start.y), "offset",
                static_cast<int>(buffer.getCursorOffset(cursor.start))),
            "end",
            lua.create_table_with("x", static_cast<int>(cursor.end.x), "y", static_cast<int>(cursor.end.y), "offset",
                static_cast<int>(buffer.getCursorOffset(cursor.end))));
    }

    void setCursor(sol::table start, sol::table end)
    {
        auto& buffer = editor::getBuffer();
        auto makeEnd = [&buffer](sol::table cend) -> Cursor::End {
            if (cend["offset"].valid()) {
                return buffer.getCursorEndFromOffset(cend["offset"]);
            } else {
                return Cursor::End { cend["x"], cend["y"] };
            }
        };
        buffer.getCursor().start = makeEnd(start);
        buffer.getCursor().end = makeEnd(end);
    }

    std::string getBufferText()
    {
        return editor::getBuffer().getText().getString();
    }

    std::string getBufferLanguage()
    {
        return editor::getBuffer().getLanguage()->name;
    }

    void replaceBufferText(std::string_view text, std::optional<bool> undoable)
    {
        if (undoable.value_or(true)) {
            editor::getBuffer().setTextUndoable(std::string(text));
        } else {
            editor::getBuffer().setText(text);
        }
    }
}
}

Config& Config::get()
{
    static Config config;
    return config;
}

void executeHook(std::string_view hookName)
{
    auto hooks = getLuaState()["exq"]["_hooks"][hookName].get<sol::table>();
    for (size_t i = 1; i <= hooks.size(); ++i) {
        const auto res = hooks[i]();
        if (!res.valid()) {
            debug("Error in hook: {}", static_cast<sol::error>(res).what());
        }
    }
}

void loadConfig()
{
    const auto configDirectory = getConfigDirectory();
    const auto configFilePath = configDirectory / "config.lua";
    const fs::path localConfigPath = "exquisite.lua";
    if (!fs::exists(configFilePath) && !fs::exists(localConfigPath)) {
        return;
    }

    auto& lua = getLuaState();
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::coroutine, sol::lib::string,
        sol::lib::os, sol::lib::math, sol::lib::table, sol::lib::io);

    lua["package"]["path"]
        = (configDirectory / "?.lua;").u8string() + lua["package"]["path"].get<std::string>();

    auto exq = lua.create_named_table("exq");

    // internal
    // I just store the hooks in the lua state, so I don't have to worry about ownership
    auto hooks = exq.create_named("_hooks");
    hooks["init"] = lua.create_table();
    hooks["presave"] = lua.create_table();
    hooks["exit"] = lua.create_table();

    // api
    exq["bind"] = api::bind;
    exq["hook"] = api::hook;
    exq["debug"] = [](std::string_view str) { debug("{}", str); };

    exq["run"] = api::run;
    exq["getCursor"] = api::getCursor;
    exq["setCursor"] = api::setCursor;
    exq["getBufferText"] = api::getBufferText;
    exq["getBufferLanguage"] = api::getBufferLanguage;
    exq["replaceBufferText"] = api::replaceBufferText;

    // config
    auto& config = Config::get();

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
    config.whitespace.tabEnd = ws["tabEnd"];

    config.trimTrailingWhitespaceOnSave = lconfig["trimTrailingWhitespaceOnSave"];
    config.showLineNumbers = lconfig["showLineNumbers"];
    config.highlightCurrentLine = lconfig["highlightCurrentLine"];
    config.numPromptOptions = lconfig["numPromptOptions"];

    executeHook("init");
}
