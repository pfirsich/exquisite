exq.hook("presave", function()
    local ret = exq.run(
        {"clang-format"}, -- "--cursor", cursor.start.offset
        {stdin = exq.getBufferText()})
    if ret == nil then
        exq.debug("Could not execute clang-format")
        return
    end
    if ret.status == 0 then
        exq.replaceBufferText(ret.stdout)
    else
        exq.debug(ret.stderr)
    end
end)
