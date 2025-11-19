/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * Copyright (c) 2025 LooUQ Incorporated
 * 
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_DRIVERS_RGBI_H_
#define APP_DRIVERS_RGBI_H_

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/toolchain.h>
#include <zephyr/sys_clock.h>
#include <zephyr/drivers/led_strip.h>

typedef uint8_t rgbi_color_t;

#define CREATE_RGB_PIXELS(_p,_r,_g,_b)  struct led_rgb _p = { .r = _r, .g = _g, .b = _b };
#define SET_RGB_PIXELS(_p,_r,_g,_b)     _p.r = _r; _p.g = _g; _p.b = _b;

/* Planned API
void rgbi_setColor(rgb_indicator_t * indicator, const struct led_rgb * pixels);                                 // set color and display
void rgbi_setColorFromPixels(rgb_indicator_t * indicator, rgbi_color_t red, rgbi_color_t green, rgbi_color_t blue);   // set color and display
void rgbi_off(rgb_indicator_t * indicator);                                                                     // set all channels to 0 = OFF
void rgbi_flash(rgb_indicator_t * indicator, struct led_rgb *pixels, k_timeout_t onDuration, k_timeout_t offDuration, uint8_t count);
void rgbi_flashContinuous(rgb_indicator_t * indicator, struct led_rgb *pixels, k_timeout_t onDuration, k_timeout_t offDuration);
void rgbi_cancel(rgb_indicator_t *indicator);
bool rgbi_isBusy(rgb_indicator_t * indicator);
*/


__subsystem struct rgbi_driver_api {

    /**
     * @brief Set color and display using RGB struct.
     * 
     * @param indicator 
     * @param pixels 
     * @return int 
     */
    // int rgbi_setColor(rgb_indicator_t * indicator, const struct led_rgb * pixels);
    int (*setColor)(const struct device *indicator, const struct led_rgb * pixels);

    /**
     * @brief Set color and display using individual color pixels.
     * 
     * @param indicator 
     * @param red 
     * @param green 
     * @param blue 
     * @return int 
     */
    int (*setColorFromPixels)(const struct device *indicator, rgbi_color_t red, rgbi_color_t green, rgbi_color_t blue);   // set color and display

    /**
     * @brief Turn the indicator off, skips and returns if the indicator is performing a flash sequence.
     * 
     * @param indicator 
     * @return int 
     */
    int (*off)(const struct device *indicator);                                                                     // set all channels to 0 = OFF

    /**
     * @brief Start a flash sequence on the indicator.
     * 
     * @param indicator 
     * @param pixels 
     * @param onDuration 
     * @param offDuration 
     * @param count 
     * @return int 
     */
    int (*flash)(const struct device *indicator, const struct led_rgb * pixels, k_timeout_t onDuration, k_timeout_t offDuration, uint8_t count);

    /**
     * @brief Start a continuous (no end scheduled) flash sequence on the indicator, flash goes until a rgbi_cancel function is invoked.
     * 
     * @param indicator 
     * @param pixels 
     * @param onDuration 
     * @param offDuration 
     * @return int 
     */
    int (*flashContinuous)(const struct device *indicator, const struct led_rgb *pixels, k_timeout_t onDuration, k_timeout_t offDuration);

    /**
     * @brief Cancel a flash sequence (either continuous or count constrained).
     * 
     * @param indicator 
     * @return int int rgbi_
     */
    int (*cancel)(const struct device *indicator);

    /**
     * @brief Get (flash) busy status of the indicator
     * 
     */
    bool (*isBusy)(const struct device *indicator);
};


__syscall int rgbi_setColor(const struct device *indicator, const struct led_rgb *pixels);
__syscall int rgbi_setColorFromPixels(const struct device *indicator, rgbi_color_t red, rgbi_color_t green, rgbi_color_t blue);
__syscall int rgbi_off(const struct device *indicator);
__syscall int rgbi_flash(const struct device *indicator, struct led_rgb *pixels, k_timeout_t onDuration, k_timeout_t offDuration, uint8_t count);
__syscall int rgbi_flashContinuous(const struct device *indicator, struct led_rgb *pixels, k_timeout_t onDuration, k_timeout_t offDuration);
__syscall int rgbi_cancel(const struct device *indicator);
__syscall bool rgbi_isBusy(const struct device *indicator);


static inline int z_impl_rgbi_setColor(const struct device *indicator, const struct led_rgb *pixels)
{
	__ASSERT_NO_MSG(DEVICE_API_IS(rgbi, indicator));
	return DEVICE_API_GET(rgbi, indicator)->setColor(indicator, pixels);
}

static inline int z_impl_rgbi_setColorFromPixels(const struct device *indicator, rgbi_color_t red, rgbi_color_t green, rgbi_color_t blue)
{
	__ASSERT_NO_MSG(DEVICE_API_IS(rgbi, indicator));
	return DEVICE_API_GET(rgbi, indicator)->setColorFromPixels(indicator,red, green, blue);
}


static inline int z_impl_rgbi_off(const struct device *indicator)
{
	__ASSERT_NO_MSG(DEVICE_API_IS(rgbi, indicator));
	return DEVICE_API_GET(rgbi, indicator)->off(indicator);
}


static inline int z_impl_rgbi_flash(const struct device *indicator, struct led_rgb *pixels, k_timeout_t onDuration, k_timeout_t offDuration, uint8_t count)
{
	__ASSERT_NO_MSG(DEVICE_API_IS(rgbi, indicator));
	return DEVICE_API_GET(rgbi, indicator)->flash(indicator, pixels, onDuration, offDuration, count);
}

static inline int z_impl_rgbi_flashContinuous(const struct device *indicator, struct led_rgb *pixels, k_timeout_t onDuration, k_timeout_t offDuration)
{
	__ASSERT_NO_MSG(DEVICE_API_IS(rgbi, indicator));
	return DEVICE_API_GET(rgbi, indicator)->flashContinuous(indicator, pixels, onDuration, offDuration);
}

static inline int z_impl_rgbi_cancel(const struct device *indicator)
{
	__ASSERT_NO_MSG(DEVICE_API_IS(rgbi, indicator));
	return DEVICE_API_GET(rgbi, indicator)->cancel(indicator);
}


static inline bool z_impl_rgbi_isBusy(const struct device *indicator)
{
	__ASSERT_NO_MSG(DEVICE_API_IS(rgbi, indicator));
	return DEVICE_API_GET(rgbi, indicator)->isBusy(indicator);
}

// static inline bool rgbi_isBusy(const struct device *dev)
// {
//     return indicator->onDuration.ticks > 0;
// }


/* STEP 2.5 Add the syscall header at the end of the header file */
#include <syscalls/rgbi.h>


#endif // APP_DRIVERS_RGBI_H_ 
