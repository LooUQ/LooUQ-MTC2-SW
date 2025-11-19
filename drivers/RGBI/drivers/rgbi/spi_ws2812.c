/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * Copyright (c) 2025 LooUQ Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT rgbi_spi_ws2812

#include <stdint.h>
#include <stdbool.h> 

#include <rgbi.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/sys/util.h>

#define LOG_LEVEL CONFIG_RGBI_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(rgbi_spi_ws2812);

#define STRIP_NUM_PIXELS (1)
#define FLASH_CONTINUOUS (0)
#define IS_FLASHING(dev) dev_data(dev)->onDuration.ticks > 0

struct rgbi_spi_ws2812_data
{
    struct led_rgb pixels;                              // colors/intensity 
    k_timeout_t onDuration;                             // flash on duration, 0=no flash active
    k_timeout_t offDuration;                            // flash off duration 
    unsigned int flashesRequested;                      // number of flashes requested via API f()
    unsigned int flashesCompleted;                      // number of flashes completed in sequence (tallied when LED is turned ON)
    bool flashStateOn;                                  // if flash sequence active, current state of LED

    struct k_timer rgbiTimer;                           // timer object to measure duration of ON or OFF segment
    struct k_work rgbiWork;                             // work object to monitor state and update LED (invoked at timer expiry)
    const struct device *rgbi_dev;                      // required for work handler to find parent device in flash
};

struct rgbi_spi_ws2812_config
{
    const struct device *led_strip;                    // pointer to parent led_strip driver (WS2812 device)
};


/**
 * @brief Convenience function to get device config
 * 
 * @param dev The RGB Indicator (dev) to actuate
 * @return const struct rgbi_spi_ws2812_config* Pointer to device configuration struct
 */
static const struct rgbi_spi_ws2812_config *dev_config(const struct device *dev)
{
    return dev->config;
}

/**
 * @brief Convienence function to get device data
 * 
 * @param dev The RGB Indicator (dev) to actuate
 * @return const struct rgbi_spi_ws2812_data* Pointer to device internal data struct
 */
static struct rgbi_spi_ws2812_data *dev_data(const struct device *dev)
{
    return dev->data;
}


// /* Private service function declarations
//  * ------------------------------------------------------------------------- */
// static bool _isFlashing(const struct device *dev);


/* ============================================================================ */
/* Planned API
void rgbi_setColor(const struct device *dev, const struct led_rgb * pixels);
void rgbi_setColorFromPixels(const struct device *dev, rgbi_color_t red, rgbi_color_t green, rgbi_color_t blue);
void rgbi_off(const struct device *dev);
void rgbi_flash(const struct device *dev, struct led_rgb * pixels, k_timeout_t onDuration, k_timeout_t offDuration, uint8_t count);
void rgbi_flash_continuous(const struct device *dev, struct led_rgb pixels, k_timeout_t onDuration, k_timeout_t offDuration);
void rgbi_cancel(const struct device *dev);
bool rgbi_isBusy(const struct device *dev);
*/

/* ============================================================================ */
// /* Public API
//  * ========================================================================= */

/**
 * @brief Set the indicator color/brightness from an RGB struct object
 * 
 * @param dev The RGB Indicator (dev) to actuate

 * @param pixels Struct containing the brightness for each of the RGB pixels
 * @return int Error status of the action
 */
static int rgbi_spi_ws2812_setColor(const struct device *dev, const struct led_rgb *pixels)
{
    const struct device *led_strip = dev_config(dev)->led_strip;
    int ret = led_strip_update_rgb(led_strip, (struct led_rgb *)pixels, STRIP_NUM_PIXELS);
    return ret;
}


/**
 * @brief Set the indicator color/brightness from individual RGB color values
 * 
 * @param dev The RGB Indicator (dev) to actuate

 * @param red Red pixel intensity 0 (off) - 255 (full-brightness)
 * @param green Green pixel intensity 0 (off) - 255 (full-brightness)
 * @param blue Blue pixel intensity 0 (off) - 255 (full-brightness)
 * @return int Error status of the action
 */
