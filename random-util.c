#include <stdlib.h>

// PRNG state is the linear feedback shift register (lfsr)
static unsigned int state = 0xACE1;

// Get a pseudorandom number generator from Wikipedia
int rand()
{
    /* taps: 16 14 13 11; characteristic polynomial: x^16 + x^14 + x^13 + x^11 + 1 */
    unsigned int bit = ((state >> 0) ^ (state >> 2) ^ (state >> 3) ^ (state >> 5) ) & 1;
    state = (state >> 1) | (bit << 15);
    return (int)state;
}

// Set the state of pseudorandom number generator
void srand(unsigned int seed)
{
    state = seed & 0xFFFF;
}
