#include <string.h>

#include "fio.h"
#include "shell.h"

/* Command handlers. */
void cmd_echo(int argc, char *argv[]);
void cmd_export(int argc, char *argv[]);
void cmd_help(int argc, char *argv[]);
void cmd_history(int argc, char *argv[]);
void cmd_man(int argc, char *argv[]);
void cmd_ps(int argc, char *argv[]);

/* Structure for command handler. */
typedef struct {
	char cmd[MAX_CMDNAME + 1];
	void (*func)(int, char**);
	char description[MAX_CMDHELP + 1];
} hcmd_entry;
#if 0
static const hcmd_entry cmd_data[CMD_COUNT] = {
	[CMD_ECHO] = {.cmd = "echo", .func = cmd_echo, .description = "Show words you input."},
	[CMD_EXPORT] = {.cmd = "export", .func = cmd_export, .description = "Export environment variables."},
	[CMD_HELP] = {.cmd = "help", .func = cmd_help, .description = "List all commands you can use."},
	[CMD_HISTORY] = {.cmd = "history", .func = cmd_history, .description = "Show latest commands entered."}, 
	[CMD_MAN] = {.cmd = "man", .func = cmd_man, .description = "Manual pager."},
	[CMD_PS] = {.cmd = "ps", .func = cmd_ps, .description = "List all the processes."}
};
#endif

/* Command history buffer. */
static char cmd[HISTORY_COUNT][CMDBUF_SIZE];
static int cur_his=0;

/* Environment variable buffer. */
static evar_entry env_var[MAX_ENVCOUNT];
static int env_count = 0;

void shell_task(void *pvParameters)
{
	char hint[] = "root@FreeRTOS:~# ";
	char *p = NULL;
	char c;

	for (;; cur_his = (cur_his + 1) % HISTORY_COUNT) {
		p = cmd[cur_his];
		fio_write(1, hint, strlen(hint));

		while (1) {
			fio_read(0, &c, 1);

			if (c == '\r' || c == '\n') {
				*p = '\0';
				fio_write(1, "\n", 1);
				break;
			}
			else if (c == 127 || c == '\b') {
				if (p > cmd[cur_his]) {
					p--;
					fio_write(1, "\b \b", 3);
				}
			}
			else if (p - cmd[cur_his] < CMDBUF_SIZE - 1) {
				*p++ = c;
				fio_write(1, &c, 1);
			}
		}
		//check_keyword();	
	}
}
