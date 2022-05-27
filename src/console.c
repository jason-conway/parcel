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
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 * 
 */

#include "xplatform.h"
#include "xutils.h"

typedef struct cursor_pos_t
{
	size_t row;
	size_t offset; // Cursor index in bytes
	size_t column; // Cursor index in characters
	size_t rendered_column; // Active cursor column
} cursor_pos_t;

typedef struct line_t
{
	const char *prompt;
	size_t prompt_len; // Rendered length
	size_t prompt_size; // Prompt size in bytes
	char *line;
	size_t line_len; // Rendered length
	size_t line_size; // Line size in bytes
	cursor_pos_t cursor;
	size_t console_width;
} line_t;

enum KeyCodes
{
	NUL = 0,
	BEL = 7,
	BS = 8,
	TAB = 9,
	ENTER = 13,
	ESC = 27,
	BACKSPACE = 127
};

enum cursor_direction
{
	MOVE_UP,
	MOVE_DOWN,
	MOVE_RIGHT,
	MOVE_LEFT,
	MOVE_HOME,
	MOVE_END
};

void clear_screen(void)
{
	(void)xwrite(STDOUT_FILENO, "\033[H\033[2J", 7);
	fflush(stdout);
}

// TODO: restore title on exit
bool set_title(void)
{
	static const char esc_seq[26] = {"\033]2;parcel v" STR(PARCEL_VERSION) "\a"};
	const ssize_t len = strlen(esc_seq);
	if (xwrite(STDIN_FILENO, esc_seq, len) != len) {
		return false;
	}
	return true;
}

static void flash_screen(void)
{
	(void)xwrite(STDOUT_FILENO, "\a", 1);
	fflush(stdout);
}

static size_t sizeof_cp_prev(const char *str)
{
	const char *s = (const char *)str;
	size_t len = 0; // Bytes in codepoint
	do {
		len++;
		s--;
	} while (((s[0] & 0x80)) && ((s[0] & 0xc0) == 0x80));

	return len;
}

static size_t sizeof_cp_next(const char *str)
{
	if ((str[0] & 0xf8) == 0xf0) { // 11110xxx
		return 4;
	}
	else if ((str[0] & 0xf0) == 0xe0) { // 1110xxxx
		return 3;
	}
	else if ((str[0] & 0xe0) == 0xc0) { // 110xxxxx
		return 2;
	}
	return 1;
}

// Length of rendered unicode string
static size_t utf8_strnlen(const char *str, size_t len)
{
	const char *s = str;
	size_t length = 0;

	while ((size_t)(str - s) < len && str[0]) {
		if ((str[0] & 0xf8) == 0xf0) { // 11110xxx
			str += 4;
		}
		else if ((str[0] & 0xf0) == 0xe0) { // 1110xxxx
			str += 3;
		}
		else if ((str[0] & 0xe0) == 0xc0) { // 110xxxxx
			str += 2;
		}
		else {
			str += 1;
		}

		length++;
	}

	return ((size_t)(str - s) > len) ? length - 1 : length;
}

static size_t utf8_rendered_length(const char *str)
{
	char *stripped = xcalloc(strlen(str));
	if (!stripped) {
		return 0;
	}

	size_t len = 0;
	for (size_t i = 0; str[i]; i++) {
		if (str[i] == ESC) {
			for (; str[i] != 'm'; i++) { };
		}
		stripped[len++] = str[i];
	}
	
	len = utf8_strnlen(stripped, len);
	xfree(stripped);
	return len;
}

static void update_row_count(line_t *ctx, size_t rows)
{
	if (rows > ctx->cursor.row) {
		ctx->cursor.row = rows;
	}
}

static void move_cursor_pos(slice_t *seq, enum cursor_direction direction, unsigned int distance)
{
	char ascii[11] = { 0 };
	const size_t len = xutoa(distance, ascii);
	const char esc_code[2] = {'A' + (char)direction };

	slice_append(seq, "\033[", 2);
	slice_append(seq, ascii, len);
	slice_append(seq, esc_code, 1);
}

