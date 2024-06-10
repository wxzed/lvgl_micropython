/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Nicko van Someren
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>

#include "py/runtime.h"
#include "py/mphal.h"
#include "py/mperrno.h"
#include "extmod/vfs_fat.h"

#if MICROPY_HW_ENABLE_SDCARD

#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include "driver/spi_master.h"

//
// There are three layers of abstraction: host, slot and card.
// Creating an SD Card object will initialise the host and slot.
// Cards gets initialised by ioctl op==1 and de-inited by ioctl 2
// Hosts are de-inited in __del__. Slots do not need de-initing.
//

// Forward declaration
const mp_obj_type_t machine_sdcard_type;


typedef struct _micropy_sd_spi_bus_obj_t {
    spi_host_device_t host;
    int8_t sck;
    int8_t mosi;
    int8_t miso;
    int8_t active_devices;
    int state;

} micropy_sd_spi_bus_obj_t;


typedef struct _micropy_sd_spi_obj_t {
    mp_obj_base_t base;
    uint32_t baudrate;
    uint8_t polarity;
    uint8_t phase;
    uint8_t bits;
    uint8_t firstbit;
    int8_t cs;
    spi_device_handle_t spi_device;
    micropy_sd_spi_bus_obj_t *spi_bus;
} micropy_sd_spi_obj_t;


typedef struct _sdcard_obj_t {
    mp_obj_base_t base;
    mp_int_t flags;
    sdmmc_host_t host;
    // The card structure duplicates the host. It's not clear if we
    // can avoid this given the way that it is copied.
    sdmmc_card_t card;
} sdcard_card_obj_t;


#define SDCARD_CARD_FLAGS_HOST_INIT_DONE 0x01
#define SDCARD_CARD_FLAGS_CARD_INIT_DONE 0x02

#define _SECTOR_SIZE(self) (self->card.csd.sector_size)


static const sdspi_device_config_t spi_dev_defaults[2] = {
    {
        #if CONFIG_IDF_TARGET_ESP32
        .host_id = VSPI_HOST,
        .gpio_cs = GPIO_NUM_5,
        #else
        .host_id = SPI3_HOST,
        .gpio_cs = GPIO_NUM_34,
        #endif
        .gpio_cd = SDSPI_SLOT_NO_CD,
        .gpio_wp = SDSPI_SLOT_NO_WP,
        .gpio_int = SDSPI_SLOT_NO_INT,
    },
    SDSPI_DEVICE_CONFIG_DEFAULT(), // HSPI (ESP32) / SPI2 (ESP32S3)
};


static esp_err_t sdcard_ensure_card_init(sdcard_card_obj_t *self, bool force) {
    if (force || !(self->flags & SDCARD_CARD_FLAGS_CARD_INIT_DONE)) {

        esp_err_t err = sdmmc_card_init(&(self->host), &(self->card));
        if (err == ESP_OK) {
            self->flags |= SDCARD_CARD_FLAGS_CARD_INIT_DONE;
        } else {
            self->flags &= ~SDCARD_CARD_FLAGS_CARD_INIT_DONE;
        }

        return err;
    } else {
        return ESP_OK;
    }
}

/******************************************************************************/
// MicroPython bindings
//
// Expose the SD card or MMC as an object with the block protocol.

// Create a new SDCard object
// The driver supports either the host SD/MMC controller (default) or SPI mode
// In both cases there are two "slots". Slot 0 on the SD/MMC controller is
// typically tied up with the flash interface in most ESP32 modules but in
// theory supports 1, 4 or 8-bit transfers. Slot 1 supports only 1 and 4-bit
// transfers. Only 1-bit is supported on the SPI interfaces.
// card = SDCard(slot=1, width=None, present_pin=None, wp_pin=None)

