#define _POSIX_C_SOURCE 200809L
#include <clang-c/Index.h>
#include <cmark.h>
#include <ctype.h>
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

// --- 1. DATA STRUCTURES ---

typedef enum
{
    KIND_FUNCTION,
    KIND_STRUCT,
    KIND_ENUM,
    KIND_TYPEDEF
} ItemKind;

typedef struct
{
    char *name;
    char *type; // For Structs: type name. For Enums: value.
    char *doc;
} Field;

typedef struct
{
    char    *name;
    char    *doc_comment;
    ItemKind kind;
    char    *return_type;
    char    *underlying_type;
    Field   *args;
    Field   *fields;
    int      arg_count;
    int      field_count;
    char    *source_file;
    char    *anchor_id;
} DocItem;

typedef struct
{
    DocItem *items;
    size_t   count;
    size_t   capacity;
    char    *filename;
    char    *file_doc;
} FileContext;

typedef struct
{
    FileContext *files;
    size_t       count;
    size_t       capacity;
    DocItem    **registry;
    size_t       reg_count;
    size_t       reg_cap;
} ProjectContext;

// --- 2. HELPERS ---

char *
my_strdup(const char *s)
{
    if (!s)
    {
        return NULL;
    }
    size_t len = strlen(s);
    char  *d   = malloc(len + 1);
    if (d)
    {
        memcpy(d, s, len + 1);
    }
    return d;
}

char *
to_cstr(CXString cx_str)
{
    const char *temp   = clang_getCString(cx_str);
    char       *result = temp ? my_strdup(temp) : my_strdup("");
    clang_disposeString(cx_str);
    return result;
}

const char *
get_filename(const char *path)
{
    const char *last_slash     = strrchr(path, '/');
    const char *last_backslash = strrchr(path, '\\');
    const char *filename       = path;
    if (last_slash && last_slash > filename)
    {
        filename = last_slash + 1;
    }
    if (last_backslash && last_backslash > filename)
    {
        filename = last_backslash + 1;
    }
    return filename;
}

void
init_project(ProjectContext *proj)
{
    proj->count     = 0;
    proj->capacity  = 10;
    proj->files     = malloc(sizeof(FileContext) * proj->capacity);
    proj->reg_count = 0;
    proj->reg_cap   = 100;
    proj->registry  = malloc(sizeof(DocItem *) * proj->reg_cap);
}

FileContext *
add_file(ProjectContext *proj, const char *filepath)
{
    if (proj->count >= proj->capacity)
    {
        proj->capacity *= 2;
        proj->files = realloc(proj->files, sizeof(FileContext) * proj->capacity);
    }
    FileContext *ctx = &proj->files[proj->count++];
    ctx->filename    = my_strdup(get_filename(filepath));
    ctx->file_doc    = NULL;
    ctx->count       = 0;
    ctx->capacity    = 32;
    ctx->items       = malloc(sizeof(DocItem) * ctx->capacity);
    return ctx;
}

void
register_item(ProjectContext *proj, DocItem *item)
{
    if (proj->reg_count >= proj->reg_cap)
    {
        proj->reg_cap *= 2;
        proj->registry = realloc(proj->registry, sizeof(DocItem *) * proj->reg_cap);
    }
    proj->registry[proj->reg_count++] = item;
}

DocItem *
add_item(FileContext *ctx)
{
    if (ctx->count >= ctx->capacity)
    {
        ctx->capacity *= 2;
        ctx->items = realloc(ctx->items, sizeof(DocItem) * ctx->capacity);
    }
    DocItem *item = &ctx->items[ctx->count++];
    memset(item, 0, sizeof(DocItem));
    item->source_file = ctx->filename;
    return item;
}

int
item_exists(FileContext *ctx, const char *name)
{
    if (!name)
    {
        return 0;
    }
    for (size_t i = 0; i < ctx->count; i++)
    {
        if (ctx->items[i].name && strcmp(ctx->items[i].name, name) == 0)
        {
            return 1;
        }
    }
    return 0;
}

// --- 3. LINK RESOLUTION ---
// --- 3. LINK RESOLUTION ---

