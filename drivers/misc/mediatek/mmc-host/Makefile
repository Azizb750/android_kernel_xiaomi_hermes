
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/

ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
ifeq ($(CONFIG_ARM64), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/gud/$(MTK_PLATFORM)/gud/MobiCoreKernelApi/include \
             -I$(srctree)/drivers/misc/mediatek/gud/$(MTK_PLATFORM)/gud/MobiCoreKernelApi/public 
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/gud/$(MTK_PLATFORM)/gud/MobiCoreKernelApi/include \
             -I$(srctree)/drivers/misc/mediatek/gud/$(MTK_PLATFORM)/gud/MobiCoreKernelApi/public 
endif
endif

ccflags-y += -I$(srctree)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmc-host/$(MTK_PLATFORM)
