#include <stdlib.h>

// Default initial state
static unsigned int state = 1;

// Get a pseudorandom number generator from Wikipedia
int rand() __attribute__((naked));
int rand()
{
    /* taps: 16 14 13 11; characteristic polynomial: x^16 + x^14 + x^13 + x^11 + 1 */
    __asm__ (
        "mov r0, %1\t\n" // r0 = state
        "eor r1, r0, r0, lsr #2 \t\n" // r1 = (g_state >> 0) ^ (g_state >> 2)
        "eor r1, r1, r0, lsr #3 \t\n" // r1 = r1 ^ (g_state >> 3)
        "eor r1, r1, r0, lsr #5 \t\n" // r1 = r1 ^ (g_state >> 5)
        "and r1, r1, #1         \t\n" // bit = r1 = r1 & 1
        "lsl r1, r1, #15        \t\n" // r1 = bit << 15
        "orr r0, r1, r0, lsr #1 \t\n" // state = r1 | (state >> 1)
        "mov %0, r0             \t\n" // output
        : "=r" (state)
        : "r" (state)
        : "r0", "r1"
    );
    __asm__ ("bx lr \t\n");
}

// Set the state of pseudorandom number generator
void srand(unsigned int seed)
{
    state = seed & 0xFFFF;
}
