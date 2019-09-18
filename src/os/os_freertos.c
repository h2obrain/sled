#include <inttypes.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "../types.h"
#include "../timers.h"
#include "../main.h"
#include "task.h"
#include "semphr.h"
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>

#define STACK_DEPTH 16384
#define TASK_PRIORITY 10
#define USEC_CONST 1000 * 1000

#define NCPUS 1
#define USE_TASKS 0

static void sled_task(void* pvParameters) {
	if (sled_main(0, NULL)) {
		puts("fail\n");
	}
	puts("success\n");
}

// Main entry point.
// We can choose to pin to a core. This makes sense if your chip has more than 1 core.
// So you can pin SLED to one core and use the other cores for stuff like WiFi, Webserver, etc...
// Use core = -1 to not pin
// Set USE_TASKS to false to omit tasks
void user_main(void* pvParameters, int core) {
	if (!USE_TASKS) {
		sled_task(NULL);
		return;
	}

	if(xTaskGetSchedulerState() == 1)
		vTaskStartScheduler();
#if defined(INCLUDE_xTaskCreatePinnedToCore)
	if (core < 0 || core >= oscore_ncpus()) {
		xTaskCreate(sled_task, "sled_main", STACK_DEPTH, pvParameters, TASK_PRIORITY, NULL);
	} else {
		xTaskCreatePinnedToCore(sled_task, "sled_main", STACK_DEPTH, pvParameters, TASK_PRIORITY, NULL, core);
	}
#else
	xTaskCreate(sled_task, "sled_main", STACK_DEPTH, pvParameters, TASK_PRIORITY, NULL);
#endif
}

// -- event
// Return the current tasks handle.
//
// In this implementation, we use semaphores.
// Taken is the default state, as we can wait on getting the lock.
// Signalling is done by giving the lock, also known as unlocking.

oscore_event oscore_event_new(void) {
	return xSemaphoreCreateBinary(); // starts in taken state
}

int oscore_event_wait_until(oscore_event ev, oscore_time desired_usec) {
	// PRIu64 aka llu printing is not supported
	slogn(1000, "wait (%"PRIu32" > %"PRIu32") bg:0x%08"PRIx32"\n",
			(uint32_t)(desired_usec/1000),(uint32_t)(oscore_udate()/1000),
			(uint32_t)ev);
	
	// If desired_usec is 0, it is used to simply clear the event.
	if (desired_usec == 0) {
		xSemaphoreTake(ev, 0); // Only take when not taken.
		return 0;
	}

	if (desired_usec == 1) {
		slog("desired_usec is 1. wtf?\n");
//		oscore_task_yield();
	} else {
		slog("desired_usec is %"PRIu32".\n", (uint32_t)desired_usec);
	}

	oscore_time waketick = oscore_udate();
	if (waketick >= desired_usec) {
		slog("timer is late, waketick: %"PRIu32", desired: %"PRIu32"\n", (uint32_t)waketick, (uint32_t)desired_usec);
		return waketick==1?0:waketick;
	}
	oscore_time diff = desired_usec-waketick;
	slog("diff is %"PRIu32".\n", (uint32_t)diff);
	/*
	// make the minimum time to wait 5ms.
	// TODO: should be removed once we know what's going on.
	if (diff <= 5000) diff = 5000;
	*/

	uint32_t cnt=0;
	while (desired_usec > oscore_udate()) {
		cnt++;
		if (!(cnt%10000000)) {
			slogn(1000, "mdate %"PRIu32"ms\n", oscore_udate()/1000);
		}
	}
	/*
	TickType_t wait = pdMS_TO_TICKS(diff / 1000);
	if (wait && (xSemaphoreTake(ev, wait) == pdTRUE)) {
		slog("INTERRUPT!\n");
		return 1; // we got an interrupt
	}
	*/
	slog("Timeout.\n");
	return 0; // timeout
}

void oscore_event_signal(oscore_event ev) {
	slog("Signaling.\n");
	xSemaphoreGive(ev);
}

void oscore_event_free(oscore_event ev) {
	vSemaphoreDelete(ev);
}

// Time keeping.
// FreeRTOS provides a tick count
oscore_time oscore_udate(void) {
	return (((oscore_time)1000 * 1000 * 1000) / configTICK_RATE_HZ) * (oscore_time)xTaskGetTickCount();
}

// Below: Stubs and untestet stuff. Danger zone!

// -- mutex
oscore_mutex oscore_mutex_new(void) {
	SemaphoreHandle_t handle =  xSemaphoreCreateBinary();
	xSemaphoreGive(handle); // freertos semaphores start taken, "normal" *nix mutexes do not.
	return handle;
}

void oscore_mutex_lock(oscore_mutex m) {
	xSemaphoreTake(m, portMAX_DELAY);
}

void oscore_mutex_unlock(oscore_mutex m) {
	xSemaphoreGive(m);
}

void oscore_mutex_free(oscore_mutex m) {
	vSemaphoreDelete(m);
}

int oscore_ncpus(void) {
#ifdef ESP32
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	return chip_info.cores;
#endif
	return NCPUS;
}


oscore_task oscore_task_create(const char* name, oscore_task_function func, void* ctx) {
	TaskHandle_t xHandle = NULL;
	xTaskCreate((void *) func, name, STACK_DEPTH, ctx, TASK_PRIORITY, &xHandle);
	return xHandle;
}

void oscore_task_setprio(oscore_task task, int prio) {
#if INCLUDE_vTaskPrioritySet
	if (prio < 0) {
		prio = 0;
	} else if (prio > configMAX_PRIORITIES -1) {
		prio = configMAX_PRIORITIES -1;
	}
	vTaskPrioritySet(task, prio);
#endif
}

void oscore_task_yield(void) {
	taskYIELD();
};

void oscore_task_pin(oscore_task task, int cpu) {}

void* oscore_task_join(oscore_task task) {
	if (task != NULL) {
#if INCLUDE_vTaskDelete
		vTaskDelete(task);
#else
		slog("Dying.\n");
#endif
	}
	free(task);
	return 0;
};
