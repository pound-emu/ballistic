local log = require("log")

local M = {}

local function escape_html(str)
    if not str then return "" end
    local map = {
        ["&"] = "&amp;",
        ["<"] = "&lt;",
        [">"] = "&gt;",
        ['"'] = "&quot;",
        ["'"] = "&#39;",
    }
    return (str:gsub("[&<>\"']", map))
end

function M.escape(str)
    return escape_html(str)
end

function M.keyword(syntax_state, str)
    return syntax_state.formats.keyword(syntax_state.classes, escape_html(str))
end

function M.type(syntax_state, str, href)
    return syntax_state.formats.type(syntax_state.classes, escape_html(str), href)
end

function M.function_name(syntax_state, str)
    return syntax_state.formats.function_name(syntax_state.classes, escape_html(str))
end

function M.literal(syntax_state, str)
    return syntax_state.formats.literal(syntax_state.classes, escape_html(str))
end

function M.comment(syntax_state, str)
    return syntax_state.formats.comment(syntax_state.classes, escape_html(str))
end

function M.attribute(syntax_state, str)
    return syntax_state.formats.attribute(syntax_state.classes, escape_html(str))
end

function M.create(options)
    options = options or {}
    local classes = options.classes or {}
    local formats = options.formats or {}

    log.debug("Creating syntax state.")

    return {
        classes = {
            keyword = classes.keyword or options.kw_class or "kw",
            type = classes.type or options.type_class or "type",
            function_name = classes.function_name or options.fn_class or "fn",
            literal = classes.literal or options.literal_class or "lit",
            comment = classes.comment or options.comment_class or "comment",
            attribute = classes.attribute or options.attr_class or "attr",
        },
        -- Formatters receive the classes table and the text
        formats = {
            keyword = formats.keyword or function(classes, text)
                return string.format("<span class='%s'>%s</span>", classes.keyword, text)
            end,
            type = formats.type or function(classes, text, href)
                if href and href ~= "" then
                    return string.format("<a class='%s' href='%s'>%s</a>", classes.type, href, text)
                end
                return string.format("<span class='%s'>%s</span>", classes.type, text)
            end,
            function_name = formats.function_name or function(classes, text)
                return string.format("<span class='%s'>%s</span>", classes.function_name, text)
            end,
            literal = formats.literal or function(classes, text)
                return string.format("<span class='%s'>%s</span>", classes.literal, text)
            end,
            comment = formats.comment or function(classes, text)
                return string.format("<span class='%s'>%s</span>", classes.comment, text)
            end,
            attribute = formats.attribute or function(classes, text)
                return string.format("<span class='%s'>%s</span>", classes.attribute, text)
            end,
        }
    }
end

return M
