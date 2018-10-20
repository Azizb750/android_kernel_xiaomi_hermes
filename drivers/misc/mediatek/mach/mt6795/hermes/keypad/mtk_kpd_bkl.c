#include <mach/mt_typedefs.h>
#include <mtk_kpd.h>

#if KPD_DRV_CTRL_BACKLIGHT
void kpd_enable_backlight(void) {}
void kpd_disable_backlight(void) {}
#endif

void kpd_set_backlight(bool onoff, void *val1, void *val2) {}
