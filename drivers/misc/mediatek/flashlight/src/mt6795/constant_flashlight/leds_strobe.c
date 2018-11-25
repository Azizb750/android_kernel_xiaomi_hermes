#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/xlog.h>

#include <asm/uaccess.h>
#include <asm/io.h>

#include <cust_gpio_usage.h>

#include "kd_camera_hw.h"
#include "kd_flashlight.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
#include <linux/mutex.h>
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif
#endif

#define TAG_NAME "leds_strobe.c"
#define PK_DBG_FUNC(fmt, arg...)    xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME, KERN_INFO  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_TRC_VERBOSE(fmt, arg...) xlog_printk(ANDROID_LOG_VERBOSE, TAG_NAME,  fmt, ##arg)
#define PK_ERROR(fmt, arg...)       xlog_printk(ANDROID_LOG_ERROR  , TAG_NAME, KERN_ERR "%s: " fmt, __FUNCTION__ ,##arg)

//#define DEBUG_LEDS_STROBE
#ifdef  DEBUG_LEDS_STROBE
    #define PK_DBG PK_DBG_FUNC
    #define PK_VER PK_TRC_VERBOSE
    #define PK_ERR PK_ERROR
#else
    #define PK_DBG(a,...)
    #define PK_VER(a,...)
    #define PK_ERR(a,...)
#endif

static DEFINE_SPINLOCK(g_strobeSMPLock);

static u32 strobe_Res;
static int g_timeOutTimeMs;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
static DEFINE_MUTEX(g_strobeSem);
#else
static DECLARE_MUTEX(g_strobeSem);
#endif

static struct work_struct workTimeOut;
static void work_timeOutFunc(struct work_struct *data);

int FL_Enable(void)
{
    mt_set_gpio_out(GPIO_CAMERA_FLASH_EN_PIN,GPIO_OUT_ONE);
    mt_set_gpio_out(GPIO_CAMERA_FLASH_MODE_PIN,GPIO_OUT_ZERO);
    PK_DBG("FL_Enable %d\n",__LINE__);
    return 0;
}

int FL_Disable(void)
{
    mt_set_gpio_out(GPIO_CAMERA_FLASH_EN_PIN,GPIO_OUT_ZERO);
    mt_set_gpio_out(GPIO_CAMERA_FLASH_MODE_PIN,GPIO_OUT_ZERO);
    PK_DBG("FL_Disable %d\n",__LINE__);
    return 0;
}

int FL_Init(void)
{
    if(mt_set_gpio_mode(GPIO_CAMERA_FLASH_EN_PIN,GPIO_MODE_00)){PK_DBG("[constant_flashlight] set gpio mode failed!! \n");}
    if(mt_set_gpio_dir(GPIO_CAMERA_FLASH_EN_PIN,GPIO_DIR_OUT)){PK_DBG("[constant_flashlight] set gpio dir failed!! \n");}
    if(mt_set_gpio_out(GPIO_CAMERA_FLASH_EN_PIN,GPIO_OUT_ZERO)){PK_DBG("[constant_flashlight] set gpio failed!! \n");}
    if(mt_set_gpio_mode(GPIO_CAMERA_FLASH_MODE_PIN,GPIO_MODE_00)){PK_DBG("[constant_flashlight] set gpio mode failed!! \n");}
    if(mt_set_gpio_dir(GPIO_CAMERA_FLASH_MODE_PIN,GPIO_DIR_OUT)){PK_DBG("[constant_flashlight] set gpio dir failed!! \n");}
    if(mt_set_gpio_out(GPIO_CAMERA_FLASH_MODE_PIN,GPIO_OUT_ZERO)){PK_DBG("[constant_flashlight] set gpio failed!! \n");}
    INIT_WORK(&workTimeOut, work_timeOutFunc);
    return 0;
}

int FL_Uninit(void)
{
    if(mt_set_gpio_mode(GPIO_CAMERA_FLASH_EN_PIN,GPIO_MODE_00)){PK_DBG("[constant_flashlight] set gpio mode failed!! \n");}
    if(mt_set_gpio_dir(GPIO_CAMERA_FLASH_EN_PIN,GPIO_DIR_OUT)){PK_DBG("[constant_flashlight] set gpio dir failed!! \n");}
    if(mt_set_gpio_out(GPIO_CAMERA_FLASH_EN_PIN,GPIO_OUT_ZERO)){PK_DBG("[constant_flashlight] set gpio failed!! \n");}
    return 0;
}

static void work_timeOutFunc(struct work_struct *data)
{
    FL_Disable();
}

enum hrtimer_restart ledTimeOutCallback(struct hrtimer *timer)
{
    schedule_work(&workTimeOut);
    return HRTIMER_NORESTART;
}

