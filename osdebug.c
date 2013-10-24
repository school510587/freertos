#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "osdebug.h"
#include "serial_io.h"

#define ALLOC_SIZE_MASK 0x7FF 
#define MIN_ALLOC_SIZE 256
#define RANGE_PER_RECORD 256
#define RECORD_INDEX_OF(size) (((size) - MIN_ALLOC_SIZE) / RANGE_PER_RECORD)
#define CIRCBUFSIZE (configTOTAL_HEAP_SIZE / MIN_ALLOC_SIZE)
#define circbuf_size(r_ptr, w_ptr) (((w_ptr) + CIRCBUFSIZE - (r_ptr)) % CIRCBUFSIZE)

typedef struct {
    void *pointer;
    unsigned int size;
    uint16_t lfsr;
} mmtest_slot;

static uint16_t lfsr = 0xACE1;

// Get a pseudorandom number generator from Wikipedia
static int prng(void) __attribute__((naked));
static int prng(void)
{
    /* taps: 16 14 13 11; characteristic polynomial: x^16 + x^14 + x^13 + x^11 + 1 */
    __asm__ (
        "mov r0, %1\t\n" // r0 = lfsr
        "eor r1, r0, r0, lsr #2 \t\n" // r1 = (g_lfsr >> 0) ^ (g_lfsr >> 2)
        "eor r1, r1, r0, lsr #3 \t\n" // r1 = r1 ^ (g_lfsr >> 3)
        "eor r1, r1, r0, lsr #5 \t\n" // r1 = r1 ^ (g_lfsr >> 5)
        "and r1, r1, #1         \t\n" // bit = r1 = r1 & 1
        "lsl r1, r1, #15        \t\n" // r1 = bit << 15
        "orr r0, r1, r0, lsr #1 \t\n" // lfsr = r1 | (lfsr >> 1)
        "mov %0, r0             \t\n"
        : "=r" (lfsr)
        : "r" (lfsr)
        : "r0", "r1"
    );
    __asm__ ("bx lr \t\n");
}

static mmtest_slot read_cb(const mmtest_slot *slots, unsigned int *read_pointer, unsigned int write_pointer)
{
    const mmtest_slot *pfoo;
    if (write_pointer == *read_pointer) {
        // circular buffer is empty
        return (mmtest_slot){ .pointer=NULL, .size=0, .lfsr=0 };
    }
    pfoo = &slots[(*read_pointer)++];
    *read_pointer %= CIRCBUFSIZE;
    return *pfoo;
}

void mmtest_task(void * pvParameters)
{
    mmtest_slot slots[CIRCBUFSIZE];
    unsigned int write_pointer = 0;
    unsigned int read_pointer = 0;
    int record[RECORD_INDEX_OF(ALLOC_SIZE_MASK) + 1][2] = {0};
    int i;
    int size;
    char *p;
    char c;

    for (;;) {
        do {
            i = prng();
            size = i & ALLOC_SIZE_MASK;
        } while (size < MIN_ALLOC_SIZE);
        printf("try to allocate %d bytes\n", size);
        p = (char*)malloc(size);
        printf("malloc returned %p\n", p);
        if (p == NULL) {
            // can't do new allocations until we free some older ones
            while (circbuf_size(read_pointer, write_pointer) > 0) {
                // confirm that data didn't get trampled before freeing
                mmtest_slot foo = read_cb(slots, &read_pointer, write_pointer);
                p = foo.pointer;
                lfsr = foo.lfsr;  // reset the PRNG to its earlier state
                size = foo.size;
                printf("free a block, size %d\n", size);
                for (i = 0; i < size; i++) {
                    unsigned char u = p[i];
                    unsigned char v = (unsigned char) prng();
                    if (u != v) {
                        printf("OUCH: u=%02X, v=%02X\n", u, v);
                        return;
                    }
                }
                free(p);
                if ((prng() & 1) == 0) break;
            }
            record[RECORD_INDEX_OF(size)][1]++;
        } else {
            printf("allocate a block, size %d\n", size);
            if (circbuf_size(read_pointer, write_pointer) == CIRCBUFSIZE - 1) {
                fio_write(2, "circular buffer overflow\n", 25);
                return;
            }
            slots[write_pointer++] = (mmtest_slot){.pointer=p, .size=size, .lfsr=i};
            write_pointer %= CIRCBUFSIZE;
            for (i = 0; i < size; i++) {
                p[i] = (unsigned char) prng();
            }
            record[RECORD_INDEX_OF(size)][0]++;
        }
        c = recv_byte(&i);
        if (i == pdTRUE) {
            if (tolower(c) == 'x')
                return;
            for (i = 0; i < sizeof(record) / sizeof(record[0]); i++) {
                int j = MIN_ALLOC_SIZE + i * RANGE_PER_RECORD;
                printf("%4d~%4d %7d %7d\n", j, j + RANGE_PER_RECORD - 1, record[i][0], record[i][1]);
            }
            puts("(x: exit; other key to continue)");
            c = recv_byte(NULL);
            puts("\n");
            if (tolower(c) == 'x')
                return;
        }
    }

    while (circbuf_size(read_pointer, write_pointer) > 0) {
        mmtest_slot foo = read_cb(slots, &read_pointer, write_pointer);

        free(foo.pointer);
    }
}

void osDbgPrintf(const char * fmt, ...) { }
