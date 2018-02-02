//
// Created by sjw on 2018/1/18.
//
#include "hdtd.h"
#include "pdf.h"

#define IS_NUMBER \
	'+':case'-':case'.':case'0':case'1':case'2':case'3':\
	case'4':case'5':case'6':case'7':case'8':case'9'
#define IS_WHITE \
	'\x00':case'\x09':case'\x0a':case'\x0c':case'\x0d':case'\x20'
#define IS_HEX \
	'0':case'1':case'2':case'3':case'4':case'5':case'6':\
	case'7':case'8':case'9':case'A':case'B':case'C':\
	case'D':case'E':case'F':case'a':case'b':case'c':\
	case'd':case'e':case'f'
#define IS_DELIM \
	'(':case')':case'<':case'>':case'[':case']':case'{':\
	case'}':case'/':case'%'

#define RANGE_0_9 \
	'0':case'1':case'2':case'3':case'4':case'5':\
	case'6':case'7':case'8':case'9'
#define RANGE_a_f \
	'a':case'b':case'c':case'd':case'e':case'f'
#define RANGE_A_F \
	'A':case'B':case'C':case'D':case'E':case'F'
#define RANGE_0_7 \
	'0':case'1':case'2':case'3':case'4':case'5':case'6':case'7'

static inline int iswhite(int ch)
{
    return
            ch == '\000' ||
            ch == '\011' ||
            ch == '\012' ||
            ch == '\014' ||
            ch == '\015' ||
            ch == '\040';
}

static inline int hd_isprint(int ch)
{
    return ch >= ' ' && ch <= '~';
}

static inline int unhex(int ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 0xA;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 0xA;
    return 0;
}

static void
lex_white(hd_context *ctx, hd_stream *f)
{
	int c;
	do {
		c = hd_read_byte(ctx, f);
	} while ((c <= 32) && (iswhite(c)));
	if (c != EOF)
		hd_unread_byte(ctx, f);
}

static void
lex_comment(hd_context *ctx, hd_stream *f)
{
	int c;
	do {
		c = hd_read_byte(ctx, f);
	} while ((c != '\012') && (c != '\015') && (c != EOF));
}

void pdf_lexbuf_init(hd_context *ctx, pdf_lexbuf *lb, int size)
{
    lb->size = lb->base_size = size;
    lb->len = 0;
    lb->scratch = &lb->buffer[0];
}

void pdf_lexbuf_fin(hd_context *ctx, pdf_lexbuf *lb)
{
	if (lb && lb->size != lb->base_size)
		hd_free(ctx, lb->scratch);
}

int pdf_lexbuf_grow(hd_context *ctx, pdf_lexbuf *lb)
{
    char *old = lb->scratch;
    int newsize = lb->size * 2;
    if (lb->size == lb->base_size)
    {
        lb->scratch = hd_malloc(ctx, newsize);
        memcpy(lb->scratch, lb->buffer, lb->size);
    }
    else
    {
        lb->scratch = hd_resize_array(ctx, lb->scratch, newsize, 1);
    }
    lb->size = newsize;
    return lb->scratch - old;
}

/* Fast(ish) but inaccurate strtof, with Adobe overflow handling. */
static float acrobat_compatible_atof(char *s)
{
	int neg = 0;
	int i = 0;

	while (*s == '-')
	{
		neg = 1;
		++s;
	}
	while (*s == '+')
	{
		++s;
	}

	while (*s >= '0' && *s <= '9')
	{
		/* We deliberately ignore overflow here.
		 * Tests show that Acrobat handles * overflows in exactly the same way we do:
		 * 123450000000000000000678 is read as 678.
		 */
		i = i * 10 + (*s - '0');
		++s;
	}

	if (*s == '.')
	{
		float v = i;
		float n = 0;
		float d = 1;
		++s;
		while (*s >= '0' && *s <= '9')
		{
			n = 10 * n + (*s - '0');
			d = 10 * d;
			++s;
		}
		v += n / d;
		return neg ? -v : v;
	}
	else
	{
		return neg ? -i : i;
	}
}

