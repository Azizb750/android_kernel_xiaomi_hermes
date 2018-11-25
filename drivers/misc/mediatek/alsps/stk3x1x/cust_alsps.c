#include <linux/types.h>
#include <mach/mt_pm_ldo.h>
#include "cust_alsps.h"

static struct alsps_hw stk_cust_alsps_hw = {
    .i2c_num = 3,
    .polling_mode_ps = 0,
    .polling_mode_als = 1,
    .power_id = MT65XX_POWER_NONE,
    .power_vol = VOL_DEFAULT,
    .i2c_addr = {0x48, 0x00},
    .als_level = {1, 4, 10, 30, 80, 120, 225, 320, 550, 800, 1250, 2000, 3500, 6000, 10000, 20000, 50000},
    .als_value = {0, 5, 10, 30, 80, 120, 225, 320, 550, 800, 1250, 2000, 6000, 10000, 11000, 15000, 18000, 24000},
    .state_val = 0x0,
    .psctrl_val = 0x31,
    .alsctrl_val = 0x39,
    .ledctrl_val = 0xBF,
    .wait_val = 0x7,
    .ps_threshold_high = 1050,
    .ps_threshold_low = 980,
};
struct alsps_hw *stk_get_cust_alsps_hw(void) {
    return &stk_cust_alsps_hw;
}
