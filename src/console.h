/**
 * @file console.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Cross-platform console IO (thanks a lot, Terminal.app)
 * @version 0.9.2
 * @date 2022-05-29
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

typedef struct wchar_interval_t
{
	uint32_t first;
	uint32_t last;
} wchar_interval_t;

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

size_t utf8_rendered_length(const char *str);
size_t codepoint_width(const char *str, size_t len);
void sizeof_cp_prev(const char *str, size_t *cp_size, size_t *cp_len);
void sizeof_cp_next(const char *str, size_t *cp_size, size_t *cp_len);