/* Fast but inaccurate atoi. */
static int fast_atoi(char *s)
{
	int neg = 0;
	int i = 0;

	while (*s == '-')
	{
		neg = 1;
		++s;
	}
	while (*s == '+')
	{
		++s;
	}

	while (*s >= '0' && *s <= '9')
	{
		/* We deliberately ignore overflow here. */
		i = i * 10 + (*s - '0');
		++s;
	}

	return neg ? -i : i;
}

static int
lex_number(hd_context *ctx, hd_stream *f, pdf_lexbuf *buf, int c)
{
	char *s = buf->scratch;
	char *e = buf->scratch + buf->size - 1; /* leave space for zero terminator */
	char *isreal = (c == '.' ? s : NULL);
	int neg = (c == '-');
	int isbad = 0;

	*s++ = c;

	c = hd_read_byte(ctx, f);

	/* skip extra '-' signs at start of number */
	if (neg)
	{
		while (c == '-')
			c = hd_read_byte(ctx, f);
	}

	while (s < e)
	{
		switch (c)
		{
			case IS_WHITE:
			case IS_DELIM:
				hd_unread_byte(ctx, f);
				goto end;
			case EOF:
				goto end;
			case '.':
				if (isreal)
					isbad = 1;
				isreal = s;
				*s++ = c;
				break;
			case RANGE_0_9:
				*s++ = c;
				break;
			default:
				isbad = 1;
				*s++ = c;
				break;
		}
		c = hd_read_byte(ctx, f);
	}

	end:
	*s = '\0';
	if (isbad)
		return PDF_TOK_ERROR;
	if (isreal)
	{
		/* We'd like to use the fastest possible atof
		 * routine, but we'd rather match acrobats
		 * handling of broken numbers. As such, we
		 * spot common broken cases and call an
		 * acrobat compatible routine where required. */
		if (neg > 1 || isreal - buf->scratch >= 10)
			buf->f = acrobat_compatible_atof(buf->scratch);
		else
			buf->f = atof(buf->scratch);
		return PDF_TOK_REAL;
	}
	else
	{
		buf->i = fast_atoi(buf->scratch);
		return PDF_TOK_INT;
	}
}
static inline int hd_mini(int a, int b)
{
	return (a < b ? a : b);
}

//XXX
static void
lex_name(hd_context *ctx, hd_stream *f, pdf_lexbuf *lb)
{
	char *s = lb->scratch;
	char *e = s + hd_mini(127, lb->size);
	int c;

	while (1)
	{
		if (s == e)
		{
			if (e - lb->scratch >= 127)
				hd_throw(ctx, HD_ERROR_SYNTAX, "name too long");
			s += pdf_lexbuf_grow(ctx, lb);
			e = lb->scratch + hd_mini(127, lb->size);
		}
		c = hd_read_byte(ctx, f);
		switch (c)
		{
			case IS_WHITE:
			case IS_DELIM:
				hd_unread_byte(ctx, f);
				goto end;
			case EOF:
				goto end;
			case '#':
			{
				int hex[2];
				int i;
				for (i = 0; i < 2; i++)
				{
					c = hd_peek_byte(ctx, f);
					switch (c)
					{
						case RANGE_0_9:
							if (i == 1 && c == '0' && hex[0] == 0)
								goto illegal;
							hex[i] = hd_read_byte(ctx, f) - '0';
							break;
						case RANGE_a_f:
							hex[i] = hd_read_byte(ctx, f) - 'a' + 10;
							break;
						case RANGE_A_F:
							hex[i] = hd_read_byte(ctx, f) - 'A' + 10;
							break;
						default:
						case EOF:
							goto illegal;
					}
				}
				*s++ = (hex[0] << 4) + hex[1];
				break;
				illegal:
				if (i == 1)
					hd_unread_byte(ctx, f);
				*s++ = '#';
				continue;
			}
			default:
				*s++ = c;
				break;
		}
	}
	end:
	*s = '\0';
	lb->len = s - lb->scratch;
}

