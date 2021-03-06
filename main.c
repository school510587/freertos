#define USE_STDPERIPH_DRIVER
#include "stm32f10x.h"
#include "serial_io.h"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* Filesystem includes */
#include "filesystem.h"
#include "fio.h"
#include "romfs.h"

/* Shell includes */
#include "shell.h"

extern const uint8_t _sromfs;

int main()
{
	init_serial_io();

	fs_init();
	fio_init();

	register_romfs("romfs", &_sromfs);

	/* Create a task to output text read from romfs. */
	xTaskCreate(shell_task,
	            (const signed portCHAR *) "Shell",
	            1024 /* stack size */, NULL, tskIDLE_PRIORITY + 2, NULL);

	/* Start running the tasks. */
	vTaskStartScheduler();

	return 0;
}

void vApplicationTickHook()
{
}