static int rgbi_spi_ws2812_setColorFromPixels(const struct device *dev, rgbi_color_t red, rgbi_color_t green, rgbi_color_t blue)
{
    CREATE_RGB_PIXELS(pixels, red, green, blue);
    return rgbi_spi_ws2812_setColor(dev, &pixels);
}


/**
 * @brief Turn indicator off
 * 
 * @param dev The RGB Indicator (dev) to actuate

 * @return int Error status of the action
 */
static int rgbi_spi_ws2812_off(const struct device *dev)
{
    if (IS_FLASHING(dev))                                                       // do not change indicator if flash sequence underway
    {
        return -EBUSY;
    }
    return rgbi_setColorFromPixels(dev, 0, 0, 0);
}


/**
 * @brief Setup/initiate a indicator flash sequence
 *
 * @param dev The RGB Indicator (dev) to actuate

 * @param colors The color set to display when flash indicates ON
 * @param onDuration The amount of time the indicator is on
 * @param offDuration The amount of time the indicator is off
 * @param count The number of "on" flashes to display, if count==0 the flash sequence continues until cancelled
 */
static int rgbi_spi_ws2812_flash(const struct device *dev, const struct led_rgb * pixels, k_timeout_t onDuration, k_timeout_t offDuration, uint8_t count)
{
    if (IS_FLASHING(dev))
    {
        return -EBUSY;
    }

	struct rgbi_spi_ws2812_data *data = dev_data(dev);

    data->onDuration = onDuration;
    data->offDuration = offDuration;
    data->flashesRequested = count;                                         // number of ON pulses OR 0==continuous
    data->flashesCompleted = 0;
    memcpy(&(data->pixels), pixels, sizeof(struct led_rgb));                // copy pixels into object (for next flash ON event)

    rgbi_setColor(dev, pixels);                                             // start the sequence, the flashExpiry() callback will take it from here
    k_timer_start(&data->rgbiTimer, data->onDuration, K_NO_WAIT);
    data->flashStateOn = true;

    return 0;
}


/**
 * @brief 
 * 
 * @param dev 
 * @param pixels 
 * @param onDuration 
 * @param offDuration 
 * @return int 
 */
static int rgbi_spi_ws2812_flashContinuous(const struct device *dev, const struct led_rgb *pixels, k_timeout_t onDuration, k_timeout_t offDuration)
{
    rgbi_spi_ws2812_flash(dev, pixels, onDuration, offDuration, FLASH_CONTINUOUS);
    return 0;
}


/**
 * @brief Let caller know if the indicator in busy displaying a flash sequence
 *
 * @param dev The RGB Indicator (dev) to actuate

 * @return true Flash sequence is underway, could be continuous
 * @return false The indicator is idle (not executing a flash sequence)
 */
static int rgbi_spi_ws2812_cancel(const struct device *dev)
{
    k_timer_stop(&dev_data(dev)->rgbiTimer);                              // stop timer, prevent expiry event
    dev_data(dev)->onDuration = K_NO_WAIT;                                // clear flashing: onDuration==0 indicates idle
    rgbi_off(dev);                                                        // idle state is LED OFF
    return 0;
}


/**
 * @brief Cancel flash sequence, if underway, and turn indicator off
 *
 * @param dev The RGB Indicator (dev) to actuate

 */
static bool rgbi_spi_ws2812_isBusy(const struct device *dev)
{
    return IS_FLASHING(dev);
}

/* =============================================================*/
/* =============================================================*/
/* =============================================================*/


/* Assign driver functions to API
 * ------------------------------------------------------------------------- */
