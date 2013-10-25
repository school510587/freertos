#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

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
    unsigned int prng_state;
} mmtest_slot;

static mmtest_slot read_cb(const mmtest_slot *slots, unsigned int *read_pointer, unsigned int write_pointer)
{
    const mmtest_slot *pfoo;
    if (write_pointer == *read_pointer) {
        // circular buffer is empty
        return (mmtest_slot){ .pointer=NULL, .size=0, .prng_state=0 };
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

    srand(0xACE1);
    for (;;) {
        do {
            i = rand();
            size = i & ALLOC_SIZE_MASK;
        } while (size < MIN_ALLOC_SIZE);
        printf("try to allocate %d bytes\n", size);
        p = (char*)malloc(size);
        printf("malloc returned %p\n", p);
        if (p == NULL) {
            // can't do new allocations until we free some old ones
            while (circbuf_size(read_pointer, write_pointer) > 0) {
                // confirm that data didn't get trampled before freeing
                mmtest_slot foo = read_cb(slots, &read_pointer, write_pointer);
                p = foo.pointer;
                // reset the PRNG to its state for random data
                srand(foo.prng_state);
                size = foo.size;
                printf("free a block, size %d\n", size);
                for (i = 0; i < size; i++) {
                    unsigned char u = p[i];
                    unsigned char v = (unsigned char) rand();
                    if (u != v) {
                        printf("OUCH: u=%02x, v=%02x\n", u, v);
                        return;
                    }
                }
                free(p);
                if ((rand() & 1) == 0)
                    break;
            }
            record[RECORD_INDEX_OF(size)][1]++;
        }
        else {
            printf("allocate a block, size %d\n", size);
            if (circbuf_size(read_pointer, write_pointer) == CIRCBUFSIZE - 1) {
                fio_write(2, "circular buffer overflow\n", 25);
                return;
            }
            slots[write_pointer++] = (mmtest_slot){.pointer=p, .size=size, .prng_state=i};
            write_pointer %= CIRCBUFSIZE;
            srand(i);
            for (i = 0; i < size; i++) {
                p[i] = (unsigned char) rand();
            }
            record[RECORD_INDEX_OF(size)][0]++;
        }
        c = recv_byte(&i);
        if (i == pdTRUE) {
            if (tolower(c) == 'x')
                return;
            puts("  Range   Success Failure\n");
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
