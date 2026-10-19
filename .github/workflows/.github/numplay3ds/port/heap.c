/* Some of NumPlay's big games keep their working memory in the calculator's spare RAM, between two linker symbols
 * (_heap_start and _heap_end): Hollow Knight's texture cache, NumPowder's particles, Champion Island's sprite cache.
 * A native build has no such RAM, so this provides the same two symbols over a static array.
 * NP_HEAP_BYTES (set per game by tools/build_big.py) is how much the game gets. */
#ifndef NP_HEAP_BYTES
#define NP_HEAP_BYTES 262144
#endif
#define NP_STR2(x) #x
#define NP_STR(x) NP_STR2(x)
static char np_heap_mem[NP_HEAP_BYTES] __attribute__((aligned(8), used));
__asm__(".global _heap_start\n.set _heap_start, np_heap_mem\n"
        ".global _heap_end\n.set _heap_end, np_heap_mem + " NP_STR(NP_HEAP_BYTES) "\n");