static DEVICE_API(rgbi, rgbi_spi_ws2812_api) = {
    .setColor = &rgbi_spi_ws2812_setColor,
    .setColorFromPixels = &rgbi_spi_ws2812_setColorFromPixels,
    .off = &rgbi_spi_ws2812_off,
    .flash = &rgbi_spi_ws2812_flash,
    .flashContinuous = &rgbi_spi_ws2812_flashContinuous,
    .cancel = &rgbi_spi_ws2812_cancel,
    .isBusy = &rgbi_spi_ws2812_isBusy
};


/* Private service functions definitions
 * ------------------------------------------------------------------------- */

// /**
//  * @brief Convenience function to determine flash state
//  * 
//  * @param dev The RGB Indicator (dev) to actuate
//  * @return true The indicator is performing a flash sequence
//  * @return false The indicator is idle (not flashing)
//  */
// static bool _isFlashing(const struct device *dev)
// {
//     return dev_data(dev)->onDuration.ticks > 0;
// }


/**
 * @brief Callback from flash timer expiration
 * 
 * @param timer System timer for performing flash sequence
 */
static void rgbi_spi_ws2812_on_timer_expire(struct k_timer *timer)
{
    const struct device *dev = k_timer_user_data_get(timer);
    k_work_submit(&dev_data(dev)->rgbiWork);                                    // submit the work item to system WQ to update flash sequence
}


// /**
//  * @brief For a flashWork struct find the parent RGBI device
//  * 
//  * @param flashWork The flashWork struct to locate
//  * @return const struct device* Pointer to the parent RGBI device. Returns NULL if no parent device found.
//  */
// static const struct device * rgbi_spi_findInstance(struct k_work *flashWork)
// {
    // #if (DT_NODE_EXISTS(DT_INST(0, DT_DRV_COMPAT)))
    //     const struct device *dt_search = DEVICE_DT_GET(DT_INST(0, DT_DRV_COMPAT));
    //     struct rgbi_spi_ws2812_config *search_config = dev_config(dt_search);

    //     if (&search_config->rgbiWork == flashWork)
    //     {
    //         LOG_INF("%s-inst:0 is target", dt_search->name);
    //         return dt_search;
    //     }
    // #endif

    // #if (DT_NODE_EXISTS(DT_INST(1, DT_DRV_COMPAT)))
    //     const struct device *dt_search = DEVICE_DT_GET(DT_INST(0, DT_DRV_COMPAT));
    //     struct rgbi_spi_ws2812_data *search_data = dev_data(dt_search);

    //     if (&search_data->flashWork == flashWork)
    //     {
    //         LOG_INF("%s-inst:1 is target", dt_search->name);
    //         return dt_search;
    //     }
    // #endif

    // #if (DT_NODE_EXISTS(DT_INST(2, DT_DRV_COMPAT)))
    //     const struct device *dt_search = DEVICE_DT_GET(DT_INST(0, DT_DRV_COMPAT));
    //     struct rgbi_spi_ws2812_data *search_data = dev_data(dt_search);

    //     if (&search_data->flashWork == flashWork)
    //     {
    //         LOG_INF("%s-inst:2 is target", dt_search->name);
    //         return dt_search;
    //     }
    // #endif
//
//     return NULL;
// }


/**
 * @brief Handler to perform RGB indicator (dev) update (flash ON>>OFF or OFF>>ON)
 *
 * @param flashWork workqueue item to process
 */
