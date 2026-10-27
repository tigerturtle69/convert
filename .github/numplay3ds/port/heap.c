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

/* Games that use the launcher's gfx.c expect its bump allocator (launcher/src/platform.c, np_alloc): a small pool
 * here, weak so a game with its own keeps it. */
#include <stdint.h>
#ifndef NP_ALLOC_BYTES
#define NP_ALLOC_BYTES 262144
#endif
static uint8_t np_pool[NP_ALLOC_BYTES] __attribute__((aligned(8)));
static uint32_t np_pool_pos;
__attribute__((weak)) void *np_alloc(uint32_t size) {
  size = (size + 7u) & ~7u;
  if (np_pool_pos + size > sizeof np_pool) return 0;
  void *p = np_pool + np_pool_pos;
  np_pool_pos += size;
  return p;
}
__attribute__((weak)) void np_alloc_reset(void) { np_pool_pos = 0; }