static struct hrtimer g_timeOutTimer;
void timerInit(void)
{
    g_timeOutTimeMs = 1000;
    hrtimer_init(&g_timeOutTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    g_timeOutTimer.function=ledTimeOutCallback;
}

void FL_Flash_Mode(void)
{
    PK_DBG("FL_Flash_Mode %d\n",__LINE__);
    if(mt_set_gpio_mode(GPIO_CAMERA_FLASH_MODE_PIN,GPIO_MODE_00)){PK_DBG("[constant_flashlight] set gpio mode failed!! \n");}
    if(mt_set_gpio_dir(GPIO_CAMERA_FLASH_MODE_PIN,GPIO_DIR_OUT)){PK_DBG("[constant_flashlight] set gpio dir failed!! \n");}
    if(mt_set_gpio_out(GPIO_CAMERA_FLASH_MODE_PIN,GPIO_OUT_ONE)){PK_DBG("[constant_flashlight] set gpio failed!! \n");}
}

void FL_Torch_Mode(void)
{
    PK_DBG("FL_Torch_Mode %d\n",__LINE__);
    if(mt_set_gpio_mode(GPIO_CAMERA_FLASH_MODE_PIN,GPIO_MODE_00)){PK_DBG("[constant_flashlight] set gpio mode failed!! \n");}
    if(mt_set_gpio_dir(GPIO_CAMERA_FLASH_MODE_PIN,GPIO_DIR_OUT)){PK_DBG("[constant_flashlight] set gpio dir failed!! \n");}
    if(mt_set_gpio_out(GPIO_CAMERA_FLASH_MODE_PIN,GPIO_OUT_ZERO)){PK_DBG("[constant_flashlight] set gpio failed!! \n");}
}

static int constant_flashlight_ioctl(MUINT32 cmd, MUINT32 arg)
{
    int i4RetValue;
    int ior_shift = cmd - (_IOR(FLASHLIGHT_MAGIC, 0, int));
    int iow_shift = cmd - (_IOW(FLASHLIGHT_MAGIC, 0, int));
    int iowr_shift = cmd - (_IOWR(FLASHLIGHT_MAGIC, 0, int));
    PK_DBG("constant_flashlight_ioctl() line=%d ior_shift=%d, iow_shift=%d iowr_shift=%d arg=%d\n",__LINE__, ior_shift, iow_shift, iowr_shift, arg);
    switch (cmd) {
        case FLASH_IOC_SET_TIME_OUT_TIME_MS:
            PK_DBG("FLASH_IOC_SET_TIME_OUT_TIME_MS: %d\n",arg);
            g_timeOutTimeMs=arg;
            break;

        case FLASH_IOC_SET_DUTY:
            PK_DBG("FLASHLIGHT_DUTY: %d\n",arg);
            break;

        case FLASH_IOC_SET_STEP:
            PK_DBG("FLASH_IOC_SET_STEP: %d\n",arg);
            break;

        case FLASH_IOC_SET_ONOFF:
            PK_DBG("FLASHLIGHT_ONOFF: %d\n",arg);
            if (arg == 1) {
                if(g_timeOutTimeMs != 0) {
                    ktime_t ktime = ktime_set(0, g_timeOutTimeMs * 1000000);
                    hrtimer_start(&g_timeOutTimer, ktime, HRTIMER_MODE_REL);
                }
                FL_Enable();
            } else {
                FL_Disable();
                hrtimer_cancel(&g_timeOutTimer);
            }
            break;
        default:
            PK_DBG("No such command\n");
            i4RetValue = -EPERM;
            break;
    }
    return i4RetValue;
}

static int constant_flashlight_open(void *pArg)
{
    int i4RetValue;
    PK_DBG("constant_flashlight_open\n");

    if (0 == strobe_Res) {
        FL_Init();
        timerInit();
    }

    spin_lock_irq(&g_strobeSMPLock);
    if(strobe_Res) {
        PK_ERR("busy!\n");
        i4RetValue = -EBUSY;
    } else
        strobe_Res += 1;
    spin_unlock_irq(&g_strobeSMPLock);

    return i4RetValue;
}


static int constant_flashlight_release(void *pArg)
{
    PK_DBG("constant_flashlight_release\n");
    if (strobe_Res) {
        spin_lock_irq(&g_strobeSMPLock);
        strobe_Res = 0;
        spin_unlock_irq(&g_strobeSMPLock);
        FL_Uninit();
    }
    return 0;
}

void flashlight_onoff(int level)
{
    if (strobe_Res == 0) {
        FL_Init();
        if (level == 119)
            FL_Flash_Mode();
        else {
            FL_Torch_Mode();
            if (!level) {
                FL_Disable();
                return;
            }
        }
        FL_Enable();
    }
}

FLASHLIGHT_FUNCTION_STRUCT constantFlashlightFunc = {
    constant_flashlight_open,
    constant_flashlight_release,
    constant_flashlight_ioctl
};


MUINT32 constantFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc)
{
    if (pfFunc != NULL)
        *pfFunc = &constantFlashlightFunc;
    return 0;
}

ssize_t strobe_VDIrq(void)
{
    return 0;
}

EXPORT_SYMBOL(strobe_VDIrq);
