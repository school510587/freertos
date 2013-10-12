#define USE_STDPERIPH_DRIVER
#include "stm32f10x.h"
#include "rs232_io.h"

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

#if 0
void read_romfs_task(void *pvParameters)
{
	char buf[128];
	size_t count;
	int fd = fs_open("/romfs/test.txt", 0, O_RDONLY);
	do {
		//Read from /romfs/test.txt to buffer
		count = fio_read(fd, buf, sizeof(buf));

		//Write buffer to fd 1 (stdout, through uart)
		fio_write(1, buf, count);
	} while (count);

	while (1);
}
#endif

int main()
{
	init_rs232_io();

	fs_init();
	fio_init();

	register_romfs("romfs", &_sromfs);

	/* Create a task to output text read from romfs. */
	xTaskCreate(shell_task,
	            (signed portCHAR *) "Read romfs",
	            512 /* stack size */, NULL, tskIDLE_PRIORITY + 2, NULL);

	/* Start running the tasks. */
	vTaskStartScheduler();

	return 0;
}

void vApplicationTickHook()
{
}
