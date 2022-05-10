/**
 * @file console.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Cross-platform console IO (thanks a lot, Terminal.app)
 * @version 0.9.1
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
	size_t column;
	size_t _column;
} cursor_pos_t;

typedef struct line_t
{
	const char *prompt;
	size_t prompt_len;
	char *line;
	size_t line_len;
	cursor_pos_t cursor;
	size_t console_width;
} line_t;

enum Console
{
	LINE_MAX_LEN = (1 << 12) - 1,
};

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
	const size_t cursor_rows = ctx->cursor.row - ((ctx->prompt_len + ctx->cursor._column + ctx->console_width) / ctx->console_width);
	if (cursor_rows) {
		move_cursor_pos(&esc_seq, MOVE_DOWN, cursor_rows);
	}

	for (size_t i = 0; i < ctx->cursor.row - 1; i++) {
		slice_append(&esc_seq, "\r" "\033[0K" "\033[1A", 9);
	}
	slice_append(&esc_seq, "\r" "\033[0K", 5);

	// Restore prompt and text
	slice_append(&esc_seq, ctx->prompt, ctx->prompt_len);
	slice_append(&esc_seq, ctx->line, ctx->line_len);

	const bool eol = (ctx->cursor.column && ctx->cursor.column == ctx->line_len);
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

	ctx->cursor._column = ctx->cursor.column;

	if (xwrite(STDOUT_FILENO, esc_seq.data, esc_seq.len) < 0) {
		xfree(esc_seq.data);
		exit(EXIT_FAILURE); // TODO: return an error code
	}
	xfree(esc_seq.data);
}

// Insert char 'c' at the current cursor position
static bool insert_char(line_t *ctx, char c)
{
	if (ctx->line_len > LINE_MAX_LEN) {
		return false;
	}

	if (ctx->line_len != ctx->cursor.column) { // Add char at pos
		memmove(&ctx->line[ctx->cursor.column + 1], &ctx->line[ctx->cursor.column], ctx->line_len - ctx->cursor.column);
	}

	ctx->line[ctx->cursor.column++] = c;
	ctx->line[++ctx->line_len] = '\0';
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
			ctx->cursor.column -= (ctx->cursor.column > 0) ? 1 : 0;
			break;
		case MOVE_RIGHT:
			ctx->cursor.column += (ctx->cursor.column != ctx->line_len) ? 1 : 0;
			break;
		case MOVE_HOME:
			ctx->cursor.column = 0;
			break;
		case MOVE_END:
			ctx->cursor.column = ctx->line_len;
			break;
	}
	flush_console(ctx);
}

static void delete_char(line_t *ctx, bool del)
{
	bool block = true;
	if (!del && ctx->cursor.column > 0 && ctx->line_len > 0) { // just backspace
		memmove(ctx->line + ctx->cursor.column - 1, ctx->line + ctx->cursor.column, ctx->line_len - ctx->cursor.column);
		ctx->cursor.column--;
		block = false;
	}
	if (del && ctx->line_len > 0 && ctx->cursor.column < ctx->line_len) { // delete
		memmove(ctx->line + ctx->cursor.column, ctx->line + ctx->cursor.column + 1, ctx->line_len - ctx->cursor.column - 1);
		block = false;
	}
	if (!block) {
		ctx->line[--ctx->line_len] = '\0';
		flush_console(ctx);
	}
}

// Length of a string exclusing ASCII control codes (for colors)
static size_t ansi_code_strlen(const char *str)
{
	size_t len = 0;
	for (size_t i = 0; str[i]; i++) {
		if (str[i] == ESC) {
			for (; str[i] != 'm'; i++) { };
		}
		len++;
	}
	return len;
}

static ssize_t xgetline(char *line, const char *prompt)
{
	line_t ctx = {
		.prompt = prompt,
		.prompt_len = ansi_code_strlen(prompt),
		.line = line,
		.console_width = xwinsize(),
	};

	while (1) {
		char c = xgetch();
		switch (c) {
			case NUL:
				return ctx.line_len;
			case TAB:
				flash_screen();
				break;
			case ENTER:
				if (ctx.line_len) {
					update_cursor_pos(&ctx, MOVE_END);
					return ctx.line_len;
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
				if (!insert_char(&ctx, c)) {
					return EOF;
				}
				break;
		}
	}
	return ctx.line_len;
}

char *_xprompt(const char *prompt, size_t *len)
{
	console_t orig;
	if (xtcsetattr(&orig, CONSOLE_MODE_RAW)) {
		return NULL;
	}

	char *line = xcalloc(LINE_MAX_LEN + 1);
	if (!line) {
		return NULL;
	}
	
	const ssize_t prompt_len = strlen(prompt);
	if (xwrite(STDOUT_FILENO, prompt, prompt_len) != prompt_len) {
		xfree(line);
		return NULL;
	}
	fflush(stdout);

	ssize_t line_length = xgetline(line, prompt);
	
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
