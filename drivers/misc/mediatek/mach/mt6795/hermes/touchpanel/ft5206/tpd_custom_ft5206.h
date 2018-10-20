#ifndef TOUCHPANEL_H__
#define TOUCHPANEL_H__

#include <linux/hrtimer.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#include <cust_eint.h>
#include <linux/jiffies.h>
#include <pmic_drv.h>
#include <linux/earlysuspend.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0))
 #if defined(MODULE) || defined(CONFIG_HOTPLUG)
  #define __devexit_p(x) x
 #else
  #define __devexit_p(x) NULL
 #endif
 #define __devinit   __section(.devinit.text) __cold notrace
 #define __devinitdata    __section(.devinit.data)
 #define __devinitconst   __section(.devinit.rodata)
 #define __devexit   __section(.devexit.text) __exitused __cold notrace
 #define __devexitdata    __section(.devexit.data)
 #define __devexitconst   __section(.devexit.rodata)
#endif

struct Upgrade_Info 
{
    u8 CHIP_ID;
    u8 FTS_NAME[20];
    u8 TPD_MAX_POINTS;
    u8 AUTO_CLB;
    u16 delay_aa;
    u16 delay_55;
    u8 upgrade_id_1;
    u8 upgrade_id_2;
    u16 delay_readid;
    u16 delay_earse_flash;
};

extern struct Upgrade_Info fts_updateinfo_curr;
extern int fts_i2c_Read(struct i2c_client *client, char *writebuf,int writelen, char *readbuf, int readlen);
extern int fts_i2c_Write(struct i2c_client *client, char *writebuf, int writelen);
extern int fts_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue);
extern int fts_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue);
extern void fts_get_upgrade_array(void);
extern void fts_reset_tp(int HighOrLow);

#define TPD_POWER_SOURCE_CUSTOM 10
#define TPD_HAVE_BUTTON
#define TPD_KEY_COUNT 3
#define TPD_KEYS {KEY_MENU, KEY_HOMEPAGE, KEY_BACK}
#define TPD_KEYS_DIM {{160, 2100, 20, 40}, {540, 2100, 20, 40}, {920, 2100, 20, 40}}
#define TPD_I2C_NUMBER 2
#define VELOCITY_CUSTOM
#define TPD_VELOCITY_CUSTOM_X 15
#define TPD_VELOCITY_CUSTOM_Y 20
#define TPD_CALIBRATION_MATRIX {962, 0, 0, 0, 1600, 0, 0, 0}
#define AUTO_CLB_NEED 1
#define DBG(fmt, args...) do{}while(0)
#endif
