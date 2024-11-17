#include "soc/soc_caps.h"

#if SOC_LCD_RGB_SUPPORTED
    #ifndef _ESP32_RGB_BUS_H_
        #define _ESP32_RGB_BUS_H_

        //local_includes
        #include "lcd_types.h"

        // esp-idf includes
        #include "esp_lcd_panel_io.h"
        #include "esp_lcd_panel_rgb.h"

        #include "freertos/FreeRTOS.h"
        #include "freertos/task.h"
        #include "freertos/semphr.h"
        #include "freertos/event_groups.h"
        #include "freertos/idf_additions.h"

        // micropython includes
        #include "mphalport.h"
        #include "py/obj.h"
        #include "py/objarray.h"


        typedef struct _rgb_bus_lock_t {
            SemaphoreHandle_t handle;
            StaticSemaphore_t buffer;
        } rgb_bus_lock_t;

        typedef struct _rgb_bus_event_t {
            EventGroupHandle_t handle;
            StaticEventGroup_t buffer;
        } rgb_bus_event_t;


        typedef struct _mp_lcd_rgb_bus_obj_t {
            mp_obj_base_t base;

            mp_obj_t callback;

            mp_obj_array_t *view1;
            mp_obj_array_t *view2;

            uint32_t buffer_flags;

            bool trans_done;
            bool rgb565_byte_swap;

            lcd_panel_io_t panel_io_handle;

            esp_lcd_rgb_panel_config_t panel_io_config;
            esp_lcd_rgb_timing_t bus_config;

            esp_lcd_panel_handle_t panel_handle;
            uint32_t buffer_size;

            uint8_t *active_fb;
            uint8_t *idle_fb;
            uint8_t *partial_buf;

            int x_start;
            int y_start;
            int x_end;
            int y_end;
            uint16_t width;
            uint16_t height;
            uint8_t rotation: 2;
            uint8_t bytes_per_pixel: 2;

            rgb_bus_lock_t copy_lock;
            rgb_bus_event_t copy_task_exit;
            rgb_bus_event_t last_update;
            rgb_bus_event_t partial_copy;
            rgb_bus_event_t swap_bufs;
            rgb_bus_lock_t swap_lock;

            TaskHandle_t copy_task_handle;

        } mp_lcd_rgb_bus_obj_t;

        void rgb_bus_event_init(rgb_bus_event_t *event);
        void rgb_bus_event_delete(rgb_bus_event_t *event);
        bool rgb_bus_event_isset(rgb_bus_event_t *event);
        void rgb_bus_event_set(rgb_bus_event_t *event);
        void rgb_bus_event_clear(rgb_bus_event_t *event);
        void rgb_bus_event_clear_from_isr(rgb_bus_event_t *event);
        bool rgb_bus_event_isset_from_isr(rgb_bus_event_t *event);
        void rgb_bus_event_set_from_isr(rgb_bus_event_t *event);

        int  rgb_bus_lock_acquire(rgb_bus_lock_t *lock, int32_t wait_ms);
        void rgb_bus_lock_release(rgb_bus_lock_t *lock);
        void rgb_bus_lock_init(rgb_bus_lock_t *lock);
        void rgb_bus_lock_delete(rgb_bus_lock_t *lock);
        void rgb_bus_lock_release_from_isr(rgb_bus_lock_t *lock);

        void rgb_bus_copy_task(void *self_in);

        extern const mp_obj_type_t mp_lcd_rgb_bus_type;

    #endif /* _ESP32_RGB_BUS_H_ */
#else
    #include "../common_include/rgb_bus.h"

#endif /*SOC_LCD_RGB_SUPPORTED*/