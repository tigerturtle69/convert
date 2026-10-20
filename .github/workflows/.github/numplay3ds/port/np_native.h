/* Force-included into every game of a native build (-include np_native.h). */
#include <stdint.h>
/* Set when running inside the launcher: on the calculator the launcher defines it (launcher/src/main.c), and
 * the games read it to leave Home and the RAM handling to the launcher. A native build is always the launcher. */
extern const uint8_t numplay_launcher[];
