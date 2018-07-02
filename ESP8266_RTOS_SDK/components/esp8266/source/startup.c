
#include "sdkconfig.h"
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "esp_wifi_osi.h"
#include "esp_image_format.h"

#define FLASH_MAP_ADDR 0x40200000

static void user_init_entry(void *param)
{
    void (*func)(void);

    extern void (__init_array_start)(void);
    extern void (__init_array_end)(void);

    extern void app_main(void);

    /* initialize C++ construture function */
    for (func = &__init_array_start; func < &__init_array_end; func++)
        func();

    tcpip_adapter_init();

    app_main();

    wifi_task_delete(NULL);
}

void call_user_start(void)
{
    int i;
    int *p;

    extern int _bss_start, _bss_end;

    extern void chip_boot(void);
    extern int rtc_init(void);
    extern int mac_init(void);
    extern int base_gpio_init(void);
    extern int phy_calibrate(void);
    extern int watchdog_init(void);
    extern int wifi_timer_init(void);
    extern int wifi_nvs_init(void);

    esp_image_header_t *head = (esp_image_header_t *)(FLASH_MAP_ADDR + CONFIG_PARTITION_TABLE_CUSTOM_APP_BIN_OFFSET);
    esp_image_segment_header_t *segment = (esp_image_segment_header_t *)((uintptr_t)head + sizeof(esp_image_header_t));

    for (i = 0; i < 3; i++) {
        segment = (esp_image_segment_header_t *)((uintptr_t)segment + sizeof(esp_image_segment_header_t) + segment->data_len);

        uint32_t *dest = (uint32_t *)segment->load_addr;
        uint32_t *src = (uint32_t *)((uintptr_t)segment + sizeof(esp_image_segment_header_t));
        uint32_t size = segment->data_len / sizeof(uint32_t);

        while (size--)
            *dest++ = *src++;
    }

    /* clear bss data */
    for (p = &_bss_start; p < &_bss_end; p++)
        *p = 0;

    __asm__ __volatile__(
        "rsil       a2, 2\n"
        "movi       a1, _chip_interrupt_tmp\n"
        "movi       a2, 0xffffff00\n"
        "and        a1, a1, a2\n"
        "movi       a2, 0x40100000\n"
        "wsr        a2, vecbase\n");

    chip_boot();

    wifi_os_init();

    assert(nvs_flash_init() == 0);
    assert(wifi_nvs_init() == 0);
    assert(rtc_init() == 0);
    assert(mac_init() == 0);
    assert(base_gpio_init() == 0);
    assert(phy_calibrate() == 0);
    assert(watchdog_init() == 0);
    assert(wifi_timer_init() == 0);
    assert(wifi_task_create(user_init_entry, "uiT", 512, NULL, wifi_task_get_max_priority()) != NULL);

    wifi_os_start();
}
