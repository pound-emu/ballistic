local log = require("log")

local M = {}

local THEMES = {
    dark = {
        ["--bg"] = "#0f1419",
        ["--sidebar-bg"] = "#14191f",
        ["--text"] = "#c5c5c5",
        ["--header-text"] = "#ffffff",
        ["--link"] = "#39afd7",
        ["--code-bg"] = "#191f26",
        ["--border"] = "#252c37",
        ["--sidebar-hover"] = "#1e252e",
        ["--sidebar-text"] = "#c5c5c5",
        ["--kw-color"] = "#ff7b72",
        ["--type-color"] = "#79c0ff",
        ["--fn-color"] = "#d2a8ff",
        ["--lit-color"] = "#a5d6ff",
        ["--comment-color"] = "#8b949e",
        ["--attr-color"] = "#7ee787",
        ["--badge-bg"] = "#21262d",
        ["--badge-text"] = "#8b949e",
        ["--callout-bg"] = "rgba(57, 175, 215, 0.1)",
        ["--callout-border"] = "#39afd7",
        ["--callout-safety-bg"] = "rgba(248, 81, 73, 0.12)",
        ["--callout-safety-border"] = "#f85149",
        ["--callout-error-bg"] = "rgba(240, 136, 62, 0.12)",
        ["--callout-error-border"] = "#f0883e",
    },
    light = {
        ["--bg"] = "#ffffff",
        ["--sidebar-bg"] = "#f5f5f5",
        ["--text"] = "#333333",
        ["--header-text"] = "#000000",
        ["--link"] = "#3873ad",
        ["--code-bg"] = "#f7f7f7",
        ["--border"] = "#e0e0e0",
        ["--sidebar-hover"] = "#e8e8e8",
        ["--sidebar-text"] = "#333333",
        ["--kw-color"] = "#8959a8",
        ["--type-color"] = "#4271ae",
        ["--fn-color"] = "#c82829",
        ["--lit-color"] = "#718c00",
        ["--comment-color"] = "#8e908c",
        ["--attr-color"] = "#3e999f",
        ["--badge-bg"] = "#eaeaea",
        ["--badge-text"] = "#555555",
        ["--callout-bg"] = "rgba(56, 115, 173, 0.08)",
        ["--callout-border"] = "#3873ad",
        ["--callout-safety-bg"] = "rgba(211, 47, 47, 0.08)",
        ["--callout-safety-border"] = "#d32f2f",
        ["--callout-error-bg"] = "rgba(245, 124, 0, 0.08)",
        ["--callout-error-border"] = "#f57c00",
    }
}

function M.theme_name(theme_state)
    return theme_state.name
end

function M.get_color(theme_state, name)
    return theme_state.colors[name] or "#ffffff"
end

function M.get_css_variables(theme_state)
    local lines = { ":root {" }

    for k, v in pairs(theme_state.colors) do
        table.insert(lines, string.format("    %s: %s;", k, v))
    end

    table.insert(lines, "}")
    return table.concat(lines, "\n")
end

function M.create(name)
    name = (name and THEMES[name:lower()]) and name:lower() or "dark"
    log.debug("Creating theme state for '%s'.", name)

    return {
        name = name,
        colors = THEMES[name]
    }
end

return M