static int
lex_string(hd_context *ctx, hd_stream *f, pdf_lexbuf *lb)
{
	char *s = lb->scratch;
	char *e = s + lb->size;
	int bal = 1;
	int oct;
	int c;

	while (1)
	{
		if (s == e)
		{
			s += pdf_lexbuf_grow(ctx, lb);
			e = lb->scratch + lb->size;
		}
		c = hd_read_byte(ctx, f);
		switch (c)
		{
			case EOF:
				goto end;
			case '(':
				bal++;
				*s++ = c;
				break;
			case ')':
				bal --;
				if (bal == 0)
					goto end;
				*s++ = c;
				break;
			case '\\':
				c = hd_read_byte(ctx, f);
				switch (c)
				{
					case EOF:
						goto end;
					case 'n':
						*s++ = '\n';
						break;
					case 'r':
						*s++ = '\r';
						break;
					case 't':
						*s++ = '\t';
						break;
					case 'b':
						*s++ = '\b';
						break;
					case 'f':
						*s++ = '\f';
						break;
					case '(':
						*s++ = '(';
						break;
					case ')':
						*s++ = ')';
						break;
					case '\\':
						*s++ = '\\';
						break;
					case RANGE_0_7:
						oct = c - '0';
						c = hd_read_byte(ctx, f);
						if (c >= '0' && c <= '7')
						{
							oct = oct * 8 + (c - '0');
							c = hd_read_byte(ctx, f);
							if (c >= '0' && c <= '7')
								oct = oct * 8 + (c - '0');
							else if (c != EOF)
								hd_unread_byte(ctx, f);
						}
						else if (c != EOF)
							hd_unread_byte(ctx, f);
						*s++ = oct;
						break;
					case '\n':
						break;
					case '\r':
						c = hd_read_byte(ctx, f);
						if ((c != '\n') && (c != EOF))
							hd_unread_byte(ctx, f);
						break;
					default:
						*s++ = c;
				}
				break;
			default:
				*s++ = c;
				break;
		}
	}
	end:
	lb->len = s - lb->scratch;
	return PDF_TOK_STRING;
}

static int
lex_hex_string(hd_context *ctx, hd_stream *f, pdf_lexbuf *lb)
{
	char *s = lb->scratch;
	char *e = s + lb->size;
	int a = 0, x = 0;
	int c;

	while (1)
	{
		if (s == e)
		{
			s += pdf_lexbuf_grow(ctx, lb);
			e = lb->scratch + lb->size;
		}
		c = hd_read_byte(ctx, f);
		switch (c)
		{
			case IS_WHITE:
				break;
			case IS_HEX:
				if (x)
				{
					*s++ = a * 16 + unhex(c);
					x = !x;
				}
				else
				{
					a = unhex(c);
					x = !x;
				}
				break;
			case '>':
			case EOF:
				goto end;
			default:
				hd_warn(ctx, "ignoring invalid character in hex string");
		}
	}
	end:
	lb->len = s - lb->scratch;
	return PDF_TOK_STRING;
}

static pdf_token
pdf_token_from_keyword(char *key)
{
	switch (*key)
	{
		case 'R':
			if (!strcmp(key, "R")) return PDF_TOK_R;
			break;
		case 't':
			if (!strcmp(key, "true")) return PDF_TOK_TRUE;
			if (!strcmp(key, "trailer")) return PDF_TOK_TRAILER;
			break;
		case 'f':
			if (!strcmp(key, "false")) return PDF_TOK_FALSE;
			break;
		case 'n':
			if (!strcmp(key, "null")) return PDF_TOK_NULL;
			break;
		case 'o':
			if (!strcmp(key, "obj")) return PDF_TOK_OBJ;
			break;
		case 'e':
			if (!strcmp(key, "endobj")) return PDF_TOK_ENDOBJ;
			if (!strcmp(key, "endstream")) return PDF_TOK_ENDSTREAM;
			break;
		case 's':
			if (!strcmp(key, "stream")) return PDF_TOK_STREAM;
			if (!strcmp(key, "startxref")) return PDF_TOK_STARTXREF;
			break;
		case 'x':
			if (!strcmp(key, "xref")) return PDF_TOK_XREF;
			break;
	}

	while (*key)
	{
		if (!hd_isprint(*key))
			return PDF_TOK_ERROR;
		++key;
	}

	return PDF_TOK_KEYWORD;
}

