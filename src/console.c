/**
 * @file console.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Cross-platform console IO (thanks a lot, Terminal.app)
 * @ref https://www.asciitable.com/
 * @ref https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797
 * @ref https://en.wikipedia.org/wiki/ANSI_escape_code
 * @version 0.9.2
 * @date 2022-05-02
 *
 * @copyright Copyright (c) 2022 - 2024 Jason Conway. All rights reserved.
 *
 */

#include "console.h"

void clear_screen(void)
{
    WRITE_CONST_STR(STDOUT_FILENO, "\033[H\033[2J");
    fflush(stdout);
}

void ring_bell(void)
{
    WRITE_CONST_STR(STDOUT_FILENO, "\a");
    fflush(stdout);
}

static void update_row_count(line_t *ctx, size_t rows)
{
    if (rows > ctx->cursor.row) {
        ctx->cursor.row = rows;
    }
}

static void move_cursor_pos(slice_t *seq, uint8_t dir, size_t dist)
{
    char ascii[11] = { 0 };
    const size_t len = xutoa(dist, ascii);

    SLICE_APPEND_CONST_STR(seq, "\033[");
    slice_append(seq, ascii, len);

    uint8_t c = 'A' + dir;
    slice_append(seq, &c, 1);
}

static void flush_console(line_t *ctx)
{
    slice_t esc_seq = DYNAMIC_SLICE();

    size_t rows = (ctx->prompt_len + ctx->line_len + ctx->console_width - 1) / ctx->console_width;
    update_row_count(ctx, rows);
    const size_t cursor_rows = ctx->cursor.row - ((ctx->prompt_len + ctx->cursor.rendered_column + ctx->console_width) / ctx->console_width);
    if (cursor_rows) {
        move_cursor_pos(&esc_seq, MOVE_DOWN, cursor_rows);
    }

    // TODO: handle ctx->cursor.row == 0
    for (size_t i = 0; i < ctx->cursor.row - 1; i++) {
        SLICE_APPEND_CONST_STR(&esc_seq, "\r\033[0K\033[1A");
    }
    SLICE_APPEND_CONST_STR(&esc_seq, "\r\033[0K");

    // Restore prompt and text
    slice_append(&esc_seq, (char *)ctx->prompt, ctx->prompt_size);
    slice_append(&esc_seq, ctx->line, ctx->line_size);

    const bool eol = (ctx->cursor.offset && ctx->cursor.offset == ctx->line_size);
    const size_t columns = (ctx->prompt_len + ctx->cursor.column) % ctx->console_width;
    if (eol && !columns) {
        SLICE_APPEND_CONST_STR(&esc_seq, "\n\r");
        rows++;
        update_row_count(ctx, rows);
    }

    // Move cursor to correct row (when applicable)
    if (rows -= ((ctx->prompt_len + ctx->cursor.column + ctx->console_width) / ctx->console_width)) {
        move_cursor_pos(&esc_seq, MOVE_UP, rows);
    }

    // Move to left-hand side
    SLICE_APPEND_U8(&esc_seq, '\r');

    // Slide to correct column position
    if (columns) {
        move_cursor_pos(&esc_seq, MOVE_RIGHT, columns);
    }

    ctx->cursor.rendered_column = ctx->cursor.column;

    if (!full_write(STDOUT_FILENO, esc_seq.data, esc_seq.len)) {
        xfree(esc_seq.data);
        exit(EXIT_FAILURE); // TODO: return an error code
    }
    xfree(esc_seq.data);
}

// Insert a 'len' byte unicode char 'c' at the current cursor position
static bool insert_char(line_t *ctx, const uint8_t *c, size_t len)
{
    // TODO: someday I might implement zero-width modifiers
    const size_t cp_len = codepoint_width((char *)c, len);
    if (!cp_len) {
        return false;
    }

    if (ctx->line_size != ctx->cursor.offset) { // Add char at pos
        memmove(&ctx->line[ctx->cursor.offset + len], &ctx->line[ctx->cursor.offset], ctx->line_size - ctx->cursor.offset);
    }

    memcpy(&ctx->line[ctx->cursor.offset], c, len);
    ctx->cursor.offset += len;
    ctx->line_size += len;
    ctx->line_len += cp_len;
    ctx->cursor.column += cp_len;
    ctx->line[ctx->line_size] = '\0';

    flush_console(ctx);
    return true;
}