static void rgbi_spi_ws2812_flashWorkHandler(struct k_work *flashWork)
{
    struct rgbi_spi_ws2812_data *data = CONTAINER_OF(flashWork, struct rgbi_spi_ws2812_data, rgbiWork);     // get ptr device instance data (void* req'd for next CONTAINER_OF)

    if (data->onDuration.ticks > 0)
    {
        if (data->flashStateOn)                                                 //-- indicator is ON currently
        {
            rgbi_setColorFromPixels(data->rgbi_dev, 0, 0, 0);                   // can't use rgbi_off inside flash sequence
            data->flashStateOn = false;
            data->flashesCompleted++;                                           // completed tallied with ON flash

            if (data->flashesCompleted < data->flashesRequested || data->flashesRequested == FLASH_CONTINUOUS)
            {
                k_timer_start(&(data->rgbiTimer), data->offDuration, K_NO_WAIT);    // wait out OFF (for next ON event)
            }
            else                                                                // done with flash sequence, reset
            {
                data->flashesRequested = 0;
                data->flashesCompleted = 0;
                data->onDuration = K_NO_WAIT;                                   // ON duration == 0: signals done
            }
        }
        else                                                                    //-- indicator is OFF
        {
            data->flashStateOn = true;
            if (data->flashesCompleted < data->flashesRequested)
            {
                rgbi_setColor(data->rgbi_dev, &(data->pixels));                 // turn indicator ON
                k_timer_start(&(data->rgbiTimer), data->onDuration, K_NO_WAIT);
            }
        }
    }
}


/**
 * @brief Initialize RGB Indicator class
 * 
 * @param dev 
 * @return int 
 */
static int rgbi_spi_ws2812_init(const struct device *dev)
{
    struct rgbi_spi_ws2812_data *data = dev_data(dev);

    SET_RGB_PIXELS(data->pixels, 0, 0, 0);
    data->onDuration.ticks = 0;
    data->offDuration.ticks = 0;
    data->flashesRequested = 0;
    data->flashesCompleted = 0;
    data->flashStateOn = false;
    data->rgbi_dev = dev;                       // store addr of parent device for work queue handler, CONTAINER_OF can't find parent of pointer fields

    if (!device_is_ready(dev_config(dev)->led_strip))
    {
        LOG_INF("LED_Strip (SPI) device not ready");
		return -ENODEV;
    }

    k_timer_init(&data->rgbiTimer, rgbi_spi_ws2812_on_timer_expire, NULL);
    k_timer_user_data_set(&data->rgbiTimer, (void *)dev);
    k_work_init(&data->rgbiWork, rgbi_spi_ws2812_flashWorkHandler);

    return 0;
}


/* Devicetree Access MACROs
 ----------------------------------------------------------------------------*/
#define RGBI_TEST_VAL(idx)    (DT_INST_PROP(idx, test_val))
// #define WS2812_DEV_ID(idx)      STRINGIFY(DT_INST_PROP(idx, ws2812_dev_id))

// #define RGBI_SELF(idx)              DT_INST(idx, rgbi_spi_ws2812)
#define RGBI_PARENT(idx)            DT_PARENT(DT_INST(idx, rgbi_spi_ws2812))
// #define RGBI_PARENT_DEVICE(idx)     DEVICE_DT_GET(RGBI_PARENT(idx))


/* Instantiation Macro
 * -------------------------------------------------------------------- */
#define RGBI_SPI_WS2812_DEFINE(idx)                                                         \
                                                                                            \
    static const struct rgbi_spi_ws2812_config rgbi_spi_ws2812_##idx##_config = {           \
        .led_strip = DEVICE_DT_GET(DT_PARENT(DT_INST(idx, rgbi_spi_ws2812)))                \
    };                                                                                      \
                                                                                            \
    static struct rgbi_spi_ws2812_data rgbi_spi_ws2812_##idx##_data;                        \
                                                                                            \
    DEVICE_DT_INST_DEFINE(idx,                                                              \
                          rgbi_spi_ws2812_init,                                             \
                          NULL,                                                             \
                          &rgbi_spi_ws2812_##idx##_data,                                    \
                          &rgbi_spi_ws2812_##idx##_config,                                  \
                          POST_KERNEL,                                                      \
                          CONFIG_RGBI_INIT_PRIORITY,                                        \
                          &rgbi_spi_ws2812_api);

DT_INST_FOREACH_STATUS_OKAY(RGBI_SPI_WS2812_DEFINE)


/* ========================================================================================================================== */
/* ========================================================================================================================== */
/* ========================================================================================================================== */

