#include "volume.h"
#include <string.h>

/* Q31 gains for steps 1..30: 10^(-2*(31-step)/20) * 2^31 */
static const int32_t gain_q31[30] = {
       2147484, /* step  1:  -60.0 dB */
       2703522, /* step  2:  -58.0 dB */
       3403532, /* step  3:  -56.0 dB */
       4284793, /* step  4:  -54.0 dB */
       5394235, /* step  5:  -52.0 dB */
       6790940, /* step  6:  -50.0 dB */
       8549286, /* step  7:  -48.0 dB */
      10762914, /* step  8:  -46.0 dB */
      13549706, /* step  9:  -44.0 dB */
      17058069, /* step 10:  -42.0 dB */
      21474836, /* step 11:  -40.0 dB */
      27035217, /* step 12:  -38.0 dB */
      34035322, /* step 13:  -36.0 dB */
      42847932, /* step 14:  -34.0 dB */
      53942350, /* step 15:  -32.0 dB */
      67909396, /* step 16:  -30.0 dB */
      85492864, /* step 17:  -28.0 dB */
     107629139, /* step 18:  -26.0 dB */
     135497058, /* step 19:  -24.0 dB */
     170580690, /* step 20:  -22.0 dB */
     214748365, /* step 21:  -20.0 dB */
     270352174, /* step 22:  -18.0 dB */
     340353221, /* step 23:  -16.0 dB */
     428479319, /* step 24:  -14.0 dB */
     539423504, /* step 25:  -12.0 dB */
     679093957, /* step 26:  -10.0 dB */
     854928639, /* step 27:   -8.0 dB */
    1076291389, /* step 28:   -6.0 dB */
    1354970580, /* step 29:   -4.0 dB */
    1705806895, /* step 30:   -2.0 dB */
};

void volume_apply(int32_t *samples, size_t n_samples, int step)
{
    if (step >= VOLUME_STEPS - 1) return;                /* bit-perfect bypass */
    if (step <= 0) {
        memset(samples, 0, n_samples * sizeof(int32_t)); /* mute */
        return;
    }

    const int64_t g = gain_q31[step - 1];
    for (size_t i = 0; i < n_samples; i++) {
        int64_t v = ((int64_t)samples[i] * g + (1LL << 30)) >> 31;
        if (v >  INT32_MAX) v =  INT32_MAX;
        if (v < INT32_MIN)  v =  INT32_MIN;
        samples[i] = (int32_t)v;
    }
}
