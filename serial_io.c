#include "serial_io.h"
#include "stm32f10x.h"
#include "stm32_p103.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

static volatile xSemaphoreHandle serial_tx_wait_sem = NULL;

/* IRQ handler to handle USART2 interruptss (both transmit and receive
 * interrupts).
 */
void USART2_IRQHandler()
{
	static signed portBASE_TYPE xHigherPriorityTaskWoken;

	/* If this interrupt is for a transmit... */
	if (USART_GetITStatus(USART2, USART_IT_TXE) != RESET) {
		/* "give" the serial_tx_wait_sem semaphore to notfiy processes
		 * that the buffer has a spot free for the next byte.
		 */
		xSemaphoreGiveFromISR(serial_tx_wait_sem, &xHigherPriorityTaskWoken);

		/* Disables the transmit interrupt. */
		USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
		/* If this interrupt is for a receive... */
	}
	else {
		/* Only transmit and receive interrupts should be enabled.
		 * If this is another type of interrupt, freeze.
		 */
		while(1);
	}

	if (xHigherPriorityTaskWoken) {
		taskYIELD();
	}
}

void init_rs232_io()
{
	init_rs232();
	enable_rs232_interrupts();
	enable_rs232();

	/* Create the queue used by the serial task.  Messages for write to
	 * the RS232.
	 */
	vSemaphoreCreateBinary(serial_tx_wait_sem);
}

void send_byte(char ch)
{
	/* Wait until the RS232 port can receive another byte (this semaphore
	 * is "given" by the RS232 port interrupt when the buffer has room for
	 * another byte.
	 */
	while (!xSemaphoreTake(serial_tx_wait_sem, portMAX_DELAY));

	/* Send the byte and enable the transmit interrupt (it is disabled by
	 * the interrupt).
	 */
	USART_SendData(USART2, ch);
	USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
}
