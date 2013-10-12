#include <string.h>

#include "fio.h"
#include "shell.h"

/* Command handlers. */
static void cmd_echo(int argc, char *argv[]);
static void cmd_export(int argc, char *argv[]);
static void cmd_help(int argc, char *argv[]);
static void cmd_history(int argc, char *argv[]);
static void cmd_man(int argc, char *argv[]);
static void cmd_ps(int argc, char *argv[]);

/* Structure for command handler. */
typedef struct {
	char cmd[MAX_CMDNAME + 1];
	void (*func)(int, char**);
	char description[MAX_CMDHELP + 1];
} hcmd_entry;
static const hcmd_entry cmd_data[CMD_COUNT] = {
	[CMD_ECHO] = {.cmd = "echo", .func = cmd_echo, .description = "Show words you input."},
	[CMD_EXPORT] = {.cmd = "export", .func = cmd_export, .description = "Export environment variables."},
	[CMD_HELP] = {.cmd = "help", .func = cmd_help, .description = "List all commands you can use."},
	[CMD_HISTORY] = {.cmd = "history", .func = cmd_history, .description = "Show latest commands entered."}, 
	[CMD_MAN] = {.cmd = "man", .func = cmd_man, .description = "Manual pager."},
	[CMD_PS] = {.cmd = "ps", .func = cmd_ps, .description = "List all the processes."}
};

/* Command history buffer. */
static char cmd[HISTORY_COUNT][CMDBUF_SIZE];
static int cur_his=0;

/* Environment variable buffer. */
static evar_entry env_var[MAX_ENVCOUNT];
static int env_count = 0;

/* See if c is alphabet or number. */
static inline int isalnum(char c)
{
	return (('a' <= (c) && (c) <= 'z') ||
		('A' <= (c) && (c) <= 'Z') ||
		('0' <= (c) && (c) <= '9'));
}

/* See if c is space. */
static inline int isspace(char c)
{
	return (c == ' ');
}

/* Implementation of the behavior of '!' in shell. */
static void find_events()
{
	char buf[CMDBUF_SIZE];
	char *p = cmd[cur_his];
	char *q;
	int i;

	for (; *p; p++) {
		if (*p == '!') {
			q = p;
			while (*q && !isspace(*q))
				q++;
			for (i = cur_his + HISTORY_COUNT - 1; i > cur_his; i--) {
				if (!strncmp(cmd[i % HISTORY_COUNT], p + 1, q - p - 1)) {
					strcpy(buf, q);
					strcpy(p, cmd[i % HISTORY_COUNT]);
					p += strlen(p);
					strcpy(p--, buf);
					break;
				}
			}
		}
	}
}

/* Split command into tokens. */
static char *cmdtok(char *cmd)
{
	static char *cur = NULL;
	static char *end = NULL;
	if (cmd) {
		char quo = '\0';
		cur = cmd;
		for (end = cmd; *end; end++) {
			if (*end == '\'' || *end == '\"') {
				if (quo == *end)
					quo = '\0';
				else if (quo == '\0')
					quo = *end;
				*end = '\0';
			}
			else if (isspace(*end) && !quo)
				*end = '\0';
		}
	}
	else
		for (; *cur; cur++)
			;

	for (; *cur == '\0'; cur++)
		if (cur == end) return NULL;
	return cur;
}

static char *find_envvar(const char *name)
{
	int i;

	for (i = 0; i < env_count; i++) {
		if (!strcmp(env_var[i].name, name))
			return env_var[i].value;
	}

	return NULL;
}

/* Fill in entire value of argument. */
static int fill_arg(char *const dest, const char *argv)
{
	char env_name[MAX_ENVNAME + 1];
	char *buf = dest;
	char *p = NULL;

	for (; *argv; argv++) {
		if (isalnum(*argv) || *argv == '_') {
			if (p)
				*p++ = *argv;
			else
				*buf++ = *argv;
		}
		else { /* Symbols. */
			if (p) {
				*p = '\0';
				p = find_envvar(env_name);
				if (p) {
					strcpy(buf, p);
					buf += strlen(p);
					p = NULL;
				}
			}
			if (*argv == '$')
				p = env_name;
			else
				*buf++ = *argv;
		}
	}
	if (p) {
		*p = '\0';
		p = find_envvar(env_name);
		if (p) {
			strcpy(buf, p);
			buf += strlen(p);
		}
	}
	*buf = '\0';

	return buf - dest;
}

