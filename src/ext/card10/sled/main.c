#include "epicardium.h"

#include <math.h>

#include "FreeRTOS.h"
#include "task.h"
#include "api/dispatcher.h"
#include "modules/log.h"


void user_main(void* pvParameters, int core);

extern TaskHandle_t dispatcher_task_id;

int main(void) {
	user_main(NULL, -1);
}

#include <stdio.h>
void pre_idle_sleep(TickType_t xExpectedIdleTime)
{
	printf("Pre idle sleep (%d)!\n", (int)xExpectedIdleTime); fflush(stdout);
	if (xExpectedIdleTime > 0) {
		/*
		 * WFE because the other core should be able to notify
		 * epicardium if it wants to issue an API call.
		 */

		/*
		 * TODO: Ensure this is actually correct and does not have any
		 * race conditions.
		 */
		if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) == 0) {
			__asm volatile("dsb" ::: "memory");
			__asm volatile("wfe");
			__asm volatile("isb");
		}
	}
}

/*
 * This hook is called after FreeRTOS exits tickless idle.
 */
void post_idle_sleep(TickType_t xExpectedIdleTime)
{
	printf("Pre idle sleep (%d)!\n", (int)xExpectedIdleTime); fflush(stdout);
	/* Check whether a new API call was issued. */
	if (api_dispatcher_poll_once()) {
		xTaskNotifyGive(dispatcher_task_id);
	}

	/*
	 * Do card10 house keeping. e.g. polling the i2c devices if they
	 * triggered an interrupt.
	 *
	 * TODO: Do this in a more task focused way (high/low ISR)
	 */
	//card10_poll();
}


void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName)
{
	LOG_CRIT("rtos", "Task \"%s\" overflowed stack!", pcTaskName);
}

#if LOG_ENABLE
#include <stdio.h>
#include <stdarg.h>
int log_msg(const char *subsys, const char *format, ...)
{
	va_list ap;
	int ret;

	if (!LOG_ENABLE_DEBUG && format[0] == 'D') {
		return 0;
	}

	va_start(ap, format);
	if (LOG_ENABLE_COLOR) {
		char *msg_color = "";

		switch (format[0]) {
		case 'D':
			msg_color = "\x1B[2m";
			break;
		case 'W':
			msg_color = "\x1B[1m";
			break;
		case 'E':
			msg_color = "\x1B[31m";
			break;
		case 'C':
			msg_color = "\x1B[37;41;1m";
			break;
		}

		printf("\x1B[32m[%12lu] \x1B[33m%s\x1B[0m: %s",
		       xTaskGetTickCount(),
		       subsys,
		       msg_color);

		ret = vprintf(&format[1], ap);

		printf("\x1B[0m\n");
	} else {
		printf("[%12lu] %s: ", xTaskGetTickCount(), subsys);
		ret = vprintf(&format[1], ap);
		printf("\n");
	}
	va_end(ap);

	return ret;
}
#endif /* LOG_ENABLE */
