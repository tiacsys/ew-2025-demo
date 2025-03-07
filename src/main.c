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

/* Microstep resolution*/
#define MS_RESOLUTION 8

/* Defines speeds for the stepper motor, the api uses step interval timings in
 * nanoseconds, the microstep resolution is set to 8 for this demo and the motor needs 200 fulsteps
 * for one revolution. */
#define SPEED0 1250000 /* 800 microsteps/s -> 100 full steps/s -> 0.5 revolutions/s */
#define SPEED1 625000  /* 1600 microsteps/s -> 200 full steps/s -> 1.0 revolutions/s */
#define SPEED2 416667  /* 2400 microsteps/s -> 300 full steps/s -> 1.5 revolutions/s */

/* Number of microsteps/target posion for regular movement. */
#define SEGMENT_STEPS 6400

/* Defines watchdog timeout. */
#define WATCHDOG_TIMEOUT 5 * SEC_PER_MIN * MSEC_PER_SEC

/* Event IDs */
#define NEW_SPEED_EVENT_ID     0x001
#define MOVE_FINISHED_EVENT_ID 0x002
#define ALL_EVENT_ID           NEW_SPEED_EVENT_ID + MOVE_FINISHED_EVENT_ID

/* User data for the event callback. */
struct speed_selection_data {
	uint64_t speed;
	struct k_event event;
	int32_t watchdog_channel;
};

/* Watchdog callback to stop stepper motor if the speed hasn't changed for some time to save
 * power and reduce heat buildup. */
void watchdog_callback(int channel_id, void *user_data)
{
	struct speed_selection_data *selection_data = user_data;

	printk("Speed hasn't changed for some time, stopping motor.\n");

	selection_data->speed = 0;

	/* Posts the new_speed event after speed is set to 0, causing the motor to be stopped. */
	k_event_post(&selection_data->event, NEW_SPEED_EVENT_ID);
}

/* Stepper Callback, if stepper motor has stopped, send event. */
void stepper_callback(const struct device *dev, const enum stepper_event event, void *user_data)
{
	struct speed_selection_data *selection_data = user_data;

	k_event_post(&selection_data->event, MOVE_FINISHED_EVENT_ID);
}

/* Processes input events. */
static void speed_selection(struct input_event *evt, void *user_data)
{
	struct speed_selection_data *selection_data = user_data;

	/* Feeds watchdog, preventing stepper motor stop if speed is regularly changed. */
	task_wdt_feed(selection_data->watchdog_channel);

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

	/* Post new_speed event, as the speed has changed. */
	k_event_post(&selection_data->event, NEW_SPEED_EVENT_ID);

	/* Print the input event value to console. */
	if (evt->value == 1) {
		printk("Speed Selection Code: %u\n", evt->code);
	}
}

/* Initialize Input callback, user data needs to be defined at compile time. */
struct speed_selection_data data;
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_ALIAS(input_device)), speed_selection, &data);

int main(void)
{
	uint32_t events;
	int32_t err;

	/* Initialize data. */
	enum stepper_direction direction = STEPPER_DIRECTION_POSITIVE;
	const struct device *stepper = DEVICE_DT_GET(DT_ALIAS(stepper_motor));
	bool enabled = false;
	data.speed = 0;

	/* Initialize watchdog to cause the stepper motor to stop if the speed hasn't changed for
	 * some time. */
	err = task_wdt_init(NULL);
	data.watchdog_channel = task_wdt_add(WATCHDOG_TIMEOUT, watchdog_callback, &data);
	if (err < 0 || data.watchdog_channel < 0) {
		printk("Could not inizialize watchdog. Terminating programm.");
		return -ECANCELED;
	}

	/* Initial stepper configuration. */
	err = stepper_set_event_callback(stepper, stepper_callback, &data);
	if (err < 0) {
		printk("Could not set stepper event callback (error code %d). Terminating "
		       "programm.",
		       err);
		return -ECANCELED;
	}
	stepper_set_micro_step_res(stepper, MS_RESOLUTION);
	if (err < 0) {
		printk("Could not set microstep resolution (error code %d). Terminating "
		       "programm.",
		       err);
		return -ECANCELED;
	}

	/* Initialize event handling. */
	k_event_init(&data.event);

	/* We want to keep our motor turning, so this is an enless loop, but if we
	 * just want to perform a specific task, it would not be nessecary. */
	while (true) {
		events = k_event_wait(&data.event, ALL_EVENT_ID, false, K_FOREVER);

		/* If both events occur at the same time, we want to prioritize th new_speed event,
		 * as it makes the move_finished event irrelevant. */
		if ((events & NEW_SPEED_EVENT_ID) == NEW_SPEED_EVENT_ID) {
			k_event_clear(&data.event, ALL_EVENT_ID);
			/* If speed > 0, set motor into motion*/
			if (data.speed != 0) {
				/* Enable stepper, as it might have been disabled. */
				err = stepper_enable(stepper, true);
				if (err < 0) {
					printk("Could not enable stepper motor (error code %d). "
					       "Waiting for new event.",
					       err);
					continue;
				}
				enabled = true;
				/* Set stepper motor into motion. */
				err = stepper_set_microstep_interval(stepper, data.speed);
				if (err < 0) {
					printk("Could not set new speed (error code %d). "
					       "Waiting for new event.",
					       err);
					continue;
				}
				if (direction == STEPPER_DIRECTION_POSITIVE) {
					stepper_move_to(stepper, SEGMENT_STEPS);
				} else {
					stepper_move_to(stepper, 0);
				}
			}
			/* Otherwise stop motor by having it decelerate (the 0 speed is set by event
			   handling). */
			else {
				k_event_clear(&data.event, ALL_EVENT_ID);
				err = stepper_set_microstep_interval(stepper, 0);
				if (err < 0) {
					printk("Could not set speed to 0 (error code %d). "
					       "Waiting for new event.",
					       err);
					continue;
				}
				if (enabled) {
					stepper_run(stepper, direction);
				}
			}
		}
		/* If movement is finished either start moving in another direction or disable
		   motor. */
		else if (events == MOVE_FINISHED_EVENT_ID) {
			k_event_clear(&data.event, ALL_EVENT_ID);
			if (data.speed == 0) {
				printk("Disabling motor.\n");
				err = stepper_enable(stepper, false);
				if (err < 0) {
					printk("Could not disable motor (error code %d). "
					       "Waiting for new event.",
					       err);
					continue;
				}
				enabled = false;
			} else {
				/* Change direction. */
				if (direction == STEPPER_DIRECTION_POSITIVE) {
					direction = STEPPER_DIRECTION_NEGATIVE;
				} else {
					direction = STEPPER_DIRECTION_POSITIVE;
				}
				k_event_post(&data.event, NEW_SPEED_EVENT_ID);
			}
		}
	}
	return 0;
}
