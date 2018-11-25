#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <mach/mt_pwm.h>
#include <mach/mt_pwm_hal.h>
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <mach/mt_pm_ldo.h>

#include "mt_consumerir.h"

#ifdef GPIO_IRTX_OUT_PIN
#define CONSUMERIR_GPIO GPIO_IRTX_OUT_PIN
#else
#define CONSUMERIR_GPIO GPIO129
#endif

struct mt_irtx mt_irtx_dev;
void __iomem *irtx_reg_base;
unsigned int irtx_irq;

struct pwm_spec_config irtx_pwm_config = {
	.pwm_no = 0,
	.mode = PWM_MODE_MEMORY,
	.clk_div = CLK_DIV1,
	.clk_src = PWM_CLK_NEW_MODE_BLOCK,
	.pmic_pad = 0,
	.PWM_MODE_MEMORY_REGS.IDLE_VALUE = IDLE_FALSE,
	.PWM_MODE_MEMORY_REGS.GUARD_VALUE = GUARD_FALSE,
	.PWM_MODE_MEMORY_REGS.STOP_BITPOS_VALUE = 31,
	.PWM_MODE_MEMORY_REGS.HDURATION = 25, /* 1 microseconds, assume clock source is 26M */
	.PWM_MODE_MEMORY_REGS.LDURATION = 25,
	.PWM_MODE_MEMORY_REGS.GDURATION = 0,
	.PWM_MODE_MEMORY_REGS.WAVE_NUM = 1,
};

static int dev_char_open()
{
	if (atomic_read(&mt_irtx_dev.usage_cnt))
		return -EBUSY;
	spin_lock(&mt_irtx_dev.usage_cnt);
	atomic_inc(&mt_irtx_dev.usage_cnt);
	spin_unlock(&mt_irtx_dev.usage_cnt);

	return 0;
}

static int dev_char_release()
{
	if (atomic_read(&mt_irtx_dev.usage_cnt))
		return -EBUSY;
	spin_lock(&mt_irtx_dev.usage_cnt);
	atomic_dec(&mt_irtx_dev.usage_cnt);
	spin_unlock(&mt_irtx_dev.usage_cnt);

	return 0;
}

static ssize_t dev_char_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	return 0;
}

static long dev_char_ioctl( struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned int para = 0, gpio_id = -1, en = 0;
	unsigned long mode = 0, outp = 0;

	switch (cmd) {
	case IRTX_IOC_SET_CARRIER_FREQ:
	    if (copy_from_user(&mt_irtx_dev.carrier_freq, (void __user *)arg, sizeof(unsigned int))) {
		    pr_err("[consumerir] IRTX_IOC_SET_CARRIER_FREQ: copy_from_user fail!\n");
		    ret = -EFAULT;
	    } else {
		    pr_warn("[consumerir] IRTX_IOC_SET_CARRIER_FREQ: %d\n", mt_irtx_dev.carrier_freq);
		    if (!mt_irtx_dev.carrier_freq) {
			    ret = -EINVAL;
			    mt_irtx_dev.carrier_freq = 38000;
		    }
		}
		break;

	case IRTX_IOC_SET_IRTX_LED_EN:
	    if (copy_from_user(&para, (void __user *)arg, sizeof(unsigned int))) {
		    pr_err("[consumerir] IRTX_IOC_SET_IRTX_LED_EN: copy_from_user fail!\n");
		    ret = -EFAULT;
	    } else {
		    /* en: bit 12; */
		    /* gpio: bit 0-11 */
		    gpio_id = (unsigned long)((para & 0x0FFF0000) > 16);
		    en = (para & 0xF);
		    pr_warn("[consumerir] IRTX_IOC_SET_IRTX_LED_EN: 0x%x, gpio_id:%ul, en:%ul\n", para, gpio_id, en);
		    if (en) {
			    mode = GPIO_MODE_03;
			    outp = GPIO_OUT_ZERO; /* Low means enable LED */
		    } else {
			    mode = GPIO_MODE_00;
			    outp = GPIO_OUT_ONE;  /* High means disable LED */
		    }
		    gpio_id = CONSUMERIR_GPIO;

		    mt_set_gpio_mode(gpio_id, mode);
		    mt_set_gpio_dir(gpio_id, GPIO_DIR_OUT);
		    mt_set_gpio_out(gpio_id, outp);

		    pr_warn("[consumerir] IOC_SET_IRTX_LED_EN: gpio:0x%xl, mode:%d, dir:%d, out:%d\n", gpio_id
			    , mt_get_gpio_mode(gpio_id), mt_get_gpio_dir(gpio_id), mt_get_gpio_out(gpio_id));
	    }
        break;

	default:
	    pr_err("[consumerir] unknown ioctl cmd 0x%x\n", cmd);
	    ret = -ENOTTY;
        break;
	}
	return ret;
}

