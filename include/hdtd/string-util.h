//
// Created by sjw on 2018/1/15.
//

#ifndef HDCONTENTS_HDTD_STRING_UTIL_H
#define HDCONTENTS_HDTD_STRING_UTIL_H

#include "hdtd/system.h"

/* The Unicode character used to incoming character whose value is unknown or unrepresentable. */
#define HD_REPLACEMENT_CHARACTER 0xFFFD

/*
	Safe string functions
*/

/*
	hd_strsep: Given a pointer to a C string (or a pointer to NULL) break
	it at the first occurrence of a delimiter char (from a given set).

	stringp: Pointer to a C string pointer (or NULL). Updated on exit to
	point to the first char of the string after the delimiter that was
	found. The string pointed to by stringp will be corrupted by this
	call (as the found delimiter will be overwritten by 0).

	delim: A C string of acceptable delimiter characters.

	Returns a pointer to a C string containing the chars of stringp up
	to the first delimiter char (or the end of the string), or NULL.
*/
char *hd_strsep(char **stringp, const char *delim);

/*
	hd_strlcpy: Copy at most n-1 chars of a string into a destination
	buffer with null termination, returning the real length of the
	initial string (excluding terminator).

	dst: Destination buffer, at least n bytes long.

	src: C string (non-NULL).

	n: Size of dst buffer in bytes.

	Returns the length (excluding terminator) of src.
*/
size_t hd_strlcpy(char *dst, const char *src, size_t n);

/*
	hd_strlcat: Concatenate 2 strings, with a maximum length.

	dst: pointer to first string in a buffer of n bytes.

	src: pointer to string to concatenate.

	n: Size (in bytes) of buffer that dst is in.

	Returns the real length that a concatenated dst + src would have been
	(not including terminator).
*/
size_t hd_strlcat(char *dst, const char *src, size_t n);

/*
	hd_dirname: extract the directory component from a path.
*/
void hd_dirname(char *dir, const char *path, size_t dirsize);

/*
	hd_urldecode: decode url escapes.
*/
char *hd_urldecode(char *url);

/*
	hd_format_output_path: create output file name using a template.
    If the path contains %[0-9]*d, the first such pattern will be replaced
	with the page number. If the template does not contain such a pattern, the page
	number will be inserted before the file suffix. If the template does not have
	a file suffix, the page number will be added to the end.
*/
void hd_format_output_path(hd_context *ctx, char *path, size_t size, const char *fmt, int page);

/*
	hd_cleanname: rewrite path to the shortest string that names the same path.

	Eliminates multiple and trailing slashes, interprets "." and "..".
	Overwrites the string in place.
*/
char *hd_cleanname(char *name);

/*
	FZ_UTFMAX: Maximum number of bytes in a decoded rune (maximum length returned by hd_chartorune).
*/
enum { FZ_UTFMAX = 4 };

/*
	hd_chartorune: UTF8 decode a single rune from a sequence of chars.

	rune: Pointer to an int to assign the decoded 'rune' to.

	str: Pointer to a UTF8 encoded string.

	Returns the number of bytes consumed. Does not throw exceptions.
*/
int hd_chartorune(int *rune, const char *str);

/*
	hd_runetochar: UTF8 encode a rune to a sequence of chars.

	str: Pointer to a place to put the UTF8 encoded character.

	rune: Pointer to a 'rune'.

	Returns the number of bytes the rune took to output. Does not throw
	exceptions.
*/
int hd_runetochar(char *str, int rune);

/*
	hd_runelen: Count how many chars are required to represent a rune.

	rune: The rune to encode.

	Returns the number of bytes required to represent this run in UTF8.
*/
int hd_runelen(int rune);

/*
	hd_utflen: Count how many runes the UTF-8 encoded string
	consists of.

	s: The UTF-8 encoded, NUL-terminated text string.

	Returns the number of runes in the string.
*/
int hd_utflen(const char *s);

/*
	hd_strtod/hd_strtof: Locale-independent decimal to binary
	conversion. On overflow return (-)INFINITY and set errno to ERANGE. On
	underflow return 0 and set errno to ERANGE. Special inputs (case
	insensitive): "NAN", "INF" or "INFINITY".
*/
double hd_strtod(const char *s, char **es);
float hd_strtof(const char *s, char **es);

/*
	hd_strtof_no_exp: Like hd_strtof, but does not recognize exponent
	format. So hd_strtof_no_exp("1.5e20", &tail) will return 1.5 and tail
	will point to "e20".
*/

float hd_strtof_no_exp(const char *string, char **tailptr);
/*
	hd_grisu: Compute decimal integer m, exp such that:
		f = m * 10^exp
		m is as short as possible without losing exactness
	Assumes special cases (0, NaN, +Inf, -Inf) have been handled.
*/
int hd_grisu(float f, char *s, int *exp);

/*
	Check and parse string into page ranges:
		( ','? ([0-9]+|'N') ( '-' ([0-9]+|N) )? )+
*/
int hd_is_page_range(hd_context *ctx, const char *s);
const char *hd_parse_page_range(hd_context *ctx, const char *s, int *a, int *b, int n);


#endif //HDCONTENTS_HDTD_STRING_UTIL_H