static inline bool move_cursor_right(line_t *ctx)
{
    if (ctx->cursor.offset != ctx->line_size) {
        size_t cp_size = 0;
        size_t cp_len = 0;
        next_codepoint(&ctx->line[ctx->cursor.offset], &cp_size, &cp_len);
        ctx->cursor.offset += cp_size;
        ctx->cursor.column += cp_len;
        return true;
    }
    return false;
}

static inline bool move_cursor_left(line_t *ctx)
{
    if (ctx->cursor.offset > 0) {
        size_t cp_size = 0;
        size_t cp_len = 0;
        prev_codepoint(&ctx->line[ctx->cursor.offset], &cp_size, &cp_len);
        ctx->cursor.offset -= cp_size;
        ctx->cursor.column -= cp_len;
        return true;
    }
    return false;
}

static inline void move_cursor_word_start(line_t *ctx)
{
    for (char *cp = prev_codepoint(&ctx->line[ctx->cursor.offset], NULL, NULL); *cp == ' ';) {
        if (!move_cursor_left(ctx)) {
            break;
        }
        cp = prev_codepoint(&ctx->line[ctx->cursor.offset], NULL, NULL);
    }
    for (char *cp = prev_codepoint(&ctx->line[ctx->cursor.offset], NULL, NULL); *cp != ' ';) {
        if (!move_cursor_left(ctx)) {
            break;
        }
        cp = prev_codepoint(&ctx->line[ctx->cursor.offset], NULL, NULL);
    }
}

static inline void move_cursor_word_end(line_t *ctx)
{
    while (ctx->cursor.offset < ctx->line_size) {
        if (!ctx->line[ctx->cursor.offset] || ctx->line[ctx->cursor.offset] != ' ') {
            break;
        }
        move_cursor_right(ctx);
    }
    // Eat whitespace
    while (ctx->cursor.offset < ctx->line_size) {
        if (!ctx->line[ctx->cursor.offset] || ctx->line[ctx->cursor.offset] == ' ') {
            break;
        }
        move_cursor_right(ctx);
    }
}

static void update_cursor_pos(line_t *ctx, enum cursor_direction direction)
{
    switch (direction) {
        case MOVE_UP: // TODO
        case MOVE_DOWN:
            break;
        case MOVE_RIGHT:
            move_cursor_right(ctx);
            break;
        case MOVE_LEFT:
            move_cursor_left(ctx);
            break;
        case MOVE_HOME:
            ctx->cursor.offset = 0;
            ctx->cursor.column = 0;
            break;
        case MOVE_END:
            ctx->cursor.offset = ctx->line_size;
            ctx->cursor.column = ctx->line_len;
            break;
        case JUMP_FORWARD:
            move_cursor_word_end(ctx);
            break;
        case JUMP_BACKWARD:
            move_cursor_word_start(ctx);
            break;
    }
    flush_console(ctx);
}

static void delete_char(line_t *ctx, bool del)
{
    size_t cp_size = 0;
    size_t cp_len = 0;
    bool block = true;

    if (!del && ctx->cursor.offset > 0 && ctx->line_size > 0) { // just backspace
        prev_codepoint(&ctx->line[ctx->cursor.offset], &cp_size, &cp_len);
        memmove(&ctx->line[ctx->cursor.offset - cp_size], &ctx->line[ctx->cursor.offset], ctx->line_size - ctx->cursor.offset);
        ctx->cursor.offset -= cp_size;
        ctx->cursor.column -= cp_len;
        block = false;
    }
    if (del && ctx->line_size > 0 && ctx->cursor.offset < ctx->line_size) { // delete
        next_codepoint(&ctx->line[ctx->cursor.offset], &cp_size, &cp_len);
        memmove(&ctx->line[ctx->cursor.offset], &ctx->line[ctx->cursor.offset + cp_size], ctx->line_size - ctx->cursor.offset - cp_size);
        block = false;
    }

    if (!block) {
        ctx->line_size -= cp_size;
        ctx->line[ctx->line_size] = '\0';
        ctx->line_len -= cp_len;
        flush_console(ctx);
    }
}

