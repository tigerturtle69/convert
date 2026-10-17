/* Hollow Knight keeps its texture cache in the calculator's spare RAM, between two linker symbols. A native
 * build has no such RAM, so this provides the same two symbols over a static array. */
#define NP_HK_HEAP 262144
static char np_hk_heap[NP_HK_HEAP] __attribute__((aligned(8), used));
__asm__(".global _heap_start\n.set _heap_start, np_hk_heap\n.global _heap_end\n.set _heap_end, np_hk_heap + 262144\n");
