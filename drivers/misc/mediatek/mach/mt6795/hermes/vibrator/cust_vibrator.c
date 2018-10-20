#include <cust_vibrator.h>
#include <linux/types.h>

static struct vibrator_hw cust_vibrator_hw = {
    .vib_timer = 25,
    .vib_limit = 9,
    .vib_vol = 0x5, /* 2.8V */
};

struct vibrator_hw *get_cust_vibrator_hw(void)
{
    return &cust_vibrator_hw;
}
