#
# Makefile for misc devices that really don't fit anywhere else.
#
include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-y += mt65xx_lcm_list.o tps65132.o

LCM_LISTS := $(subst ",,$(CONFIG_CUSTOM_KERNEL_LCM))
obj-y += $(foreach LCM,$(LCM_LISTS),$(LCM)/)