pdf_token
pdf_lex(hd_context *ctx, hd_stream *f, pdf_lexbuf *buf)
{
	while (1)
	{
		int c = hd_read_byte(ctx, f);
		switch (c)
		{
			case EOF:
				return PDF_TOK_EOF;
			case IS_WHITE:
				lex_white(ctx, f);
				break;
			case '%':
				lex_comment(ctx, f);
				break;
			case '/':
				lex_name(ctx, f, buf);
				return PDF_TOK_NAME;
			case '(':
				return lex_string(ctx, f, buf);
			case ')':
				hd_warn(ctx, "lexical error (unexpected ')')");
				continue;
			case '<':
				c = hd_read_byte(ctx, f);
				if (c == '<')
				{
					return PDF_TOK_OPEN_DICT;
				}
				else
				{
					hd_unread_byte(ctx, f);
					return lex_hex_string(ctx, f, buf);
				}
			case '>':
				c = hd_read_byte(ctx, f);
				if (c == '>')
				{
					return PDF_TOK_CLOSE_DICT;
				}
				hd_warn(ctx, "lexical error (unexpected '>')");
				if (c == EOF)
				{
					return PDF_TOK_EOF;
				}
				hd_unread_byte(ctx, f);
				continue;
			case '[':
				return PDF_TOK_OPEN_ARRAY;
			case ']':
				return PDF_TOK_CLOSE_ARRAY;
			case '{':
				return PDF_TOK_OPEN_BRACE;
			case '}':
				return PDF_TOK_CLOSE_BRACE;
			case IS_NUMBER:
				return lex_number(ctx, f, buf, c);
			default: /* isregular: !isdelim && !iswhite && c != EOF */
				hd_unread_byte(ctx, f);
				lex_name(ctx, f, buf);
				return pdf_token_from_keyword(buf->scratch);
		}
	}
}

pdf_token
pdf_lex_no_string(hd_context *ctx, hd_stream *f, pdf_lexbuf *buf)
{
	while (1)
	{
		int c = hd_read_byte(ctx, f);
		switch (c)
		{
			case EOF:
				return PDF_TOK_EOF;
			case IS_WHITE:
				lex_white(ctx, f);
				break;
			case '%':
				lex_comment(ctx, f);
				break;
			case '/':
				lex_name(ctx, f, buf);
				return PDF_TOK_NAME;
			case '(':
				continue;
			case ')':
				continue;
			case '<':
				c = hd_read_byte(ctx, f);
				if (c == '<')
				{
					return PDF_TOK_OPEN_DICT;
				}
				else
				{
					continue;
				}
			case '>':
				c = hd_read_byte(ctx, f);
				if (c == '>')
				{
					return PDF_TOK_CLOSE_DICT;
				}
				if (c == EOF)
				{
					return PDF_TOK_EOF;
				}
				hd_unread_byte(ctx, f);
				continue;
			case '[':
				return PDF_TOK_OPEN_ARRAY;
			case ']':
				return PDF_TOK_CLOSE_ARRAY;
			case '{':
				return PDF_TOK_OPEN_BRACE;
			case '}':
				return PDF_TOK_CLOSE_BRACE;
			case IS_NUMBER:
				return lex_number(ctx, f, buf, c);
			default: /* isregular: !isdelim && !iswhite && c != EOF */
				hd_unread_byte(ctx, f);
				lex_name(ctx, f, buf);
				return pdf_token_from_keyword(buf->scratch);
		}
	}
}
