#include "theme.h"

/* Data font (times, format badges, battery %, wordmark) is under evaluation:
 * Scotch Mono vs Slab Mono vs Diatype itself. The sim's --datafont flag
 * switches it; once Micah picks, this collapses to a #define. */
const lv_font_t *pact_font_data = &diatype_regular_16;
