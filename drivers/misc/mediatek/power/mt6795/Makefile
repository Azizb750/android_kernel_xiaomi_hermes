include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-$(CONFIG_MTK_PMIC) += da9210.o pmic.o pmic_auxadc.o pmic_chr_type_det.o tps6128x.o upmu_common.o

obj-$(CONFIG_MTK_SMART_BATTERY) += battery_meter_hal.o 
ifeq ($(CONFIG_MTK_SWCHR_SUPPORT),y)
	obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_mt_sw_charger.o                
else
	obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_pmic.o 
endif
