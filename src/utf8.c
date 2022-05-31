/**
 * @file utf8.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Minimal UTF-8 handling for `console`
 * @version 0.9.2
 * @date 2022-05-29
 *
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 *
 */

#include "console.h"

static size_t cp_rendered_width(uint32_t c)
{
	// TODO: emojis with skin tones break things
	static const wchar_interval_t wide[] = {
		{ 0x00001100, 0x0000115f }, { 0x0000231a, 0x0000231b }, { 0x00002329, 0x0000232a }, { 0x000023e9, 0x000023ec },
		{ 0x000023f0, 0x000023f0 }, { 0x000023f3, 0x000023f3 }, { 0x000025fd, 0x000025fe }, { 0x00002614, 0x00002615 },
		{ 0x00002648, 0x00002653 }, { 0x0000267f, 0x0000267f }, { 0x00002693, 0x00002693 }, { 0x000026a1, 0x000026a1 },
		{ 0x000026aa, 0x000026ab }, { 0x000026bd, 0x000026be }, { 0x000026c4, 0x000026c5 }, { 0x000026ce, 0x000026ce },
		{ 0x000026d4, 0x000026d4 }, { 0x000026ea, 0x000026ea }, { 0x000026f2, 0x000026f3 }, { 0x000026f5, 0x000026f5 },
		{ 0x000026fa, 0x000026fa }, { 0x000026fd, 0x000026fd }, { 0x00002705, 0x00002705 }, { 0x0000270a, 0x0000270b },
		{ 0x00002728, 0x00002728 }, { 0x0000274c, 0x0000274c }, { 0x0000274e, 0x0000274e }, { 0x00002753, 0x00002755 },
		{ 0x00002757, 0x00002757 }, { 0x00002795, 0x00002797 }, { 0x000027b0, 0x000027b0 }, { 0x000027bf, 0x000027bf },
		{ 0x00002b1b, 0x00002b1c }, { 0x00002b50, 0x00002b50 }, { 0x00002b55, 0x00002b55 }, { 0x00002e80, 0x00002e99 },
		{ 0x00002e9b, 0x00002ef3 }, { 0x00002f00, 0x00002fd5 }, { 0x00002ff0, 0x00002ffb }, { 0x00003000, 0x0000303e },
		{ 0x00003041, 0x00003096 }, { 0x00003099, 0x000030ff }, { 0x00003105, 0x0000312f }, { 0x00003131, 0x0000318e },
		{ 0x00003190, 0x000031e3 }, { 0x000031f0, 0x0000321e }, { 0x00003220, 0x00003247 }, { 0x00003250, 0x00004dbf },
		{ 0x00004e00, 0x0000a48c }, { 0x0000a490, 0x0000a4c6 }, { 0x0000a960, 0x0000a97c }, { 0x0000ac00, 0x0000d7a3 },
		{ 0x0000f900, 0x0000faff }, { 0x0000fe10, 0x0000fe19 }, { 0x0000fe30, 0x0000fe52 }, { 0x0000fe54, 0x0000fe66 },
		{ 0x0000fe68, 0x0000fe6b }, { 0x0000ff01, 0x0000ff60 }, { 0x0000ffe0, 0x0000ffe6 }, { 0x00016fe0, 0x00016fe4 },
		{ 0x00016ff0, 0x00016ff1 }, { 0x00017000, 0x000187f7 }, { 0x00018800, 0x00018cd5 }, { 0x00018d00, 0x00018d08 },
		{ 0x0001aff0, 0x0001aff3 }, { 0x0001aff5, 0x0001affb }, { 0x0001affd, 0x0001affe }, { 0x0001b000, 0x0001b122 },
		{ 0x0001b150, 0x0001b152 }, { 0x0001b164, 0x0001b167 }, { 0x0001b170, 0x0001b2fb }, { 0x0001f004, 0x0001f004 },
		{ 0x0001f0cf, 0x0001f0cf }, { 0x0001f18e, 0x0001f18e }, { 0x0001f191, 0x0001f19a }, { 0x0001f200, 0x0001f202 },
		{ 0x0001f210, 0x0001f23b }, { 0x0001f240, 0x0001f248 }, { 0x0001f250, 0x0001f251 }, { 0x0001f260, 0x0001f265 },
		{ 0x0001f300, 0x0001f320 }, { 0x0001f32d, 0x0001f335 }, { 0x0001f337, 0x0001f37c }, { 0x0001f37e, 0x0001f393 },
		{ 0x0001f3a0, 0x0001f3ca }, { 0x0001f3cf, 0x0001f3d3 }, { 0x0001f3e0, 0x0001f3f0 }, { 0x0001f3f4, 0x0001f3f4 },
		{ 0x0001f3f8, 0x0001f43e }, { 0x0001f440, 0x0001f440 }, { 0x0001f442, 0x0001f4fc }, { 0x0001f4ff, 0x0001f53d },
		{ 0x0001f54b, 0x0001f54e }, { 0x0001f550, 0x0001f567 }, { 0x0001f57a, 0x0001f57a }, { 0x0001f595, 0x0001f596 },
		{ 0x0001f5a4, 0x0001f5a4 }, { 0x0001f5fb, 0x0001f64f }, { 0x0001f680, 0x0001f6c5 }, { 0x0001f6cc, 0x0001f6cc },
		{ 0x0001f6d0, 0x0001f6d2 }, { 0x0001f6d5, 0x0001f6d7 }, { 0x0001f6dd, 0x0001f6df }, { 0x0001f6eb, 0x0001f6ec },
		{ 0x0001f6f4, 0x0001f6fc }, { 0x0001f7e0, 0x0001f7eb }, { 0x0001f7f0, 0x0001f7f0 }, { 0x0001f90c, 0x0001f93a },
		{ 0x0001f93c, 0x0001f945 }, { 0x0001f947, 0x0001f9ff }, { 0x0001fa70, 0x0001fa74 }, { 0x0001fa78, 0x0001fa7c },
		{ 0x0001fa80, 0x0001fa86 }, { 0x0001fa90, 0x0001faac }, { 0x0001fab0, 0x0001faba }, { 0x0001fac0, 0x0001fac5 },
		{ 0x0001fad0, 0x0001fad9 }, { 0x0001fae0, 0x0001fae7 }, { 0x0001faf0, 0x0001faf6 }, { 0x00020000, 0x0002fffd },
		{ 0x00030000, 0x0003fffd }
	};

	if (!c) {
		return 0;
	}

	for (size_t i = 0; i < (sizeof(wide) / sizeof(*wide)); i++) {
		if (c >= wide[i].first && c <= wide[i].last) {
			return 2;
		}
	}

	return 1;
}

