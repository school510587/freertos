#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "osdebug.h"

#define ALLOC_SIZE_MASK 0x7FF 
#define MIN_ALLOC_SIZE 256
#define CIRCBUFSIZE (configTOTAL_HEAP_SIZE / MIN_ALLOC_SIZE)
#define circbuf_size(r_ptr, w_ptr) (((w_ptr) + CIRCBUFSIZE - (r_ptr)) % CIRCBUFSIZE)

typedef struct {
    void *pointer;
    unsigned int size;
    unsigned int lfsr;
} mmtest_slot;

static unsigned int lfsr = 0xACE1;

// Get a pseudorandom number generator from Wikipedia
static int prng(void)
{
    static unsigned int bit;
    /* taps: 16 14 13 11; characteristic polynomial: x^16 + x^14 + x^13 + x^11 + 1 */
    bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
    lfsr =  (lfsr >> 1) | (bit << 15);
    return lfsr & 0xffff;
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
    int test_count = 0;
    int i;
    int size;
    char *p;

    for (; test_count < 200; test_count++) {
        do {
            size = prng() & ALLOC_SIZE_MASK;
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
        } else {
            printf("allocate a block, size %d\n", size);
            if (circbuf_size(read_pointer, write_pointer) == CIRCBUFSIZE - 1) {
                fio_write(2, "circular buffer overflow\n", 25);
                return;
            }
            slots[write_pointer++] = (mmtest_slot){.pointer=p, .size=size, .lfsr=lfsr};
            write_pointer %= CIRCBUFSIZE;
            for (i = 0; i < size; i++) {
                p[i] = (unsigned char) prng();
            }
        }
    }

    while (circbuf_size(read_pointer, write_pointer) > 0) {
        mmtest_slot foo = read_cb(slots, &read_pointer, write_pointer);

        free(foo.pointer);
    }
}

void osDbgPrintf(const char * fmt, ...) { }
