/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define SLEEP_TIME_MS  500

#define TPIN_NODE DT_ALIAS(tpin0)

static const struct gpio_dt_spec tpin = GPIO_DT_SPEC_GET(TPIN_NODE, gpios);



int main(void)
{
    int ret;

    if (!gpio_is_ready_dt(&tpin)) 
    {
        return 0;
    }

    ret = gpio_pin_configure_dt(&tpin, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) 
    {
        return 0;
    }

    while (1)
    {
    	printf("Hello %s welcome to our world!\n", CONFIG_BOARD_TARGET);

        ret = gpio_pin_toggle_dt(&tpin);
        if (ret < 0) 
        {
            return 0;
        }

        k_msleep(1000);
    }

	return 0;
}