static void set_consumerir_pwm(int a1)
{
/*  signed int v2; // w4
  signed int v3; // w4
  __int16 v4; // w24
  signed __int16 v5; // w23
  int v6; // w21
  signed int v7; // w4

  printk("%s:zlya carrier_freq =%d\n");
  if ( a1 - 10001 > 89998 )
  {
    v5 = 20;
    v6 = 38690;
LABEL_6:
    v4 = v5;
    goto LABEL_4;
  }
  v2 = 1625000 / a1;
  if ( !(1625000 / a1 & 1) )
  {
    v7 = v2 >> 1;
    v5 = v7 - 1;
    v6 = 1625000 / (2 * v7);
    goto LABEL_6;
  }
  v3 = v2 >> 1;
  v4 = v3;
  v5 = v3 - 1;
  v6 = 1625000 / (v3 - 1 + v3 + 2);
LABEL_4:
  printk("zlya high =%d,low = %d,freq = %d\n");
  dword_FFFFFFC000ED48E0 = 10000000 / v6;
  printk("%s first one_clock_time = %d\n");
  word_FFFFFFC000ED46E4 = v5;
  word_FFFFFFC000ED46E6 = v4;
  printk(&unk_FFFFFFC000D32F88);
}*/
	unsigned int ir_conf_wr;
	unsigned int L0H, L0L, L1H, L1L;
	unsigned int sync_h, sync_l;
	unsigned int cdt, cwt;
	struct irtx_config ir_conf;

	pr_warn("[consumerir] configure IrTx software mode\n");

	ir_conf.mode = 3;
	ir_conf.start = 0;
	ir_conf.sw_o = 0;
	ir_conf.b_ord = 1; /* LSB first */
	ir_conf.r_ord = 0; /* R0 first */
	ir_conf.ir_os = 1; /* modulated signal */
	ir_conf.ir_inv = 1;
	ir_conf.bit_num = 0;
	ir_conf.data_inv = 0;
	L0H = 0;
	L0L = (mt_irtx_dev.pwm_ch+1) & 0x7; /* FIXME, workaround for Denali, HW will fix on Jade */
	L1H = 0;
	L1L = 0;
	sync_h = 0;
	sync_l = 0;
	cwt = (CLOCK_SRC*1000*1000)/(mt_irtx_dev.carrier_freq); /* carrier freq. */
	cdt = cwt/3; /* duty=1/3 */

	memcpy(&ir_conf_wr, &ir_conf, sizeof(ir_conf));
	irtx_write32(mt_irtx_dev.reg_base, IRTXCFG, ir_conf_wr);
	irtx_write32(mt_irtx_dev.reg_base, IRTX_L0H, L0H);
	irtx_write32(mt_irtx_dev.reg_base, IRTX_L0L, L0L);
	irtx_write32(mt_irtx_dev.reg_base, IRTX_L1H, L1H);
	irtx_write32(mt_irtx_dev.reg_base, IRTX_L1L, L1L);
	irtx_write32(mt_irtx_dev.reg_base, IRTXSYNCH, sync_h);
	irtx_write32(mt_irtx_dev.reg_base, IRTXSYNCL, sync_l);
	irtx_write32(mt_irtx_dev.reg_base, IRTXMT, (cdt<<16)|(cwt&0xFFFF));

	pr_warn("[consumerir] configured IrTx: cfg=%x L0=%x/%x L1=%x/%x sync=%x/%x mt=%x/%x\n"
		    , ir_conf_wr, L0H, L0L, L1H, L1L, sync_h, sync_l, cdt, cwt);
	pr_warn("[consumerir] configured cfg=0x%x", (unsigned int)irtx_read32(mt_irtx_dev.reg_base, IRTXCFG));
}

