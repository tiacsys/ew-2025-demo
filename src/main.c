/*
 * Copyright (c) 2025 Navimatix GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zephyr/device.h"
#include "zephyr/devicetree.h"
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/stepper.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/init.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/task_wdt/task_wdt.h>

/* Defines speeds for the stepper motor, the api uses step interval timings in
 * nanoseconds, and the microstep resolution is set to 8 for this demo */
#define SPEED0 1250000 /* 800 microsteps/s -> 100 full steps/s -> 0.5 revolutions/s */
#define SPEED1 625000  /* 1600 microsteps/s -> 200 full steps/s -> 1.0 revolutions/s */
#define SPEED2 416667  /* 2400 microsteps/s -> 300 full steps/s -> 1.5 revolutions/s */

/* Event IDs, as we use two different event structs, the first two ids can be
 * the same. */
#define DEMO_EVENT_ID  0x001
#define SPEED_EVENT_ID 0x001
#define SLEEP_EVENT_ID 0x002

#define SEGMENT_STEPS 6400

/* Defines watchdog timeout. */
#define WATCHDOG_TIMEOUT 5 * SEC_PER_MIN *MSEC_PER_SEC

/* User data for the input event callback*/
struct speed_selection_data {
	enum stepper_direction direction;
	uint64_t speed;
	const struct device *stepper;
	bool deceleration_flag;
	struct k_event speed_event;
	int32_t watchdog_channel;
	bool enabled;
};

/* Watchdog callback to disable stepper motor if the speed hasn't changed for some time to save
 * power and reduce heat buildup.*/
void watchdog_callback(int channel_id, void *user_data)
{
	struct speed_selection_data *selection_data = user_data;

	printk("Speed hasn't changed for some time, disabling motor.\n");

	/* Decelerate stepper motor until it stops. */
	selection_data->speed = 0;
	stepper_set_microstep_interval(selection_data->stepper, selection_data->speed);
	stepper_run(selection_data->stepper, selection_data->direction);

	/* Clears speed event, as the motor is stopping and posts the sleep event instead. */
	k_event_clear(&selection_data->speed_event, SPEED_EVENT_ID);
	k_event_post(&selection_data->speed_event, SLEEP_EVENT_ID);
}

/* Stepper Callback, if stepper motor has stopped, send event */
void stepper_callback(const struct device *dev, const enum stepper_event event, void *user_data)
{
	struct k_event *demo_event = user_data;

	if (event == STEPPER_EVENT_STEPS_COMPLETED) {
		k_event_set(demo_event, DEMO_EVENT_ID);
	}
}

/* Processes input events*/
static void speed_selection(struct input_event *evt, void *user_data)
{
	struct speed_selection_data *selection_data = user_data;

	/* Feeds watchdog, preventing stepper motor stop if speed is regularly changed. */
	task_wdt_feed(selection_data->watchdog_channel);

	if (evt->code == INPUT_KEY_1 && evt->value == 1) {
		selection_data->speed = 0;
		/* Clears speed event, as the motor is stopping and posts the sleep event instead.
		 */
		k_event_clear(&selection_data->speed_event, SPEED_EVENT_ID);
		k_event_post(&selection_data->speed_event, SLEEP_EVENT_ID);
	}
	if (evt->code == INPUT_KEY_2 && evt->value == 1) {
		selection_data->speed = SPEED0;
		/* Posts speed event, as the stepper motor is moving. */
		k_event_post(&selection_data->speed_event, SPEED_EVENT_ID);
	}
	if (evt->code == INPUT_KEY_3 && evt->value == 1) {
		selection_data->speed = SPEED1;
		k_event_post(&selection_data->speed_event, SPEED_EVENT_ID);
	}
	if (evt->code == INPUT_KEY_4 && evt->value == 1) {
		selection_data->speed = SPEED2;
		k_event_post(&selection_data->speed_event, SPEED_EVENT_ID);
	}
	stepper_set_microstep_interval(selection_data->stepper, selection_data->speed);

	/* Set motor into motion at new speed or cause it to stop. */
	if (selection_data->speed != 0) {
		if (selection_data->direction == STEPPER_DIRECTION_POSITIVE) {
			stepper_move_to(selection_data->stepper, SEGMENT_STEPS);
		} else {
			stepper_move_to(selection_data->stepper, 0);
		}
	} else if (selection_data->enabled) {
		stepper_run(selection_data->stepper, selection_data->direction);
	}
	/* Print the input event value to console. */
	if (evt->value == 1) {
		printk("Speed Selection Code: %u\n", evt->code);
	}
}

/* Initialize Input callback, user data needs to be defined at compile time */
struct speed_selection_data data;
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_ALIAS(input_device)), speed_selection, &data);

int main(void)
{
	uint32_t events;
	struct k_event demo_event;

	/* Initialize various data and events */
	data.speed = 0;
	data.direction = STEPPER_DIRECTION_POSITIVE;
	data.stepper = DEVICE_DT_GET(DT_ALIAS(stepper_motor));
	data.deceleration_flag = false;
	k_event_init(&data.speed_event);
	k_event_init(&demo_event);

	/* Initialize watchdog to cause the stepper motor to stop if the speed hasn't changed for
	 * some time. */
	task_wdt_init(NULL);
	task_wdt_add(WATCHDOG_TIMEOUT, watchdog_callback, &data);

	/* Initial stepper configuration */
	stepper_set_event_callback(data.stepper, stepper_callback, &demo_event);
	stepper_set_micro_step_res(data.stepper, 8);

	/* We want to keep our motor turning, so this is an enless loop, but if we
	 * just want to perform a specific task, it would not be nessecary. */
	while (true) {

		/* Wait for a stepper motor speed > 0 */
		(void)k_event_wait(&data.speed_event, SPEED_EVENT_ID, false, K_FOREVER);
		/* Enable stepper, as it might have been disabled */
		stepper_enable(data.stepper, true);
		data.enabled = true;
		/* Set stepper motor into motion. */
		stepper_set_microstep_interval(data.stepper, data.speed);
		if (data.direction == STEPPER_DIRECTION_POSITIVE) {
			stepper_move_to(data.stepper, SEGMENT_STEPS);
		} else {
			stepper_move_to(data.stepper, 0);
		}

		/* We want to wait for either 15 seconds (timeout), until the move_to command is
		 * finished or until a speed of 0 (stopping the motor) has been selected. */
		events = k_event_wait(&demo_event, DEMO_EVENT_ID, true, K_SECONDS(15));

		/* Change direction */
		if (data.direction == STEPPER_DIRECTION_POSITIVE) {
			data.direction = STEPPER_DIRECTION_NEGATIVE;
		} else {
			data.direction = STEPPER_DIRECTION_POSITIVE;
		}
		/* Disable stepper motor if speed = 0. */
		if (data.speed == 0) {
			stepper_enable(data.stepper, false);
			data.enabled = false;
		}
	}

	return 0;
}
