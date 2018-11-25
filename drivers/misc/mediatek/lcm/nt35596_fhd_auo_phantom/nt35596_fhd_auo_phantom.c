/*
 * Reversed by LazyC0DEr
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <mach/mt_pm_ldo.h>
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <cust_i2c.h>

#include "lcm_drv.h"
#include "tps65132_i2c.h"

static LCM_UTIL_FUNCS lcm_util;
static raw_spinlock_t auo_SpinLock;
static int lcm_intialized;

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define read_reg_v2(cmd, buffer, buffer_size)			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#define MDELAY(n)						(lcm_util.mdelay(n))
#define UDELAY(n)						(lcm_util.udelay(n))

#define REGFLAG_DELAY						0xFC
#define REGFLAG_END_OF_TABLE					0xFD

struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static struct LCM_setting_table lcm_backlight_level_setting[] = {
    {0xFF, 1, {0x00}},
    {0xFB, 1, {0x01}},
    {0x51, 1, {0xFF}},
    {REGFLAG_END_OF_TABLE, 0, {0x00}}
};

static struct LCM_setting_table lcm_backlight_disable[] = {
    {0x55, 1, {0x00}},
    {REGFLAG_END_OF_TABLE, 0, {0x00}}
};

static struct LCM_setting_table lcm_backlight_enable[] = {
    {0x55, 1, {0x01}},
    {REGFLAG_END_OF_TABLE, 0, {0x00}}
};

static struct LCM_setting_table lcm_suspend_setting[] = {
    {0x51, 1, {0x00}},
    {0x28, 0, {0x00}},
    {REGFLAG_DELAY, 20, {0x00}},
    {0x10, 0, {0x00}},
    {REGFLAG_DELAY, 120, {0x00}},
    {REGFLAG_END_OF_TABLE, 0, {0x00}}
};

static struct LCM_setting_table lcm_initialization_setting[] = {
    {0xFF, 1, {0xEE}},
    {0xFB, 1, {0x01}},
    {0x18, 1, {0x40}},
    {REGFLAG_DELAY, 10, {0x00}},
    {0x18, 1, {0x00}},
    {REGFLAG_DELAY, 20, {0x00}},
    {0x7C, 1, {0x31}},
    {0xFF, 1, {0x05}},
    {0xFB, 1, {0x01}},
    {0xE7, 1, {0x00}},
    {0xFF, 1, {0x04}},
    {0xFB, 1, {0x01}},
    {0x08, 1, {0x06}},
    {0xFF, 1, {0x00}},
    {0xFB, 1, {0x01}},
    {0x51, 1, {0x20}},
    {0x53, 1, {0x24}},
    {0x55, 1, {0x00}},
    {0xD3, 1, {0x06}},
    {0xD4, 1, {0x16}},
    {0x11, 0, {0x00}},
    {REGFLAG_DELAY, 120, {0x00}},
    {0x29, 0, {0x00}},
    {REGFLAG_DELAY, 20, {0x00}},
    {REGFLAG_END_OF_TABLE, 0, {0x00}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
    unsigned int i;
    for (i = 0; i < count; i++) {
        unsigned cmd;
        cmd = table[i].cmd;

        switch (cmd) {
            case REGFLAG_DELAY:
                if (table[i].count <= 10)
                    MDELAY(table[i].count);
                else
                    MDELAY(table[i].count);
                break;
            case REGFLAG_END_OF_TABLE:
                break;
            default:
                dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
        }
    }
}

static void tps65132_enable(bool enable){
    int i;
    mt_set_gpio_mode(GPIO_MHL_RST_B_PIN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_MHL_RST_B_PIN, GPIO_DIR_OUT);
    mt_set_gpio_mode(GPIO_MHL_EINT_PIN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_MHL_EINT_PIN, GPIO_DIR_OUT);
    if (enable){
        mt_set_gpio_out(GPIO_MHL_EINT_PIN, GPIO_OUT_ONE);
        MDELAY(12);
        mt_set_gpio_out(GPIO_MHL_RST_B_PIN, GPIO_OUT_ONE);
        MDELAY(12);
        for (i = 0; i < 3; i++) {
            if ((tps65132_write_bytes(0, 0xF) & 0x80000000) == 0)
                break;
            MDELAY(5);
        }
        for (i = 0; i < 3; i++) {
            if ((tps65132_write_bytes(1, 0xF) & 0x80000000) == 0)
                break;
            MDELAY(5);
        }
    } else {
        mt_set_gpio_out(GPIO_MHL_RST_B_PIN, GPIO_OUT_ZERO);
        MDELAY(12);
        mt_set_gpio_out(GPIO_MHL_EINT_PIN, GPIO_OUT_ZERO);
        MDELAY(12);
    }
}

static void auo_senddata(unsigned char value){
    int i;
    raw_spin_lock_irq(&auo_SpinLock);
    mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ONE);
    UDELAY(15);
    for (i = 7; i >= 0; i--) {
        if ((value >> i) & 1) {
            mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ZERO);
            UDELAY(10);
            mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ONE);
            UDELAY(30);
        } else {
            mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ZERO);
            UDELAY(30);
            mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ONE);
            UDELAY(10);
        }
    }
    mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ZERO);
    UDELAY(15);
    mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ONE);
    UDELAY(350);
    raw_spin_unlock_irq(&auo_SpinLock);
}

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
    memset(params, 0, sizeof(LCM_PARAMS));

    params->dsi.LANE_NUM = 4;
    params->dsi.vertical_backporch = 4;
    params->dsi.vertical_frontporch = 22;
    params->dsi.horizontal_sync_active = 4;
    params->physical_width = 68;
    params->physical_height = 121;
    params->dsi.mode = 3;
    params->dsi.PLL_CLOCK = 475;
    params->dsi.lcm_esd_check_table[0].cmd = 0xA;
    params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
    params->type = 2;
    params->dsi.data_format.format = 2;
    params->dsi.PS = 2;
    params->dsi.vertical_sync_active = 2;
    params->width = 1080;
    params->dsi.horizontal_active_pixel = 1080;
    params->height = 1920;
    params->dsi.vertical_active_line = 1920;
    params->dsi.switch_mode_enable = 0;
    params->dsi.data_format.color_order = 0;
    params->dsi.data_format.trans_seq = 0;
    params->dsi.data_format.padding = 0;
    params->dsi.noncont_clock = 1;
    params->dsi.esd_check_enable = 1;
    params->dsi.customization_esd_check_enable = 1;
    params->dsi.lcm_esd_check_table[0].count = 1;
    params->dsi.horizontal_backporch = 72;
    params->dsi.horizontal_frontporch = 72;
}

static void lcm_init(void)
{
    tps65132_enable(1);
    MDELAY(20);
    mt_set_gpio_mode(GPIO_LCM_RST, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCM_RST, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
    MDELAY(20);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
    MDELAY(20);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
    MDELAY(50);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
    MDELAY(50);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
    MDELAY(50);
    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
    MDELAY(20);
    lcm_intialized = 1;
}

static void lcm_suspend(void)
{
    push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
    mt_set_gpio_mode(GPIO_LCM_RST, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCM_RST, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
    MDELAY(10);
    tps65132_enable(0);
}

static void lcm_resume(void)
{
    tps65132_enable(1);
    MDELAY(15);
    mt_set_gpio_mode(GPIO_LCM_RST, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCM_RST, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
    MDELAY(3);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
    MDELAY(3);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
    MDELAY(3);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
    MDELAY(3);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
    MDELAY(22);
    if (lcm_intialized)
        //push_table(lcm_resume_setting, sizeof(lcm_resume_setting) / sizeof(struct LCM_setting_table), 1);
        push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
    else
        push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

static unsigned int lcm_compare_id(void)
{
    unsigned char LCD_ID_value;
    LCD_ID_value = which_lcd_module_triple();
    if (LCD_ID_value == 0) {
        unsigned int id;
        unsigned char buffer[2];
        unsigned int array[16];

        mt_set_gpio_mode(GPIO_LCM_RST, GPIO_MODE_00);
        mt_set_gpio_dir(GPIO_LCM_RST, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
        MDELAY(1);
        mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
        MDELAY(10);
        mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
        MDELAY(10);

        array[0] = 0x23700;
        dsi_set_cmdq(array, 1, 1);
        array[0] = 0xFF1500;
        dsi_set_cmdq(array, 1, 1);
        array[0] = 0x1FB1500;
        dsi_set_cmdq(array, 1, 1);
        MDELAY(10);
        read_reg_v2(0xF4, buffer, 1);
        MDELAY(20);
        id = buffer[0];
        return (id == 0x96)?1:0;
    } else
        return 0;
}

static void lcm_setbacklight_cmdq(void* handle, unsigned int level)
{
    static int auo_value_0 = 0, auo_value_1 = 0, auo_value_2 = 0;
    if (level != auo_value_0) {
        auo_value_0 = level;
        auo_value_1 = level;
        if (auo_value_1 != 0 || auo_value_2 != 0) {
            mt_set_gpio_mode(GPIO_MHL_POWER_CTRL_PIN, GPIO_MODE_00);
            mt_set_gpio_dir(GPIO_MHL_POWER_CTRL_PIN, GPIO_DIR_OUT);
            if (level) {
                if (level - 1 > 2) {
                    if (level > 255)
                        level = 255;
                } else
                    level = 3;

                if (level < 32)
                    auo_senddata(64 - level * 2);
                else {
                    if (auo_value_1 >= 32) {
                        if (auo_value_2 >= 32) {
                            mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ONE);
                            MDELAY(10);
                        } else
                            auo_senddata(0);
                    }
                }
            } else {
                mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ZERO);
                MDELAY(30);
            }

            if (level < 32)
                lcm_backlight_level_setting[2].para_list[0] = 32;
            else
                lcm_backlight_level_setting[2].para_list[0] = (unsigned char)level;
            push_table(lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);
            auo_value_2 = auo_value_1;
        }
    }
}

static void lcm_cabc_enable_cmdq(unsigned int mode)
{
    if (mode)
        push_table(lcm_backlight_enable, sizeof(lcm_backlight_enable) / sizeof(struct LCM_setting_table), 1);
    else
        push_table(lcm_backlight_disable, sizeof(lcm_backlight_disable) / sizeof(struct LCM_setting_table), 1);
}

LCM_DRIVER nt35596_fhd_auo_phantom_lcm_drv = {
    .name = "nt35596_fhd_auo_phantom",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params = lcm_get_params,
    .init = lcm_init,
    .suspend = lcm_suspend,
    .resume = lcm_resume,
    .compare_id = lcm_compare_id,
    .set_backlight_cmdq = lcm_setbacklight_cmdq,
    .set_pwm = lcm_cabc_enable_cmdq
};