static mp_obj_t machine_sdcard_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    // check arguments
    enum {
        ARG_slot,
        ARG_width,
        ARG_cd,
        ARG_wp,
        ARG_spi_bus,
        ARG_cs,
        ARG_freq,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_slot,     MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int =  1} },
        { MP_QSTR_width,    MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int =  1} },
        { MP_QSTR_cd,       MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_wp,       MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
        // These are only needed if using SPI mode
        { MP_QSTR_spi_bus,  MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_cs,       MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
        // freq is valid for both SPI and SDMMC interfaces
        { MP_QSTR_freq,     MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 20000000} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(make_new_args)];
    mp_arg_parse_all_kw_array(
        n_args,
        n_kw,
        all_args,
        MP_ARRAY_SIZE(make_new_args),
        make_new_args,
        args
    );

    bool is_spi;

    if (arg_vals[ARG_spi_bus].u_obj != mp_const_none) {
        micropy_sd_spi_obj_t *spi_bus = MP_OBJ_TO_PTR(args[ARG_spi_bus].u_obj);
        int cs = arg_vals[ARG_cs].u_int
        int slot_num = (int)spi_bus->spi_bus->host;
        is_spi = true;
    } else {
        int slot_num = arg_vals[ARG_slot].u_int;

        is_spi = false
        if (slot_num < 0 || slot_num > 3) {
            mp_raise_ValueError(MP_ERROR_TEXT("slot number must be between 0 and 3 inclusive"));
        }
    }

    sdcard_card_obj_t *self = mp_obj_malloc_with_finaliser(sdcard_card_obj_t, &machine_sdcard_type);
    self->flags = 0;
    // Note that these defaults are macros that expand to structure
    // constants so we can't directly assign them to fields.
    int freq = arg_vals[ARG_freq].u_int;
    if (is_spi) {
        sdmmc_host_t _temp_host = SDSPI_HOST_DEFAULT();
        _temp_host.max_freq_khz = freq / 1000;
        self->host = _temp_host;
    } else {
        sdmmc_host_t _temp_host = SDMMC_HOST_DEFAULT();
        _temp_host.max_freq_khz = freq / 1000;
        self->host = _temp_host;
    }

    if (is_spi) {
        // Needs to match spi_dev_defaults above.
        #if CONFIG_IDF_TARGET_ESP32
        self->host.slot = slot_num ? HSPI_HOST : VSPI_HOST;
        #else
        self->host.slot = slot_num ? SPI2_HOST : SPI3_HOST;
        #endif
    }

    check_esp_err(self->host.init());
    self->flags |= SDCARD_CARD_FLAGS_HOST_INIT_DONE;

    if (is_spi) {
        // SPI interface
        spi_host_device_t spi_host_id = self->host.slot;
        sdspi_device_config_t dev_config = spi_dev_defaults[slot_num];

        dev_config.gpio_cs = (int)arg_vals[ARG_cs].u_int;
        dev_config.gpio_cd = (int)arg_vals[ARG_cd].u_int;
        dev_config.gpio_wp = (int)arg_vals[ARG_wp].u_int;

        sdspi_dev_handle_t sdspi_handle;
        esp_err_t ret = sdspi_host_init_device(&dev_config, &sdspi_handle);

        check_esp_err(ret);

    } else {
        // SD/MMC interface
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

        // Stronger external pull-ups are still needed but apparently
        // it is a good idea to set the internal pull-ups anyway.
        // slot_config.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
        slot_config.gpio_cd = (int)arg_vals[ARG_cd].u_int;
        slot_config.gpio_wp = (int)arg_vals[ARG_wp].u_int;

        int width = arg_vals[ARG_width].u_int;
        if (width == 1 || width == 4 || (width == 8 && slot_num == 0)) {
            slot_config.width = width;
        } else {
            mp_raise_ValueError(MP_ERROR_TEXT("width must be 1 or 4 (or 8 on slot 0)"));
        }

        check_esp_err(sdmmc_host_init_slot(self->host.slot, &slot_config));
    }

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t sd_deinit(mp_obj_t self_in) {
    sdcard_card_obj_t *self = self_in;

    if (self->flags & SDCARD_CARD_FLAGS_HOST_INIT_DONE) {
        if (self->host.flags & SDMMC_HOST_FLAG_DEINIT_ARG) {
            self->host.deinit_p(self->host.slot);
        } else {
            self->host.deinit();
        }
        self->flags &= ~SDCARD_CARD_FLAGS_HOST_INIT_DONE;
    }

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_1(sd_deinit_obj, sd_deinit);


static mp_obj_t sd_info(mp_obj_t self_in) {
    sdcard_card_obj_t *self = self_in;
    // We could potential return a great deal more SD card data but it
    // is not clear that it is worth the extra code space to do
    // so. For the most part people only care about the card size and
    // block size.

    check_esp_err(sdcard_ensure_card_init((sdcard_card_obj_t *)self, false));

    uint32_t log_block_nbr = self->card.csd.capacity;
    uint32_t log_block_size = _SECTOR_SIZE(self);

    mp_obj_t tuple[2] = {
        mp_obj_new_int_from_ull((uint64_t)log_block_nbr * (uint64_t)log_block_size),
        mp_obj_new_int_from_uint(log_block_size),
    };
    return mp_obj_new_tuple(2, tuple);
}

static MP_DEFINE_CONST_FUN_OBJ_1(sd_info_obj, sd_info);


static mp_obj_t machine_sdcard_readblocks(mp_obj_t self_in, mp_obj_t block_num, mp_obj_t buf) {
    sdcard_card_obj_t *self = self_in;
    mp_buffer_info_t bufinfo;
    esp_err_t err;

    err = sdcard_ensure_card_init((sdcard_card_obj_t *)self, false);
    if (err != ESP_OK) {
        return false;
    }

    mp_get_buffer_raise(buf, &bufinfo, MP_BUFFER_WRITE);
    err = sdmmc_read_sectors(&(self->card), bufinfo.buf, mp_obj_get_int(block_num), bufinfo.len / _SECTOR_SIZE(self));

    return mp_obj_new_bool(err == ESP_OK);
}

static MP_DEFINE_CONST_FUN_OBJ_3(machine_sdcard_readblocks_obj, machine_sdcard_readblocks);


static mp_obj_t machine_sdcard_writeblocks(mp_obj_t self_in, mp_obj_t block_num, mp_obj_t buf) {
    sdcard_card_obj_t *self = self_in;
    mp_buffer_info_t bufinfo;
    esp_err_t err;

    err = sdcard_ensure_card_init((sdcard_card_obj_t *)self, false);
    if (err != ESP_OK) {
        return false;
    }

    mp_get_buffer_raise(buf, &bufinfo, MP_BUFFER_READ);
    err = sdmmc_write_sectors(&(self->card), bufinfo.buf, mp_obj_get_int(block_num), bufinfo.len / _SECTOR_SIZE(self));

    return mp_obj_new_bool(err == ESP_OK);
}

static MP_DEFINE_CONST_FUN_OBJ_3(machine_sdcard_writeblocks_obj, machine_sdcard_writeblocks);


static mp_obj_t machine_sdcard_ioctl(mp_obj_t self_in, mp_obj_t cmd_in, mp_obj_t arg_in) {
    sdcard_card_obj_t *self = self_in;
    esp_err_t err = ESP_OK;
    mp_int_t cmd = mp_obj_get_int(cmd_in);

    switch (cmd) {
        case MP_BLOCKDEV_IOCTL_INIT:
            err = sdcard_ensure_card_init(self, false);
            return MP_OBJ_NEW_SMALL_INT((err == ESP_OK) ? 0 : -1);

        case MP_BLOCKDEV_IOCTL_DEINIT:
            // Ensure that future attempts to look at info re-read the card
            self->flags &= ~SDCARD_CARD_FLAGS_CARD_INIT_DONE;
            return MP_OBJ_NEW_SMALL_INT(0); // success

        case MP_BLOCKDEV_IOCTL_SYNC:
            // nothing to do
            return MP_OBJ_NEW_SMALL_INT(0); // success

        case MP_BLOCKDEV_IOCTL_BLOCK_COUNT:
            err = sdcard_ensure_card_init(self, false);
            if (err != ESP_OK) {
                return MP_OBJ_NEW_SMALL_INT(-1);
            }
            return MP_OBJ_NEW_SMALL_INT(self->card.csd.capacity);

        case MP_BLOCKDEV_IOCTL_BLOCK_SIZE:
            err = sdcard_ensure_card_init(self, false);
            if (err != ESP_OK) {
                return MP_OBJ_NEW_SMALL_INT(-1);
            }
            return MP_OBJ_NEW_SMALL_INT(_SECTOR_SIZE(self));

        default: // unknown command
            return MP_OBJ_NEW_SMALL_INT(-1); // error
    }
}

static MP_DEFINE_CONST_FUN_OBJ_3(machine_sdcard_ioctl_obj, machine_sdcard_ioctl);


static const mp_rom_map_elem_t machine_sdcard_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_info), MP_ROM_PTR(&sd_info_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&sd_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&sd_deinit_obj) },
    // block device protocol
    { MP_ROM_QSTR(MP_QSTR_readblocks), MP_ROM_PTR(&machine_sdcard_readblocks_obj) },
    { MP_ROM_QSTR(MP_QSTR_writeblocks), MP_ROM_PTR(&machine_sdcard_writeblocks_obj) },
    { MP_ROM_QSTR(MP_QSTR_ioctl), MP_ROM_PTR(&machine_sdcard_ioctl_obj) },
};

static MP_DEFINE_CONST_DICT(machine_sdcard_locals_dict, machine_sdcard_locals_dict_table);


MP_DEFINE_CONST_OBJ_TYPE(
    machine_sdcard_type,
    MP_QSTR_SDCard,
    MP_TYPE_FLAG_NONE,
    make_new, machine_sdcard_make_new,
    locals_dict, &machine_sdcard_locals_dict
    );

#endif // MICROPY_HW_ENABLE_SDCARD