static ssize_t xgetline(char **line, const char *prompt)
{
    line_t ctx = {
        .prompt = prompt,
        .prompt_len = utf8_rendered_length(prompt),
        .prompt_size = strlen(prompt),
        .console_width = xwinsize(),
    };

    // Initial allocation
    size_t line_allocation = 64;
    ctx.line = xmalloc(line_allocation);
    if (!ctx.line) {
        return -1;
    }

    for (;;) {
        // Dynamically resize line buffer as needed
        if (ctx.line_size == line_allocation - 2) {
            if (line_allocation * 2 < line_allocation) {
                xfree(ctx.line);
                return -1;
            }
            if (!(ctx.line = xrealloc(ctx.line, line_allocation *= 2))) {
                return -1;
            }
        }

        // TODO: handle larger graphemes
        uint8_t c[4] = { 0 };
        size_t len = xgetcp(c);

        switch (*c) {
            case NUL:
                *line = ctx.line;
                return ctx.line_size;
            case TAB:
                ring_bell();
                break;
            case ENTER:
                if (ctx.line_size) {
                    update_cursor_pos(&ctx, MOVE_END);
                    *line = ctx.line;
                    return ctx.line_size;
                }
                ring_bell();
                break;
            case ESC: {
                const char seq[2] = { xgetch(), xgetch() };
                if (seq[0] == '[') {
                    if (seq[1] >= '0' && seq[1] <= '9') {
                        switch (xgetch()) {
                            case '~':
                                if (seq[1] == '3') {
                                    delete_char(&ctx, true);
                                }
                                break;
                            case ';':
                                if (xgetch() == '5') {
                                    switch (xgetch()) {
                                        case 'C':
                                            update_cursor_pos(&ctx, JUMP_FORWARD);
                                            break;
                                        case 'D':
                                            update_cursor_pos(&ctx, JUMP_BACKWARD);
                                            break;
                                    }
                                }
                                break;
                        }
                    }
                    switch (seq[1]) {
                        case 'D':
                            update_cursor_pos(&ctx, MOVE_LEFT);
                            break;
                        case 'C':
                            update_cursor_pos(&ctx, MOVE_RIGHT);
                            break;
                        case 'H':
                            update_cursor_pos(&ctx, MOVE_HOME);
                            break;
                        case 'F':
                            update_cursor_pos(&ctx, MOVE_END);
                            break;
                    }
                }
                break;
            }
            case BACKSPACE:
                delete_char(&ctx, false);
                break;
            default:
                insert_char(&ctx, c, len);
                break;
        }
    }

    return ctx.line_size;
}

static char *_xprompt(const char *prompt, size_t *len)
{
    console_t orig;
    if (xtcsetattr(&orig, CONSOLE_MODE_RAW)) {
        return NULL;
    }

    const ssize_t prompt_len = strlen(prompt);
    if (xwrite(STDOUT_FILENO, prompt, prompt_len) != prompt_len) {
        return NULL;
    }
    fflush(stdout);

    char *line = NULL;
    ssize_t line_length = xgetline(&line, prompt);
    if (!line) {
        return NULL;
    }

    if (xtcsetattr(&orig, CONSOLE_MODE_ORIG)) {
        xfree(line);
        return NULL;
    }

    if (xwrite(STDOUT_FILENO, "\n", 1) != 1) {
        xfree(line);
        return NULL;
    }

    if (line_length < 0) {
        xfree(line);
        return NULL;
    }
    *len = line_length;
    return line;
}

char *xprompt(const char *prompt_msg, const char *error_msg, size_t *len)
{
    char *line = NULL;
    size_t line_length = 0;

    for (;;) {
        line = NULL;
        line_length = 0;

        do {
            line = _xprompt(prompt_msg, &line_length);
        } while (!line);

        if (*len && line_length > *len) {
            xwarn("Maximum %s length is %zu bytes", error_msg, *len);
            xfree(line);
            continue;
        }
        break;
    }

    *len = line_length;
    return line;
}
