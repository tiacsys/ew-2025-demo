/*
 * Copyright (c) 2025 Navimatix GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include "zephyr/devicetree.h"
#include "zephyr/device.h"
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/stepper.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

#define SPEED0 1250000 /* 800 microsteps/s -> 100 full steps/s -> 0.5 revolutions/s*/
#define SPEED1 625000  /* 1600 microsteps/s -> 200 full steps/s -> 1.0 revolutions/s*/
#define SPEED2 416667  /* 2400 microsteps/s -> 150 full steps/s -> 1.5 revolutions/s*/

struct speed_selection_data {
	enum stepper_direction direction;
	uint64_t speed;
	const struct device *stepper;
	bool deceleration_flag;
};

struct k_event demo_event;

void stepper_callback(const struct device *dev, const enum stepper_event event, void *user_data)
{
	if (event == STEPPER_EVENT_STEPS_COMPLETED) {
		printk("Sending Event\n");
		k_event_set(&demo_event, 0x001);
	}
}

static void speed_selection(struct input_event *evt, void *user_data)
{
	struct speed_selection_data *selection_data = user_data;

	if (evt->code == INPUT_KEY_1 && evt->value == 1) {
		selection_data->speed = 0;
	}
	if (evt->code == INPUT_KEY_2 && evt->value == 1) {
		selection_data->speed = SPEED0;
	}
	if (evt->code == INPUT_KEY_3 && evt->value == 1) {
		selection_data->speed = SPEED1;
	}
	if (evt->code == INPUT_KEY_4 && evt->value == 1) {
		selection_data->speed = SPEED2;
	}

	if (!selection_data->deceleration_flag) {
		stepper_set_microstep_interval(selection_data->stepper, selection_data->speed);
		stepper_run(selection_data->stepper, selection_data->direction);
	}

	printk("input event: dev=%-16s %3s type=%2x code=%3d value=%d\n",
	       evt->dev ? evt->dev->name : "NULL", evt->sync ? "SYN" : "", evt->type, evt->code,
	       evt->value);
}

struct speed_selection_data data;
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(potentiometer_input)), speed_selection, &data);

int main(void)
{
	// int ret;

	k_event_init(&demo_event);

	data.speed = SPEED1;
	data.direction = STEPPER_DIRECTION_POSITIVE;
	data.stepper = DEVICE_DT_GET(DT_NODELABEL(stepper_motor));

	stepper_set_event_callback(data.stepper, stepper_callback, NULL);
	stepper_enable(data.stepper, true);
	stepper_set_micro_step_res(data.stepper, 8);
	stepper_set_microstep_interval(data.stepper, data.speed);

	stepper_run(data.stepper, data.direction);

	uint32_t events;
	data.deceleration_flag = false;

	events = k_event_wait(&demo_event, 0x001, true, K_SECONDS(5));

	while (true) {
		stepper_set_microstep_interval(data.stepper, data.speed);
		stepper_run(data.stepper, data.direction);
		data.deceleration_flag = false;
		k_msleep(6000);
		data.deceleration_flag = true;
		stepper_set_microstep_interval(data.stepper, 0);
		stepper_run(data.stepper, data.direction);
		if (data.speed != 0) {
			events = k_event_wait(&demo_event, 0x001, true, K_SECONDS(5));
			if (events != 0) {
				printk("Event Detected\n");
			} else {
				printk("No Event Detected\n");
			}
		}

		if (data.direction == STEPPER_DIRECTION_POSITIVE) {
			data.direction = STEPPER_DIRECTION_NEGATIVE;
		} else {
			data.direction = STEPPER_DIRECTION_POSITIVE;
		}
	}

	return 0;
}
