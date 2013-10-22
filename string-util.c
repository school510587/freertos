#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include "fio.h"

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

#define PRINTF_BODY(fmt) \
	char buf[12]; \
	union { \
		int i; \
		const char *s; \
		unsigned u; \
	} argv; \
	va_list arg_list; \
	\
	va_start(arg_list, fmt); \
	for (; *fmt; ++fmt) { \
		if (*fmt == '%') { \
			switch (*++fmt) { \
				case '%': \
					_PUTC_('%') \
				break; \
				case 'c': \
					argv.i = va_arg(arg_list, int); \
					_PUTC_(argv.i) \
				break; \
				case 'd': \
				case 'i': \
					argv.i = va_arg(arg_list, int); \
					itoa(argv.i, buf, 10); \
					_PUTS_(buf) \
				break; \
				case 'u': \
					argv.u = va_arg(arg_list, unsigned); \
					utoa(argv.u, buf, 10); \
					_PUTS_(buf) \
				break; \
				case 's': \
					argv.s = va_arg(arg_list, const char *); \
					if (argv.s) { \
						_PUTS_(argv.s) \
					} \
					else { \
						_PUTS_("(null)"); \
					} \
				break; \
				case 'X': \
					argv.u = va_arg(arg_list, unsigned); \
					utoa(argv.u, buf, 16); \
					_PUTS_(buf) \
				break; \
			} \
		} \
		else { \
			_PUTC_(*fmt); \
		} \
	} \
	va_end(arg_list);

static int error_n = 0;

static const char ctype_table[128 + 256] = {
	_CTYPE_DATA_128_255,
	_CTYPE_DATA_0_127,
	_CTYPE_DATA_128_255
};
const char *__ctype_ptr__ = ctype_table + 127;

static char *utoa(unsigned int num, char *dst, unsigned int base)
{
	char buf[33] = {0};
	char *p = &buf[32];

	if (num == 0)
		*--p = '0';
	else
		for (; num; num/=base)
			*--p = "0123456789ABCDEF" [num % base];

	return strcpy(dst, p);
}

int *__errno()
{
	return &error_n;
}

char *itoa(int num, char *dst, int base)
{
	if (base == 10 && num < 0) {
		utoa(-num, dst+1, base);
		*dst = '-';
	}
	else
		utoa(num, dst, base);

	return dst;
}

int printf(const char *fmt, ...)
{
	int count = 0;
	char b;

#define _PUTC_(c) \
	b = (char)(c); \
	fio_write(1, &b, 1); \
	++count;
#define _PUTS_(s) \
	puts(s); \
	count += strlen(s);

	PRINTF_BODY(fmt)

#undef _PUTC_
#undef _PUTS_

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

#define _PUTC_(c) \
	*p++ = (char)(c);
#define _PUTS_(s) \
	strcpy(p, s); \
	p += strlen(s);

	PRINTF_BODY(fmt)

#undef _PUTC_
#undef _PUTS_

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
