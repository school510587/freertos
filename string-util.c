#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "fio.h"

typedef enum {
	IOFMT_CHAR,
	IOFMT_ERROR,
	IOFMT_INT,
	IOFMT_PTR,
	IOFMT_STR,
	IOFMT_TEXT,
	IOFMT_UINT,
	IOFMT_XINT,
	IOFMT_xINT,
	IOFMT_UNKNOWN
} IOFMT_TOKEN;

typedef struct {
	const char *fw_str;
	IOFMT_TOKEN type;
	char spec;
	char width;
} output_token;

typedef int (*putc_t)(void *param, char c);
typedef int (*puts_t)(void *param, const char *s);

#define _CTYPE_DATA_0_127 \
	_C,     _C,     _C,     _C,     _C,     _C,     _C,     _C, \
	_C,     _C|_S, _C|_S, _C|_S,    _C|_S,  _C|_S,  _C,     _C, \
	_C,     _C,     _C,     _C,     _C,     _C,     _C,     _C, \
	_C,     _C,     _C,     _C,     _C,     _C,     _C,     _C, \
	_S|_B,  _P,     _P,     _P,     _P,     _P,     _P,     _P, \
	_P,     _P,     _P,     _P,     _P,     _P,     _P,     _P, \
	_N,     _N,     _N,     _N,     _N,     _N,     _N,     _N, \
	_N,     _N,     _P,     _P,     _P,     _P,     _P,     _P, \
	_P,     _U|_X,  _U|_X,  _U|_X,  _U|_X,  _U|_X,  _U|_X,  _U, \
	_U,     _U,     _U,     _U,     _U,     _U,     _U,     _U, \
	_U,     _U,     _U,     _U,     _U,     _U,     _U,     _U, \
	_U,     _U,     _U,     _P,     _P,     _P,     _P,     _P, \
	_P,     _L|_X,  _L|_X,  _L|_X,  _L|_X,  _L|_X,  _L|_X,  _L, \
	_L,     _L,     _L,     _L,     _L,     _L,     _L,     _L, \
	_L,     _L,     _L,     _L,     _L,     _L,     _L,     _L, \
	_L,     _L,     _L,     _P,     _P,     _P,     _P,     _C

#define _CTYPE_DATA_128_255 \
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

static int error_n = 0;

static const char ctype_table[128 + 256] = {
	_CTYPE_DATA_128_255,
	_CTYPE_DATA_0_127,
	_CTYPE_DATA_128_255
};
const char *__ctype_ptr__ = ctype_table + 127;

static int _putc_printf(void *param, char c)
{
	int r = fio_write(1, &c, 1);

	if (r > 0)
		(*(int*)param)++;
	return r;
}

static int _putc_sprintf(void *param, char c)
{
	*(*((char**)param))++ = c;
	return 0;
}

static int _puts_printf(void *param, const char *s)
{
	int r = puts(s);

	if (r > 0)
		*(int*)param += strlen(s);
	return r;
}

static int _puts_sprintf(void *param, const char *s)
{
	char **p = (char**)param;

	strcpy(*p, s);
	*p += strlen(s);
	return 0;
}

static output_token get_next_output_token(const char *fmt)
{
	output_token ret = {.fw_str = fmt, .type = IOFMT_UNKNOWN, .spec = '\0', .width = 0};

	if (*ret.fw_str == '%') {
		char width[8] = {0};
		char *p = width;

		ret.fw_str++;
		if (*ret.fw_str == '-' || *ret.fw_str == '0')
			ret.spec = *ret.fw_str++;
		for (; *ret.fw_str && ret.type == IOFMT_UNKNOWN; ret.fw_str++) {
			switch (*ret.fw_str) {
				case '%':
					if (ret.spec || ret.width > 0) {
						ret.type = IOFMT_ERROR;
						ret.fw_str--;
					}
					else {
						ret.spec = '%';
						ret.type = IOFMT_TEXT;
					}
				break;
				case 'c':
					ret.type = IOFMT_CHAR;
				break;
				case 'd':
				case 'i':
					ret.type = IOFMT_INT;
				break;
				case 'p':
					ret.type = IOFMT_PTR;
				break;
				case 's':
					ret.type = IOFMT_STR;
				break;
				case 'u':
					ret.type = IOFMT_UINT;
				break;
				case 'X':
					ret.type = IOFMT_XINT;
				break;
				case 'x':
					ret.type = IOFMT_xINT;
				break;
				default:
					ret.width = 10 * ret.width + (*ret.fw_str - '0');
				break;
			}
		}
	}
	else {
		ret.type = IOFMT_TEXT;
		ret.spec = *ret.fw_str++;
	}

	return ret;
}

static char *utoa(unsigned int num, char *dst, unsigned int base, int lowercase)
{
	char buf[33] = {0};
	char *p = &buf[32];

	if (num == 0)
		*--p = '0';
	else {
		char *q;

		for (; num; num/=base)
			*--p = "0123456789ABCDEF" [num % base];
		if (lowercase)
			for (q = p; *q; q++)
				*q = (char)tolower(*q);
	}

	return strcpy(dst, p);
}