// NEW: Helper to find any symbol (Item, Enum Variant, or Struct.Field)
// Returns 1 if found, setting out_file and out_anchor. 0 otherwise.
int
find_link_target(ProjectContext *proj,
                 const char     *name,
                 const char    **out_file,
                 const char    **out_anchor)
{
    if (!name)
    {
        return 0;
    }

    for (size_t i = 0; i < proj->reg_count; i++)
    {
        DocItem *item = proj->registry[i];

        // 1. Check Top-Level Item Name (Struct, Enum, Function, Typedef)
        if (item->name && strcmp(item->name, name) == 0)
        {
            *out_file   = item->source_file;
            *out_anchor = item->anchor_id;
            return 1;
        }

        // 2. Check Enum Variants (Global Scope in C)
        // Allows linking to [`SOME_ENUM_VALUE`]
        if (item->kind == KIND_ENUM)
        {
            for (int j = 0; j < item->field_count; j++)
            {
                if (item->fields[j].name && strcmp(item->fields[j].name, name) == 0)
                {
                    *out_file   = item->source_file;
                    *out_anchor = item->fields[j].name; // Anchor is the variant name
                    return 1;
                }
            }
        }

        // 3. Check Struct Fields (Scoped: StructName.FieldName)
        // Allows linking to [`MyStruct.my_field`]
        if (item->kind == KIND_STRUCT && item->name)
        {
            size_t len = strlen(item->name);
            if (strncmp(name, item->name, len) == 0 && name[len] == '.')
            {
                const char *field_part = name + len + 1;
                for (int j = 0; j < item->field_count; j++)
                {
                    if (item->fields[j].name && strcmp(item->fields[j].name, field_part) == 0)
                    {
                        *out_file   = item->source_file;
                        *out_anchor = item->fields[j].name;
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

// Kept for internal logic if needed, but mostly replaced by find_link_target
DocItem *
find_item(ProjectContext *proj, const char *name)
{
    if (!name)
    {
        return NULL;
    }
    for (size_t i = 0; i < proj->reg_count; i++)
    {
        if (proj->registry[i]->name && strcmp(proj->registry[i]->name, name) == 0)
        {
            return proj->registry[i];
        }
    }
    return NULL;
}

int
is_ident_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

char *
linkify_type(ProjectContext *proj, const char *raw_type, const char *current_file)
{
    if (!raw_type)
    {
        return NULL;
    }

    size_t cap     = strlen(raw_type) * 3 + 256;
    char  *out     = malloc(cap);
    size_t out_len = 0;
    out[0]         = '\0';

    const char *p = raw_type;
    char        word[256];
    int         w_idx = 0;

    while (*p)
    {
        if (is_ident_char(*p))
        {
            if (w_idx < 255)
            {
                word[w_idx++] = *p;
            }
        }
        else
        {
            if (w_idx > 0)
            {
                word[w_idx] = '\0';

                const char *target_file, *target_anchor;
                if (find_link_target(proj, word, &target_file, &target_anchor))
                {
                    // Check if same file for relative link
                    if (current_file && strcmp(target_file, current_file) == 0)
                    {
                        out_len += snprintf(out + out_len,
                                            cap - out_len,
                                            "<a class='type' href='#%s'>%s</a>",
                                            target_anchor,
                                            word);
                    }
                    else
                    {
                        out_len += snprintf(out + out_len,
                                            cap - out_len,
                                            "<a class='type' href='%s.html#%s'>%s</a>",
                                            target_file,
                                            target_anchor,
                                            word);
                    }
                }
                else
                {
                    out_len += snprintf(out + out_len, cap - out_len, "%s", word);
                }
                w_idx = 0;
            }
            if (out_len < cap - 1)
            {
                out[out_len++] = *p;
                out[out_len]   = '\0';
            }
        }
        p++;
    }

    if (w_idx > 0)
    {
        word[w_idx] = '\0';
        const char *target_file, *target_anchor;
        if (find_link_target(proj, word, &target_file, &target_anchor))
        {
            if (current_file && strcmp(target_file, current_file) == 0)
            {
                out_len += snprintf(out + out_len,
                                    cap - out_len,
                                    "<a class='type' href='#%s'>%s</a>",
                                    target_anchor,
                                    word);
            }
            else
            {
                out_len += snprintf(out + out_len,
                                    cap - out_len,
                                    "<a class='type' href='%s.html#%s'>%s</a>",
                                    target_file,
                                    target_anchor,
                                    word);
            }
        }
        else
        {
            out_len += snprintf(out + out_len, cap - out_len, "%s", word);
        }
    }

    return out;
}

char *
resolve_links(ProjectContext *proj, const char *text, const char *current_file)
{
    if (!text)
    {
        return NULL;
    }
    size_t      cap     = strlen(text) * 2 + 512;
    char       *output  = malloc(cap);
    size_t      out_len = 0;
    const char *p       = text;

    while (*p)
    {
        if (p[0] == '[' && p[1] == '`')
        {
            const char *start = p + 2;
            const char *end   = strstr(start, "`]");
            if (end)
            {
                int  name_len = end - start;
                char name[128];
                if (name_len < 127)
                {
                    strncpy(name, start, name_len);
                    name[name_len] = '\0';

                    const char *target_file, *target_anchor;
                    if (find_link_target(proj, name, &target_file, &target_anchor))
                    {
                        if (current_file && strcmp(target_file, current_file) == 0)
                        {
                            out_len
                                += sprintf(output + out_len, "[`%s`](#%s)", name, target_anchor);
                        }
                        else
                        {
                            out_len += sprintf(output + out_len,
                                               "[`%s`](%s.html#%s)",
                                               name,
                                               target_file,
                                               target_anchor);
                        }
                        p = end + 2;
                        continue;
                    }
                }
            }
        }
        output[out_len++] = *p++;
        if (out_len >= cap - 100)
        {
            cap *= 2;
            output = realloc(output, cap);
        }
    }
    output[out_len] = '\0';
    return output;
}

char *
clean_comment(const char *raw)
{
    if (!raw)
    {
        return NULL;
    }
    size_t len     = strlen(raw);
    char  *output  = malloc(len + 1);
    char  *out_ptr = output;
    char  *temp    = my_strdup(raw);
    char  *line    = strtok(temp, "\n");

    while (line != NULL)
    {
        char *p = line;
        while (*p && isspace((unsigned char)*p))
        {
            p++;
        }
        if (strncmp(p, "/**", 3) == 0)
        {
            p += 3;
        }
        else if (strncmp(p, "/*!", 3) == 0)
        {
            p += 3;
        }
        else if (strncmp(p, "///", 3) == 0)
        {
            p += 3;
        }
        else if (strncmp(p, "//!", 3) == 0)
        {
            p += 3;
        }
        else if (strncmp(p, "*/", 2) == 0)
        {
            line = strtok(NULL, "\n");
            continue;
        }
        else if (*p == '*')
        {
            p++;
        }
        if (*p == ' ')
        {
            p++;
        }

        if (strncmp(p, "// ---", 6) == 0)
        {
            line = strtok(NULL, "\n");
            continue;
        }

        while (*p)
        {
            *out_ptr++ = *p++;
        }
        *out_ptr++ = '\n';
        line       = strtok(NULL, "\n");
    }
    *out_ptr = '\0';
    free(temp);
    return output;
}

// --- 4. PARSING ---

void
parse_file_level_docs(FileContext *ctx, const char *real_path)
{
    FILE *f = fopen(real_path, "r");
    if (!f)
    {
        return;
    }
    char *buffer = malloc(50000);
    if (!buffer)
    {
        fclose(f);
        return;
    }
    buffer[0] = '\0';
    char line[2048];
    while (fgets(line, sizeof(line), f))
    {
        char *p = line;
        while (*p && isspace((unsigned char)*p))
        {
            p++;
        }
        if (strncmp(p, "//!", 3) == 0)
        {
            p += 3;
            if (*p == ' ')
            {
                p++;
            }
            if (strncmp(p, "// ---", 6) == 0)
            {
                continue;
            }
            strcat(buffer, p);
        }
    }
    fclose(f);
    if (strlen(buffer) > 0)
    {
        ctx->file_doc = buffer;
    }
    else
    {
        free(buffer);
    }
}

int
is_skippable(CXCursor cursor)
{
    if (clang_Cursor_isAnonymous(cursor))
    {
        return 1;
    }
    CXString    cx   = clang_getCursorSpelling(cursor);
    const char *s    = clang_getCString(cx);
    int         skip = (!s || strlen(s) == 0 || strstr(s, "(unnamed") != NULL);
    clang_disposeString(cx);
    return skip;
}

enum CXChildVisitResult
struct_visitor(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
    DocItem *item = (DocItem *)client_data;
    if (clang_getCursorKind(cursor) == CXCursor_FieldDecl)
    {
        int idx                = item->field_count++;
        item->fields           = realloc(item->fields, sizeof(Field) * item->field_count);
        item->fields[idx].name = to_cstr(clang_getCursorSpelling(cursor));
        item->fields[idx].type = to_cstr(clang_getTypeSpelling(clang_getCursorType(cursor)));
        item->fields[idx].doc  = to_cstr(clang_Cursor_getRawCommentText(cursor));
    }
    return CXChildVisit_Continue;
}

enum CXChildVisitResult
enum_visitor(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
    DocItem *item = (DocItem *)client_data;
    if (clang_getCursorKind(cursor) == CXCursor_EnumConstantDecl)
    {
        int idx                = item->field_count++;
        item->fields           = realloc(item->fields, sizeof(Field) * item->field_count);
        item->fields[idx].name = to_cstr(clang_getCursorSpelling(cursor));
        long long val          = clang_getEnumConstantDeclValue(cursor);
        char      val_str[64];
        snprintf(val_str, 64, "%lld", val);
        item->fields[idx].type = my_strdup(val_str);
        item->fields[idx].doc  = to_cstr(clang_Cursor_getRawCommentText(cursor));
    }
    return CXChildVisit_Continue;
}

enum CXChildVisitResult
typedef_param_visitor(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
    DocItem *item = (DocItem *)client_data;
    if (clang_getCursorKind(cursor) == CXCursor_ParmDecl)
    {
        int idx              = item->arg_count++;
        item->args           = realloc(item->args, sizeof(Field) * item->arg_count);
        item->args[idx].name = to_cstr(clang_getCursorSpelling(cursor));
        item->args[idx].type = to_cstr(clang_getTypeSpelling(clang_getCursorType(cursor)));
        item->args[idx].doc  = NULL;
    }
    return CXChildVisit_Continue;
}

enum CXChildVisitResult
main_visitor(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
    void          **args = (void **)client_data;
    FileContext    *ctx  = (FileContext *)args[0];
    ProjectContext *proj = (ProjectContext *)args[1];

    CXSourceLocation location = clang_getCursorLocation(cursor);
    if (clang_Location_isFromMainFile(location) == 0)
    {
        return CXChildVisit_Continue;
    }

    enum CXCursorKind kind = clang_getCursorKind(cursor);
    DocItem          *item = NULL;

    if (kind == CXCursor_FunctionDecl)
    {
        char *name = to_cstr(clang_getCursorSpelling(cursor));
        if (item_exists(ctx, name))
        {
            free(name);
            return CXChildVisit_Continue;
        }

        item              = add_item(ctx);
        item->kind        = KIND_FUNCTION;
        item->name        = name;
        item->doc_comment = to_cstr(clang_Cursor_getRawCommentText(cursor));
        item->return_type = to_cstr(clang_getTypeSpelling(clang_getCursorResultType(cursor)));

        char buf[256];
        snprintf(buf, 256, "fn.%s", item->name ? item->name : "unknown");
        item->anchor_id = my_strdup(buf);

        int num         = clang_Cursor_getNumArguments(cursor);
        item->arg_count = num;
        item->args      = malloc(sizeof(Field) * num);
        for (int i = 0; i < num; i++)
        {
            CXCursor arg       = clang_Cursor_getArgument(cursor, i);
            item->args[i].name = to_cstr(clang_getCursorSpelling(arg));
            item->args[i].type = to_cstr(clang_getTypeSpelling(clang_getCursorType(arg)));
            item->args[i].doc  = NULL;
        }
    }
    else if (kind == CXCursor_StructDecl)
    {
        if (is_skippable(cursor) || !clang_isCursorDefinition(cursor))
        {
            return CXChildVisit_Continue;
        }

        char *name = to_cstr(clang_getCursorSpelling(cursor));
        if (item_exists(ctx, name))
        {
            free(name);
            return CXChildVisit_Continue;
        }

        item              = add_item(ctx);
        item->kind        = KIND_STRUCT;
        item->name        = name;
        item->doc_comment = to_cstr(clang_Cursor_getRawCommentText(cursor));

        char buf[256];
        snprintf(buf, 256, "struct.%s", item->name ? item->name : "unknown");
        item->anchor_id = my_strdup(buf);

        clang_visitChildren(cursor, struct_visitor, item);
    }
    else if (kind == CXCursor_EnumDecl)
    {
        if (is_skippable(cursor) || !clang_isCursorDefinition(cursor))
        {
            return CXChildVisit_Continue;
        }

        char *name = to_cstr(clang_getCursorSpelling(cursor));
        if (item_exists(ctx, name))
        {
            free(name);
            return CXChildVisit_Continue;
        }

        item              = add_item(ctx);
        item->kind        = KIND_ENUM;
        item->name        = name;
        item->doc_comment = to_cstr(clang_Cursor_getRawCommentText(cursor));

        char buf[256];
        snprintf(buf, 256, "enum.%s", item->name ? item->name : "unknown");
        item->anchor_id = my_strdup(buf);

        clang_visitChildren(cursor, enum_visitor, item);
    }
    else if (kind == CXCursor_TypedefDecl)
    {
        char *name = to_cstr(clang_getCursorSpelling(cursor));
        if (item_exists(ctx, name))
        {
            free(name);
            return CXChildVisit_Continue;
        }

        item       = add_item(ctx);
        item->name = name;

        CXType underlying = clang_getTypedefDeclUnderlyingType(cursor);
        CXType canonical  = clang_getCanonicalType(underlying);

        if (canonical.kind == CXType_Record)
        {
            item->kind = KIND_STRUCT;
            char buf[256];
            snprintf(buf, 256, "struct.%s", item->name ? item->name : "unknown");
            item->anchor_id = my_strdup(buf);

            char *doc = to_cstr(clang_Cursor_getRawCommentText(cursor));
            if (!doc || !*doc)
            {
                if (doc)
                {
                    free(doc);
                }
                CXCursor sc = clang_getTypeDeclaration(canonical);
                doc         = to_cstr(clang_Cursor_getRawCommentText(sc));
            }
            item->doc_comment = doc;

            CXCursor sc = clang_getTypeDeclaration(canonical);
            clang_visitChildren(sc, struct_visitor, item);
        }
        else if (canonical.kind == CXType_Enum)
        {
            item->kind = KIND_ENUM;
            char buf[256];
            snprintf(buf, 256, "enum.%s", item->name ? item->name : "unknown");
            item->anchor_id = my_strdup(buf);

            char *doc = to_cstr(clang_Cursor_getRawCommentText(cursor));
            if (!doc || !*doc)
            {
                if (doc)
                {
                    free(doc);
                }
                CXCursor sc = clang_getTypeDeclaration(canonical);
                doc         = to_cstr(clang_Cursor_getRawCommentText(sc));
            }
            item->doc_comment = doc;

            CXCursor sc = clang_getTypeDeclaration(canonical);
            clang_visitChildren(sc, enum_visitor, item);
        }
        else
        {
            item->kind = KIND_TYPEDEF;
            char buf[256];
            snprintf(buf, 256, "type.%s", item->name ? item->name : "unknown");
            item->anchor_id       = my_strdup(buf);
            item->doc_comment     = to_cstr(clang_Cursor_getRawCommentText(cursor));
            item->underlying_type = to_cstr(clang_getTypeSpelling(underlying));

            // UPDATED: Check if it's a function pointer and extract params
            if (underlying.kind == CXType_Pointer)
            {
                CXType pointee = clang_getPointeeType(underlying);
                if (pointee.kind == CXType_FunctionProto)
                {
                    item->return_type
                        = to_cstr(clang_getTypeSpelling(clang_getResultType(pointee)));
                    // Visit children to find ParmDecl parameters
                    clang_visitChildren(cursor, typedef_param_visitor, item);
                }
            }
        }
    }

    if (item)
    {
        register_item(proj, item);
    }

    return CXChildVisit_Recurse;
}

// --- 5. HTML GENERATION ---

void
render_md(FILE *f, ProjectContext *proj, const char *text, const char *current_file)
{
    if (!text)
    {
        return;
    }
    char *linked = resolve_links(proj, text, current_file);
    char *html   = cmark_markdown_to_html(linked, strlen(linked), CMARK_OPT_UNSAFE);
    fprintf(f, "%s", html);
    free(html);
    free(linked);
}

void
write_common_head(FILE *f, const char *title)
{
    fprintf(f, "<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'>");
    fprintf(f, "<title>%s</title>", title);
    fprintf(f,
            "<link rel='stylesheet' "
            "href='https://fonts.googleapis.com/"
            "css2?family=Fira+Sans:wght@400;500&family=Source+Code+Pro:wght@400;600&family=Source+"
            "Serif+4:wght@400;600;700&display=swap'>");
    fprintf(f, "<style>");
    fprintf(f,
            ":root { --bg: #0f1419; --sidebar-bg: #14191f; --text: #c5c5c5; --link: #39afd7; "
            "--code-bg: #191f26; --border: #252c37; --header-text: #fff; }");
    fprintf(f,
            "body { font-family: 'Source Serif 4', serif; font-size: 16px; background: var(--bg); "
            "color: var(--text); margin: 0; display: flex; height: 100vh; overflow: hidden; "
            "line-height: 1.6; }");
    fprintf(f,
            ".sidebar { width: 250px; background: var(--sidebar-bg); border-right: 1px solid "
            "var(--border); overflow-y: auto; padding: 20px; flex-shrink: 0; }");
    fprintf(
        f, ".main { flex: 1; padding: 40px; overflow-y: auto; max-width: 960px; margin: 0 auto; }");
    fprintf(f,
            ".sidebar a { display: block; color: var(--text); text-decoration: none; font-family: "
            "'Fira Sans', sans-serif; font-size: 14px; margin: 6px 0; }");
    fprintf(f, ".sidebar a:hover { color: var(--link); background: #222; border-radius: 3px; }");
    fprintf(f,
            ".sidebar h3 { font-family: 'Fira Sans'; font-size: 14px; color: #fff; margin-top: "
            "20px; text-transform: uppercase; font-weight: 500; }");
    fprintf(f,
            "h1 { font-size: 28px; color: var(--header-text); margin-bottom: 20px; border-bottom: "
            "1px solid var(--border); padding-bottom: 10px; }");
    fprintf(f,
            "h2 { font-size: 24px; color: var(--header-text); margin-top: 50px; border-bottom: 1px "
            "solid var(--border); padding-bottom: 5px; font-weight: 600; }");
    fprintf(f,
            "h3 { font-size: 20px; color: var(--header-text); margin-top: 30px; margin-bottom: "
            "15px; font-weight: 600; }");
    fprintf(
        f,
        "a { color: var(--link); text-decoration: none; } a:hover { text-decoration: underline; }");
    fprintf(f,
            "pre { width: 100%%; box-sizing: border-box; background: var(--code-bg); padding: "
            "15px; border-radius: 6px; overflow-x: auto; font-size: 14px; line-height: 1.5; "
            "border: 1px solid var(--border); }");
    fprintf(f,
            "code { font-family: 'Source Code Pro', monospace; background: var(--code-bg); "
            "padding: 0.1em 0.3em; border-radius: 4px; font-size: 0.875em; }");
    fprintf(f,
            ".item-decl { width: 100%%; box-sizing: border-box; background: var(--code-bg); "
            "padding: 15px; font-family: 'Source Code Pro'; margin-bottom: 20px; border-radius: "
            "6px; white-space: pre-wrap; overflow-x: auto; font-size: 14px; line-height: 1.5; "
            "color: #e6e6e6; border: 1px solid var(--border); }");
    fprintf(f,
            ".item-decl a.type { color: #79c0ff; text-decoration: none; border-bottom: 1px dotted "
            "#555; }");
    fprintf(f, ".item-decl a.type:hover { border-bottom: 1px solid #79c0ff; }");
    fprintf(f, ".kw { color: #ff7b72; font-weight: bold; }");
    fprintf(f, ".type { color: #79c0ff; }");
    fprintf(f, ".fn { color: #d2a8ff; font-weight: bold; }");
    fprintf(f, ".lit { color: #a5d6ff; }");
    fprintf(f, ".field-item { margin-bottom: 15px; }");
    fprintf(f,
            ".field-name { font-family: 'Source Code Pro', monospace; font-size: 16px; "
            "font-weight: 600; color: #fff; background: var(--code-bg); padding: 2px 6px; "
            "border-radius: 4px; display: inline-block; }");
    fprintf(f,
            ".field-doc * { margin-top: 6px; margin-left: 10px; color: #ccc; font-size: 16px; "
            "line-height: 1.5; }");
    fprintf(f, ".field-doc { margin: 0; }");
    fprintf(f, ".docblock { margin-top: 10px; margin-bottom: 30px; font-size: 16px; }");
    fprintf(f,
            ".docblock h1 { font-size: 18px; font-weight: 600; margin-top: 25px; margin-bottom: "
            "10px; border-bottom: none; color: var(--header-text); }");
    fprintf(f,
            ".docblock h2 { font-size: 17px; font-weight: 600; margin-top: 25px; margin-bottom: "
            "10px; border-bottom: none; color: var(--header-text); }");
    fprintf(f,
            ".docblock h3 { font-size: 16px; font-weight: 600; margin-top: 20px; margin-bottom: "
            "10px; }");
    fprintf(f, ".docblock p { margin-bottom: 1em; }");
    fprintf(f, ".docblock ul { padding-left: 20px; margin-bottom: 1em; }");
    fprintf(f, "</style></head><body>");
}

int
compare_item_ptrs(const void *a, const void *b)
{
    const DocItem *da = *(const DocItem **)a;
    const DocItem *db = *(const DocItem **)b;

    if (!da->name && !db->name)
    {
        return 0;
    }
    if (!da->name)
    {
        return 1;
    }
    if (!db->name)
    {
        return -1;
    }

    return strcmp(da->name, db->name);
}

void
render_sidebar_section(FILE *f, FileContext *ctx, ItemKind kind, const char *title)
{
    size_t count = 0;
    // Count items of this kind
    for (size_t i = 0; i < ctx->count; i++)
    {
        if (ctx->items[i].kind == kind)
        {
            count++;
        }
    }

    if (count == 0)
    {
        return;
    }

    // Create a temporary array of pointers
    DocItem **ptrs = malloc(sizeof(DocItem *) * count);
    size_t    idx  = 0;
    for (size_t i = 0; i < ctx->count; i++)
    {
        if (ctx->items[i].kind == kind)
        {
            ptrs[idx++] = &ctx->items[i];
        }
    }

    // Sort the pointers alphabetically for the sidebar
    qsort(ptrs, count, sizeof(DocItem *), compare_item_ptrs);

    // Render sorted links
    fprintf(f, "<h3>%s</h3>", title);
    for (size_t i = 0; i < count; i++)
    {
        fprintf(f, "<a href='#%s'>%s</a>", ptrs[i]->anchor_id, ptrs[i]->name);
    }

    free(ptrs);
}

void
generate_file_html(ProjectContext *proj, FileContext *ctx, const char *out_dir)
{
    char path[1024];
    snprintf(path, 1024, "%s/%s.html", out_dir, ctx->filename);
    FILE *f = fopen(path, "w");
    if (!f)
    {
        return;
    }

    write_common_head(f, ctx->filename);

    // --- SIDEBAR GENERATION ---
    fprintf(f, "<nav class='sidebar'>");
    fprintf(f,
            "<a href='index.html' style='font-size: 18px; font-weight: bold; margin-bottom: "
            "20px;'>Back to Index</a>");
    fprintf(f,
            "<div style='font-weight: bold; color: #fff; margin-bottom: 10px;'>%s</div>",
            ctx->filename);

    // CHANGED: Group items by kind in the sidebar
    render_sidebar_section(f, ctx, KIND_STRUCT, "Structs");
    render_sidebar_section(f, ctx, KIND_ENUM, "Enums");
    render_sidebar_section(f, ctx, KIND_FUNCTION, "Functions");
    render_sidebar_section(f, ctx, KIND_TYPEDEF, "Type Aliases");

    fprintf(f, "</nav>");
    // --------------------------

    fprintf(f, "<main class='main'>");
    fprintf(f, "<h1>Header <span class='fn'>%s</span></h1>", ctx->filename);

    if (ctx->file_doc)
    {
        fprintf(f, "<div class='docblock'>");
        render_md(f, proj, ctx->file_doc, ctx->filename);
        fprintf(f, "</div>");
    }

    for (size_t i = 0; i < ctx->count; i++)
    {
        DocItem    *item     = &ctx->items[i];
        const char *kind_str = "Unknown";
        if (item->kind == KIND_FUNCTION)
        {
            kind_str = "Function";
        }
        else if (item->kind == KIND_STRUCT)
        {
            kind_str = "Struct";
        }
        else if (item->kind == KIND_ENUM)
        {
            kind_str = "Enum";
        }
        else if (item->kind == KIND_TYPEDEF)
        {
            kind_str = "Type Alias";
        }

        fprintf(f,
                "<h2 id='%s'>%s <a href='#%s'>%s</a></h2>",
                item->anchor_id,
                kind_str,
                item->anchor_id,
                item->name);

        fprintf(f, "<div class='item-decl'>");
        if (item->kind == KIND_FUNCTION)
        {
            char *ret_linked = linkify_type(proj, item->return_type, ctx->filename);
            fprintf(f,
                    "<span class='type'>%s</span> <span class='fn'>%s</span>(",
                    ret_linked,
                    item->name);
            free(ret_linked);

            for (int j = 0; j < item->arg_count; j++)
            {
                char *arg_linked = linkify_type(proj, item->args[j].type, ctx->filename);
                fprintf(f, "\n    <span class='type'>%s</span> %s", arg_linked, item->args[j].name);
                free(arg_linked);
                if (j < item->arg_count - 1)
                {
                    fprintf(f, ",");
                }
            }
            fprintf(f, "\n)");
        }
        else if (item->kind == KIND_STRUCT)
        {
            fprintf(f, "<span class='kw'>struct</span> <span class='type'>%s</span> {", item->name);
            for (int j = 0; j < item->field_count; j++)
            {
                char *f_linked = linkify_type(proj, item->fields[j].type, ctx->filename);
                fprintf(
                    f, "\n    <span class='type'>%s</span> %s;", f_linked, item->fields[j].name);
                free(f_linked);
            }
            fprintf(f, "\n}");
        }
        else if (item->kind == KIND_ENUM)
        {
            fprintf(f, "<span class='kw'>enum</span> <span class='type'>%s</span> {", item->name);
            for (int j = 0; j < item->field_count; j++)
            {
                fprintf(f,
                        "\n    %s = <span class='lit'>%s</span>,",
                        item->fields[j].name,
                        item->fields[j].type);
            }
            fprintf(f, "\n}");
        }
        else
        {
            if (item->return_type && item->arg_count > 0)
            {
                char *ret_linked = linkify_type(proj, item->return_type, ctx->filename);
                fprintf(f,
                        "<span class='kw'>typedef</span> %s = <span class='type'>%s</span> (*)(",
                        item->name,
                        ret_linked);
                free(ret_linked);

                for (int j = 0; j < item->arg_count; j++)
                {
                    char *arg_linked = linkify_type(proj, item->args[j].type, ctx->filename);
                    fprintf(f, "<span class='type'>%s</span> %s", arg_linked, item->args[j].name);
                    free(arg_linked);
                    if (j < item->arg_count - 1)
                    {
                        fprintf(f, ", ");
                    }
                }
                fprintf(f, ");");
            }
            else
            {
                char *under_linked = linkify_type(proj, item->underlying_type, ctx->filename);
                fprintf(f,
                        "<span class='kw'>typedef</span> %s = <span class='type'>%s</span>;",
                        item->name,
                        under_linked);
                free(under_linked);
            }
        }
        fprintf(f, "</div>");

        if (item->doc_comment)
        {
            fprintf(f, "<div class='docblock'>");
            char *clean = clean_comment(item->doc_comment);
            render_md(f, proj, clean, ctx->filename);
            fprintf(f, "</div>");
        }

        if ((item->kind == KIND_STRUCT || item->kind == KIND_ENUM) && item->field_count > 0)
        {
            int has_field_docs = 0;
            for (int k = 0; k < item->field_count; k++)
            {
                if (item->fields[k].doc)
                {
                    has_field_docs = 1;
                }
            }

            if (has_field_docs)
            {
                fprintf(f, "<h3>%s</h3>", item->kind == KIND_ENUM ? "Variants" : "Fields");
                for (int j = 0; j < item->field_count; j++)
                {
                    if (item->fields[j].doc)
                    {
                        fprintf(f, "<div id='%s' class='field-item'>", item->fields[j].name);
                        fprintf(f, "<code class='field-name'>%s</code>", item->fields[j].name);
                        fprintf(f, "<div class='field-doc'>");
                        char *clean = clean_comment(item->fields[j].doc);
                        render_md(f, proj, clean, ctx->filename);
                        free(clean);
                        fprintf(f, "</div></div>");
                    }
                }
            }
        }
    }
    fprintf(f, "</main></body></html>");
    fclose(f);
}

void
generate_index(ProjectContext *proj, const char *out_dir)
{
    char path[1024];
    snprintf(path, 1024, "%s/index.html", out_dir);
    FILE *f = fopen(path, "w");
    if (!f)
    {
        return;
    }

    write_common_head(f, "Project Documentation");

    fprintf(f, "<nav class='sidebar'><h3>Files</h3>");
    for (size_t i = 0; i < proj->count; i++)
    {
        fprintf(f, "<a href='%s.html'>%s</a>", proj->files[i].filename, proj->files[i].filename);
    }
    fprintf(f, "</nav>");

    fprintf(f, "<main class='main'>");
    fprintf(f, "<h1>Project Documentation</h1>");
    fprintf(f, "<h2>Headers</h2><ul>");
    for (size_t i = 0; i < proj->count; i++)
    {
        fprintf(f,
                "<li><a href='%s.html'>%s</a></li>",
                proj->files[i].filename,
                proj->files[i].filename);
    }
    fprintf(f, "</ul>");

    fprintf(f, "<h2>Global Symbols</h2><div style='display:flex; flex-wrap:wrap; gap: 10px;'>");
    for (size_t i = 0; i < proj->reg_count; i++)
    {
        DocItem *item = proj->registry[i];
        if (item->name)
        {
            fprintf(f,
                    "<a href='%s.html#%s' style='background: #222; padding: 5px 10px; "
                    "border-radius: 4px;'>%s</a>",
                    item->source_file,
                    item->anchor_id,
                    item->name);
        }
    }
    fprintf(f, "</div>");
    fprintf(f, "</main></body></html>");
    fclose(f);
}

int
dir_exists(const char *path)
{
    struct stat sb;
    return stat(path, &sb) == 0 && S_ISDIR(sb.st_mode);
}

int
compare_items(const void *a, const void *b)
{
    const DocItem *da = (const DocItem *)a;
    const DocItem *db = (const DocItem *)b;

    // Safety checks for NULL names
    if (!da->name && !db->name)
    {
        return 0;
    }
    if (!da->name)
    {
        return 1;
    }
    if (!db->name)
    {
        return -1;
    }

    return strcmp(da->name, db->name);
}

int
main(int argc, char **argv)
{
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    if (argc < 3)
    {
        printf("Usage: %s <output_dir> <file1.h> [file2.h ...]\n", argv[0]);
        return 1;
    }

    const char *out_dir = argv[1];

    char  clang_inc_path[1024] = { 0 };
    int   found_inc            = 0;
    FILE *p                    = popen("clang -print-resource-dir", "r");
    if (p)
    {
        if (fgets(clang_inc_path, sizeof(clang_inc_path), p))
        {
            size_t len = strlen(clang_inc_path);
            while (len > 0 && isspace((unsigned char)clang_inc_path[len - 1]))
            {
                clang_inc_path[--len] = '\0';
            }
            strcat(clang_inc_path, "/include");
            if (dir_exists(clang_inc_path))
            {
                found_inc = 1;
            }
        }
        pclose(p);
    }

    if (!found_inc)
    {
        const char *common_paths[] = { "/usr/lib/clang/18/include",
                                       "/usr/lib/clang/17/include",
                                       "/usr/lib/clang/16/include",
                                       "/usr/lib/clang/15/include",
                                       "/usr/lib/clang/14/include",
                                       "/usr/lib64/clang/18/include",
                                       NULL };
        for (int i = 0; common_paths[i]; i++)
        {
            if (dir_exists(common_paths[i]))
            {
                strcpy(clang_inc_path, common_paths[i]);
                found_inc = 1;
                break;
            }
        }
    }

    char        arg_include_flag[1100];
    const char *clang_args[8];
    int         num_args   = 0;
    clang_args[num_args++] = "-I.";
    clang_args[num_args++] = "-Iinclude";
    clang_args[num_args++] = "-xc";
    if (found_inc)
    {
        snprintf(arg_include_flag, sizeof(arg_include_flag), "-I%s", clang_inc_path);
        clang_args[num_args++] = arg_include_flag;
        printf("Using Clang headers: %s\n", clang_inc_path);
    }

    ProjectContext proj;
    init_project(&proj);
    CXIndex index = clang_createIndex(0, 1);

    for (int i = 2; i < argc; i++)
    {
        const char *filepath = argv[i];
        printf("Parsing %s...\n", filepath);

        FileContext *file_ctx = add_file(&proj, filepath);
        parse_file_level_docs(file_ctx, filepath);

        CXTranslationUnit unit = clang_parseTranslationUnit(
            index, filepath, clang_args, num_args, NULL, 0, CXTranslationUnit_None);

        if (!unit)
        {
            printf("Failed to parse %s\n", filepath);
            continue;
        }

        CXCursor root   = clang_getTranslationUnitCursor(unit);
        void    *args[] = { file_ctx, &proj };
        clang_visitChildren(root, main_visitor, args);
        clang_disposeTranslationUnit(unit);
    }

    printf("Generating HTML in '%s'...\n", out_dir);

#ifdef _WIN32
    _mkdir(out_dir);
#else
    mkdir(out_dir, 0777);
#endif

    for (size_t i = 0; i < proj.count; i++)
    {
        generate_file_html(&proj, &proj.files[i], out_dir);
    }
    generate_index(&proj, out_dir);

    clang_disposeIndex(index);
    printf("Done! Open %s/index.html\n", out_dir);
    return 0;
}