/* Command parser. */
static void execute_command()
{
	char *argv[MAX_ARGC + 1] = {NULL};
	char cmdstr[CMDBUF_SIZE];
	char buffer[CMDBUF_SIZE * MAX_ENVVALUE / 2 + 1];
	char *p = buffer;
	int argc = 1;
	int i;

	find_events();
	strcpy(cmdstr, cmd[cur_his]);
	argv[0] = cmdtok(cmdstr);
	if (!argv[0])
		return;

	while (1) {
		argv[argc] = cmdtok(NULL);
		if (!argv[argc])
			break;
		argc++;
		if (argc >= MAX_ARGC)
			break;
	}

	for(i = 0; i < argc; i++) {
		int l = fill_arg(p, argv[i]);
		argv[i] = p;
		p += l + 1;
	}

	for (i = 0; i < CMD_COUNT; i++) {
		if (!strcmp(argv[0], cmd_data[i].cmd)) {
			cmd_data[i].func(argc, argv);
			break;
		}
	}
	if (i == CMD_COUNT) {
		fio_write(1, argv[0], strlen(argv[0]));
		fio_write(1, ": command not found\n", 20);
	}
}

/* Command "echo": It can accept a "-n" option. */
static void cmd_echo(int argc, char* argv[])
{
	const int _n = 1; /* Flag for "-n" option. */
	int flag = 0;
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-n"))
			flag |= _n;
		else
			break;
	}

	for (; i < argc; i++) {
		fio_write(1, argv[i], strlen(argv[i]));
		if (i < argc - 1)
			fio_write(1, " ", 1);
	}

	if (~flag & _n)
		fio_write(1, "\n", 1);
}

/* Command "export" */
void cmd_export(int argc, char *argv[])
{
	char *found;
	char *value;
	int i;

	for (i = 1; i < argc; i++) {
		value = argv[i];
		while (*value && *value != '=')
			value++;
		if (*value)
			*value++ = '\0';
		found = find_envvar(argv[i]);
		if (found)
			strcpy(found, value);
		else if (env_count < MAX_ENVCOUNT) {
			strcpy(env_var[env_count].name, argv[i]);
			strcpy(env_var[env_count].value, value);
			env_count++;
		}
	}
}

/* Command "help" */
static void cmd_help(int argc, char* argv[])
{
	int i;

	fio_write(1, "This system has commands as follow\n", 35);
	for (i = 0; i < CMD_COUNT; i++) {
		fio_write(1, cmd_data[i].cmd, strlen(cmd_data[i].cmd));
		fio_write(1, ": ", 2);
		fio_write(1, cmd_data[i].description, strlen(cmd_data[i].description));
		fio_write(1, "\n", 1);
	}
}

/* Command "history" */
void cmd_history(int argc, char *argv[])
{
	int i;

	for (i = cur_his + 1; i <= cur_his + HISTORY_COUNT; i++) {
		if (cmd[i % HISTORY_COUNT][0]) {
			fio_write(1, cmd[i % HISTORY_COUNT], strlen(cmd[i % HISTORY_COUNT]));
			fio_write(1, "\n", 1);
		}
	}
}

/* Command "man" */
void cmd_man(int argc, char *argv[])
{
	int i;

	if (argc < 2)
		return;

	for (i = 0; i < CMD_COUNT && strcmp(cmd_data[i].cmd, argv[1]); i++)
		;

	if (i >= CMD_COUNT)
		return;

	fio_write(1, "NAME: ", 6);
	fio_write(1, cmd_data[i].cmd, strlen(cmd_data[i].cmd));
	fio_write(1, "\n", 1);
	fio_write(1, "DESCRIPTION: ", 13);
	fio_write(1, cmd_data[i].description, strlen(cmd_data[i].description));
	fio_write(1, "\n", 1);
}

/* Command "ps" */
static void cmd_ps(int argc, char* argv[])
{
#if 0
	char ps_message[]="PID STATUS PRIORITY";
	int ps_message_length = sizeof(ps_message);
	int task_i;
	int task;

	write(fdout, &ps_message , ps_message_length);
	write(fdout, &next_line , 3);

	for (task_i = 0; task_i < task_count; task_i++) {
		char task_info_pid[2];
		char task_info_status[2];
		char task_info_priority[3];

		task_info_pid[0]='0'+tasks[task_i].pid;
		task_info_pid[1]='\0';
		task_info_status[0]='0'+tasks[task_i].status;
		task_info_status[1]='\0';		       

		itoa(tasks[task_i].priority,task_info_priority);

		write(fdout, &task_info_pid , 2);
		write_blank(3);
			write(fdout, &task_info_status , 2);
		write_blank(5);
		write(fdout, &task_info_priority , 3);

		write(fdout, &next_line , 3);
	}
#endif
}

void shell_task(void *pvParameters)
{
	char hint[] = "root@FreeRTOS:/# ";
	char *p = NULL;
	char c;

	for (;; cur_his = (cur_his + 1) % HISTORY_COUNT) {
		p = cmd[cur_his];
		fio_write(1, hint, strlen(hint));

		while (1) {
			fio_read(0, &c, 1);

			if (c == '\r' || c == '\n') {
				if (p > cmd[cur_his]) {
					*p = '\0';
					fio_write(1, "\n", 1);
					break;
				}
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
		execute_command();	
	}
}