static void flush_console(line_t *ctx)
{
	slice_t esc_seq = { NULL, 0 };

	size_t rows = (ctx->prompt_len + ctx->line_len + ctx->console_width - 1) / ctx->console_width;
	update_row_count(ctx, rows);
	const size_t cursor_rows = ctx->cursor.row - ((ctx->prompt_len + ctx->cursor.rendered_column + ctx->console_width) / ctx->console_width);
	if (cursor_rows) {
		move_cursor_pos(&esc_seq, MOVE_DOWN, cursor_rows);
	}

	for (size_t i = 0; i < ctx->cursor.row - 1; i++) {
		slice_append(&esc_seq, "\r" "\033[0K" "\033[1A", 9);
	}
	slice_append(&esc_seq, "\r" "\033[0K", 5);

	// Restore prompt and text
	slice_append(&esc_seq, ctx->prompt, ctx->prompt_size);
	slice_append(&esc_seq, ctx->line, ctx->line_size);

	const bool eol = (ctx->cursor.offset && ctx->cursor.offset == ctx->line_size);
	const size_t columns = (ctx->prompt_len + ctx->cursor.column) % ctx->console_width;
	if (eol && !columns) {
		slice_append(&esc_seq, "\n\r", 2);
		rows++;
		update_row_count(ctx, rows);
	}

	// Move cursor to correct row (when applicable)
	if ((rows -= ((ctx->prompt_len + ctx->cursor.column + ctx->console_width) / ctx->console_width))) {
		move_cursor_pos(&esc_seq, MOVE_UP, rows);
	}
	
	// Move to left-hand side
	slice_append(&esc_seq, "\r", 1);
	
	// Slide to correct column position
	if (columns) {
		move_cursor_pos(&esc_seq, MOVE_RIGHT, columns);
	}

	ctx->cursor.rendered_column = ctx->cursor.column;

	if (xwrite(STDOUT_FILENO, esc_seq.data, esc_seq.len) < 0) {
		xfree(esc_seq.data);
		exit(EXIT_FAILURE); // TODO: return an error code
	}
	xfree(esc_seq.data);
}

// Insert a 'len' byte unicode char 'c' at the current cursor position
static bool insert_char(line_t *ctx, const unsigned char *c, size_t len)
{
	if (ctx->line_size != ctx->cursor.offset) { // Add char at pos
		memmove(&ctx->line[ctx->cursor.offset + len], &ctx->line[ctx->cursor.offset], ctx->line_size - ctx->cursor.offset);
	}

	memcpy(&ctx->line[ctx->cursor.offset], c, len);
	ctx->cursor.offset += len;
	ctx->line_size += len;
	ctx->line_len++;
	ctx->cursor.column++;
	ctx->line[ctx->line_size] = '\0';
	
	flush_console(ctx);
	return true;
}

static void update_cursor_pos(line_t *ctx, enum cursor_direction direction)
{
	switch (direction) {
		case MOVE_UP: // TODO
		case MOVE_DOWN:
			break;
		case MOVE_LEFT:
			if (ctx->cursor.offset > 0) {
				ctx->cursor.offset -= sizeof_cp_prev(&ctx->line[ctx->cursor.offset]);
				ctx->cursor.column--;
			}
			break;
		case MOVE_RIGHT:
			if (ctx->cursor.offset != ctx->line_size) {
				ctx->cursor.offset += sizeof_cp_next(&ctx->line[ctx->cursor.offset]);
				ctx->cursor.column++;
			}
			break;
		case MOVE_HOME:
			ctx->cursor.offset = 0;
			ctx->cursor.column = 0;
			break;
		case MOVE_END:
			ctx->cursor.offset = ctx->line_size;
			ctx->cursor.column = ctx->line_len;
			break;
	}
	flush_console(ctx);
}

static void delete_char(line_t *ctx, bool del)
{
	size_t cp_size;
	bool block = true;

	if (!del && ctx->cursor.offset > 0 && ctx->line_size > 0) { // just backspace
		cp_size = sizeof_cp_prev(&ctx->line[ctx->cursor.offset]);
		memmove(&ctx->line[ctx->cursor.offset - cp_size], &ctx->line[ctx->cursor.offset], ctx->line_size - ctx->cursor.offset);
		ctx->cursor.offset -= cp_size;
		ctx->cursor.column--;
		block = false;
	}
	if (del && ctx->line_size > 0 && ctx->cursor.offset < ctx->line_size) { // delete
		cp_size = sizeof_cp_next(&ctx->line[ctx->cursor.offset]);
		memmove(&ctx->line[ctx->cursor.offset], &ctx->line[ctx->cursor.offset + cp_size], ctx->line_size - ctx->cursor.offset - cp_size);
		block = false;
	}
	if (!block) {
		ctx->line_size -= cp_size;
		ctx->line[ctx->line_size] = '\0';
		ctx->line_len--;
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

	while (1) {
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
		// char c = xgetch();
		unsigned char c[4] = {0, 0, 0, 0};
		size_t len = xgetcp(c);

		switch (*c) {
			case NUL:
				*line = ctx.line;
				return ctx.line_size;
			case TAB:
				flash_screen();
				break;
			case ENTER:
				if (ctx.line_size) {
					update_cursor_pos(&ctx, MOVE_END);
					*line = ctx.line;
					return ctx.line_size;
				}
				flash_screen();
				break;
			case ESC: {
				const char seq[2] = { xgetch(), xgetch() };
				switch (seq[0]) {
					case '[':
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
							case '3':
								if (xgetch() == '~') {
									delete_char(&ctx, true);
								}
								break;
							default:
								break;
						}
						break;
				}
			} break;
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

char *_xprompt(const char *prompt, size_t *len)
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
