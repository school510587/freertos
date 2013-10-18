#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <limits.h>
#include "fio.h"

#define ALIGN (sizeof(size_t))
#define ONES ((size_t)-1/UCHAR_MAX)
#define HIGHS (ONES * (UCHAR_MAX/2+1))
#define HASZERO(x) ((x)-ONES & ~(x) & HIGHS)

#define SS (sizeof(size_t))

#define PRINTF_BODY(fmt) \
	char buf[8]; \
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
					_PUTS_(argv.s) \
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

int isalnum(int c)
{
	return (('a' <= (c) && (c) <= 'z') ||
		('A' <= (c) && (c) <= 'Z') ||
		('0' <= (c) && (c) <= '9'));
}

int isspace(int c)
{
	return (c == ' ');
}

void *memset(void *dest, int c, size_t n)
{
	unsigned char *s = dest;
	c = (unsigned char)c;
	for (; ((uintptr_t)s & ALIGN) && n; n--) *s++ = c;
	if (n) {
		size_t *w, k = ONES * c;
		for (w = (void *)s; n>=SS; n-=SS, w++) *w = k;
		for (s = (void *)w; n; n--, s++) *s = c;
	}
	return dest;
}

void *memcpy(void *dest, const void *src, size_t n)
{
	void *ret = dest;

	//Cut rear
	uint8_t *dst8 = dest;
	const uint8_t *src8 = src;
	switch (n % 4) {
		case 3 : *dst8++ = *src8++;
		case 2 : *dst8++ = *src8++;
		case 1 : *dst8++ = *src8++;
		case 0 : ;
	}

	//stm32 data bus width
	uint32_t *dst32 = (void *)dst8;
	const uint32_t *src32 = (void *)src8;
	n = n / 4;
	while (n--) {
		*dst32++ = *src32++;
	}

	return ret;
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