static ssize_t dev_char_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	dma_addr_t wave_phy;
	void *wave_vir;
	int ret;
	int buf_size = (count + 3) / 4;  /* when count is 5... */

	pr_warn("[consumerir] irtx write len=0x%x, pwm=%d\n", (unsigned int)count, (unsigned int)irtx_pwm_config.pwm_no);
	wave_vir = dma_alloc_coherent(&mt_irtx_dev.plat_dev->dev, count, &wave_phy, GFP_KERNEL);
	if (!wave_vir) {
		pr_err("[consumerir] alloc memory fail\n");
		return -ENOMEM;
	}
	ret = copy_from_user(wave_vir, buf, count);
	if (ret) {
		pr_err("[consumerir] write, copy from user fail %d\n", ret);
		goto exit;
	}

	hwPowerOn(5, VOL_3300, "consumerir");
	mt_set_intr_enable(0);
	mt_set_intr_enable(1);
	mt_pwm_26M_clk_enable_hal(1);
	pr_warn("[consumerir] irtx before read IRTXCFG:0x%x\n", (irtx_read32(mt_irtx_dev.reg_base, IRTXCFG)));
	irtx_pwm_config.PWM_MODE_MEMORY_REGS.BUF0_BASE_ADDR = (U32 *)wave_phy;
	irtx_pwm_config.PWM_MODE_MEMORY_REGS.BUF0_SIZE = (buf_size ? (buf_size - 1) : 0);

	//set_consumerir_pwm(mt_irtx_dev.carrier_freq);
	mt_set_gpio_mode(CONSUMERIR_GPIO, GPIO_MODE_03);
	mt_set_gpio_dir(CONSUMERIR_GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(CONSUMERIR_GPIO, GPIO_OUT_ZERO);

	//irtx_write32(mt_irtx_dev.reg_base, IRTXCFG, irtx_read32(mt_irtx_dev.reg_base, IRTXCFG)|0x1); /* STRT=1 */

	mt_set_intr_ack(0);
	mt_set_intr_ack(1);
	ret = pwm_set_spec_config(&irtx_pwm_config);
	pr_warn("[consumerir] pwm is triggered, %d\n", ret);

	msleep(count * 8 / 1000);
	msleep(100);
	ret = count;

exit:
	pr_warn("[consumerir] done, clean up\n");
	dma_free_coherent(&mt_irtx_dev.plat_dev->dev, count, wave_vir, wave_phy);
	//irtx_write32(mt_irtx_dev.reg_base, IRTXCFG, irtx_read32(mt_irtx_dev.reg_base, IRTXCFG)&0xFFFFFFF7); /* SWO=0 */
	//irtx_write32(mt_irtx_dev.reg_base, IRTXCFG, irtx_read32(mt_irtx_dev.reg_base, IRTXCFG)&0xFFFFFFFE); /* STRT=0 */
	mt_pwm_disable(irtx_pwm_config.pwm_no, irtx_pwm_config.pmic_pad);
	hwPowerDown(5, "consumerir");
	mt_set_gpio_mode(CONSUMERIR_GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(CONSUMERIR_GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(CONSUMERIR_GPIO, GPIO_OUT_ONE);

	return ret;
}
/*
static irqreturn_t irtx_isr(int irq, void *data)
{
	return IRQ_HANDLED;
}
*/
#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))
static u64 irtx_dma_mask = DMA_BIT_MASK((sizeof(unsigned long)<<3)); 

static struct file_operations char_dev_fops = {
	.owner = THIS_MODULE,
	.open = &dev_char_open,
	.read = &dev_char_read,
	.write = &dev_char_write,
	.release = &dev_char_release,
	.unlocked_ioctl = &dev_char_ioctl,
};

static int consumerir_probe(struct platform_device *plat_dev)
{
	struct cdev *c_dev;
	dev_t dev_t_consumerir;
	struct device *dev = NULL;
	static void *dev_class;
	u32 major = 0, minor = 0;
	int ret = 0;
	int error = 0;

	mt_set_gpio_mode(CONSUMERIR_GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(CONSUMERIR_GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(CONSUMERIR_GPIO, GPIO_OUT_ZERO);

	if (!major) {
		error = alloc_chrdev_region(&dev_t_consumerir, 0, 1, "consumerir");
		if (!error) {
			major = MAJOR(dev_t_consumerir);
			minor = MINOR(dev_t_consumerir);
		}
	} else {
		dev_t_consumerir = MKDEV(major, minor);
	}

	/*ret = request_irq(mt_irtx_dev.irq, irtx_isr, IRQF_TRIGGER_FALLING, "IRTX", NULL);
	if (ret) {
		pr_err("[consumerir] request IRQ(%d) fail ret=%d\n", mt_irtx_dev.irq, ret);
		goto exit;
	}*/
	irtx_pwm_config.pwm_no = mt_irtx_dev.pwm_ch;

	mt_irtx_dev.plat_dev = plat_dev;
	mt_irtx_dev.plat_dev->dev.dma_mask = &irtx_dma_mask;
	mt_irtx_dev.plat_dev->dev.coherent_dma_mask = irtx_dma_mask;
	//atomic_set(&mt_irtx_dev.usage_cnt, 0);
	mt_irtx_dev.carrier_freq = 38000; /* NEC as default */

	/*ret = register_chrdev_region(dev_t_consumerir, 1, "consumerir");
	if (ret) {
		pr_err("[consumerir] register_chrdev_region fail ret=%d\n", ret);
		goto exit;
	}
	c_dev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
	if (!c_dev) {
		pr_err("[consumerir] kmalloc cdev fail\n");
		goto exit;
	}*/
    c_dev = cdev_alloc();
	cdev_init(c_dev, &char_dev_fops);
	c_dev->owner = THIS_MODULE;
	ret = cdev_add(c_dev, dev_t_consumerir, 1);
	if (ret) {
		ret = -ENOMEM;
		pr_err("[consumerir] cdev_add fail ret=%d\n", ret);
		goto exit;
	}
	dev_class = class_create(THIS_MODULE, "consumerir");
	dev = device_create(dev_class, NULL, dev_t_consumerir, NULL, "consumerir");
	if (IS_ERR(dev)) {
		ret = -EIO;
		pr_err("[consumerir] device_create fail ret=%d\n", ret);
		goto exit;
	}

exit:
	pr_warn("[consumerir] irtx probe ret=%d\n", ret);
	return ret;
}

static struct platform_driver consumerir_driver =
{
	.driver = {
		.name = "consumerir",
		.owner = THIS_MODULE
	},
	.probe = consumerir_probe
};

static struct platform_device consumerir_device = {
	.name = "consumerir",
	.id = -1,
};

static int __init consumerir_init(void)
{
	int ret = 0;

	pr_warn("[consumerir] irtx init\n");

	ret = platform_device_register(&consumerir_device);
	if (ret) {
		pr_err("[consumerir] irtx platform device register fail\n");
		ret = -ENODEV;
		goto exit;
	}

	ret = platform_driver_register(&consumerir_driver);
	if (ret) {
		pr_err("[consumerir] irtx platform driver register fail\n");
		ret = -ENODEV;
		goto exit;
	}

exit:
	return ret;
}


module_init(consumerir_init);

MODULE_AUTHOR("Xiao Wang <xiao.wang@mediatek.com>");
MODULE_DESCRIPTION("Consumer IR transmitter driver v0.1");
