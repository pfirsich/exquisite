local function clangFormat()
    if exq.getBufferLanguage() ~= "C++" then
        return
    end

    local cursor = exq.getCursor()
    local ret = exq.run(
        {"clang-format", "--cursor", tostring(cursor.start.offset)},
        {stdin = exq.getBufferText()})
    if ret == nil then
        exq.debug("Could not execute clang-format")
        return
    end

    if ret.status == 0 then
        local headerStart, headerEnd = ret.stdout:find("({.-}\n)")
        local header = ret.stdout:sub(headerStart, headerEnd)
        local cursorOffset = tonumber(header:match("\"Cursor\": (%d+),"))
        local code = ret.stdout:sub(headerEnd + 1)
        exq.replaceBufferText(code)
        exq.setCursor({offset = cursorOffset}, {offset = cursorOffset})
    else
        exq.debug(ret.stderr)
    end
end

exq.hook("presave", clangFormat)

return {clangFormat = clangFormat}