static int vprintf_core(const char *fmt, va_list arg_list, putc_t _putc_, puts_t _puts_, void *param)
{
	char buf[12];
	output_token out;
	union {
		int i;
		const char *s;
		unsigned u;
	} argv;

	for (; *fmt; fmt = out.fw_str) {
		int ret = 0;

		out = get_next_output_token(fmt);
		switch (out.type) {
			case IOFMT_CHAR:
				argv.i = va_arg(arg_list, int);
				_putc_(param, (char)argv.i);
				argv.s = NULL;
			break;
			case IOFMT_INT:
				argv.i = va_arg(arg_list, int);
				if (argv.i < 0) {
					buf[0] = '-';
					utoa(-argv.i, buf + 1, 10, 0);
				}
				else
					utoa(argv.i, buf, 10, 0);
				argv.s = buf;
			break;
			case IOFMT_PTR:
				argv.u = va_arg(arg_list, unsigned);
				if (argv.u) {
					strcpy(buf, "0x");
					utoa(argv.u, buf + 2, 16, 1);
					argv.s = buf;
				}
				else
					argv.s = "(nil)";
			break;
			case IOFMT_STR:
				argv.s = va_arg(arg_list, const char *);
				if (!argv.s)
					argv.s = "(null)";
			break;
			case IOFMT_TEXT:
				_putc_(param, out.spec);
				argv.s = NULL;
			break;
			case IOFMT_UINT:
				argv.u = va_arg(arg_list, unsigned);
				utoa(argv.u, buf, 10, 0);
				argv.s = buf;
			break;
			case IOFMT_XINT:
				argv.u = va_arg(arg_list, unsigned);
				utoa(argv.u, buf, 16, 0);
				argv.s = buf;
			break;
			case IOFMT_xINT:
				argv.u = va_arg(arg_list, unsigned);
				utoa(argv.u, buf, 16, 1);
				argv.s = buf;
			break;
			default:
				argv.s = NULL;
			break;
		}
		if (argv.s) {
			int w = out.width - strlen(argv.s);
			if (out.spec == '-') {
				_puts_(param, argv.s);
				while (w-- > 0) {
					_putc_(param, ' ');
				}
			}
			else {
				if (!out.spec)
					out.spec = ' ';
				while (w-- > 0) {
					_putc_(param, out.spec);
				}
				_puts_(param, argv.s);
			}
		}
	}

	return 0;
}

int *__errno()
{
	return &error_n;
}

int printf(const char *fmt, ...)
{
	int count = 0;
	va_list arg_list;

	va_start(arg_list, fmt);
	vprintf_core(fmt, arg_list, _putc_printf, _puts_printf, &count);
	va_end(arg_list);

	return count;
}

int puts(const char *s)
{
	fio_write(1, s, strlen(s));
	return 1;
}

int sprintf(char *dst, const char *fmt, ...)
{
	char *p = dst;
	va_list arg_list;

	va_start(arg_list, fmt);
	vprintf_core(fmt, arg_list, _putc_sprintf, _puts_sprintf, &p);
	va_end(arg_list);
	*p = '\0';

	return p - dst;
}

char *strcat(char *dst, const char *src)
{
	char *ret = dst;

	for (; *dst; ++dst);
	while ((*dst++ = *src++));

	return ret;
}

char *strchr(const char *s, int c)
{
	for (; *s && *s != c; s++);
	return (*s == c) ? (char *)s : NULL;
}

int strcmp(const char *a, const char *b) __attribute__ ((naked));
int strcmp(const char *a, const char *b)
{
	asm(
	"strcmp_lop:                \n"
	"   ldrb    r2, [r0],#1     \n"
	"   ldrb    r3, [r1],#1     \n"
	"   cmp     r2, #1          \n"
	"   it      hi              \n"
	"   cmphi   r2, r3          \n"
	"   beq     strcmp_lop      \n"
		"       sub     r0, r2, r3      \n"
	"   bx      lr              \n"
		:::
	);
}

char *strcpy(char *dest, const char *src)
{
	const char *s = src;
	char *d = dest;
	while ((*d++ = *s++));
	return dest;
}

size_t strlen(const char *s) __attribute__ ((naked));
size_t strlen(const char *s)
{
	asm(
		"       sub  r3, r0, #1                 \n"
	"strlen_loop:               \n"
		"       ldrb r2, [r3, #1]!              \n"
		"       cmp  r2, #0                             \n"
	"   bne  strlen_loop        \n"
		"       sub  r0, r3, r0                 \n"
		"       bx   lr                                 \n"
		:::
	);
}

int strncmp(const char *a, const char *b, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		if (a[i] != b[i])
			return a[i] - b[i];

	return 0;
}

char *strncpy(char *dest, const char *src, size_t n)
{
	const char *s = src;
	char *d = dest;
	while (n-- && (*d++ = *s++));
	return dest;
}