static inline uint32_t utf8_to_32(const unsigned char *c, size_t cp_size)
{
	switch (cp_size) {
	case 0:
		return 0;
	case 1:
		return *c;
	case 2:
		return (((c[0] & 0x1f) << 0x06) |
				((c[1] & 0x3f) << 0x00));
	case 3:
		return (((c[0] & 0x0f) << 0x0c) |
				((c[1] & 0x3f) << 0x06) |
				((c[2] & 0x3f) << 0x00));
	case 4:
		return (((c[0] & 0x07) << 0x12) |
				((c[1] & 0x3f) << 0x0c) |
				((c[2] & 0x3f) << 0x06) |
				((c[3] & 0x3f) << 0x00));
	}
	return 0;
}

size_t codepoint_width(const char *str, size_t len)
{
	const unsigned char *s = (const unsigned char *)str;
	const uint32_t cp = utf8_to_32(s, len);
	return cp_rendered_width(cp);
}

void sizeof_cp_prev(const char *str, size_t *cp_size, size_t *cp_len)
{
	const char *s = (const char *)str;
	*cp_size = 0; // Bytes in codepoint
	do {
		*cp_size += 1;
		s--;
	} while (((s[0] & 0x80)) && ((s[0] & 0xc0) == 0x80));

	*cp_len = codepoint_width(s, *cp_size);
}

void sizeof_cp_next(const char *str, size_t *cp_size, size_t *cp_len)
{
	if ((str[0] & 0xf8) == 0xf0) { // 11110xxx
		*cp_size = 4;
	}
	else if ((str[0] & 0xf0) == 0xe0) { // 1110xxxx
		*cp_size = 3;
	}
	else if ((str[0] & 0xe0) == 0xc0) { // 110xxxxx
		*cp_size = 2;
	}
	else {
		*cp_size = 1;
	}

	*cp_len = codepoint_width(str, *cp_size);
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

size_t utf8_rendered_length(const char *str)
{
	char *stripped = xcalloc(strlen(str));
	if (!stripped) {
		return 0;
	}

	size_t len = 0;
	for (size_t i = 0; str[i]; i++) {
		if (str[i] == ESC) {
			for (; str[i] != 'm'; i++) {
			};
		}
		stripped[len++] = str[i];
	}

	len = utf8_strnlen(stripped, len);
	xfree(stripped);
	return len;
}
