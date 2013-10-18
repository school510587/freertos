#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#define isalnum(c) __builtin_isalnum(c)
#define isspace(c) __builtin_isspace(c)

#include "FreeRTOS.h"
#include "task.h"

#include "fattr.h"
#include "fio.h"
#include "filesystem.h"
#include "shell.h"

/* Command handlers. */
static void cmd_cat(int argc, char *argv[]);
static void cmd_echo(int argc, char *argv[]);
static void cmd_export(int argc, char *argv[]);
static void cmd_help(int argc, char *argv[]);
static void cmd_history(int argc, char *argv[]);
static void cmd_ls(int argc, char *argv[]);
static void cmd_man(int argc, char *argv[]);
static void cmd_ps(int argc, char *argv[]);
static void cmd_su(int argc, char *argv[]);

/* Structure for command handler. */
typedef struct {
	char cmd[MAX_CMDNAME + 1];
	void (*func)(int, char**);
	char description[MAX_CMDHELP + 1];
} hcmd_entry;
#define CMD_DEF(name, desc) \
	[CMD_ ## name] = {.cmd = #name, .func = cmd_ ## name, .description = desc "."}
static const hcmd_entry cmd_data[CMD_COUNT] = {
	CMD_DEF(cat, "Concatenate & print files"),
	CMD_DEF(echo, "Show words you input"),
	CMD_DEF(export, "Export environment variables"),
	CMD_DEF(help, "List all commands you can use"),
	CMD_DEF(history, "Show latest commands entered"),
	CMD_DEF(ls, "List files (& attributes)"),
	CMD_DEF(man, "Manual pager"),
	CMD_DEF(ps, "List all the processes"),
	CMD_DEF(su, "Switch user")
};

/* Command history buffer. */
static char cmd[HISTORY_COUNT][CMDBUF_SIZE];
static int cur_his=0;

/* Environment variable buffer. */
static evar_entry env_var[MAX_ENVCOUNT];
static int env_count = 0;

/* Current working directory. */
static char cwd[CWD_LEN] = "/romfs";

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

char *getenv(const char *name)
{
	int i;

	for (i = 0; i < env_count; i++) {
		if (!strcmp(env_var[i].name, name))
			return env_var[i].value;
	}

	return NULL;
}

static int putenv_internal(char *str)
{
	char *found;
	char *value = str;

	while (*value && *value != '=')
		value++;

	if (*value)
		*value++ = '\0';

	found = getenv(str);
	if (found)
		strcpy(found, value);
	else if (env_count < MAX_ENVCOUNT) {
		strcpy(env_var[env_count].name, str);
		strcpy(env_var[env_count].value, value);
		env_count++;
	}
	else
		return ENOMEM;

	return EXIT_SUCCESS;
}

int putenv(char *str)
{
	if (!str || !strncmp(str, "USER=", 5))
		return EPERM;
	return putenv_internal(str);
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
				p = getenv(env_name);
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
		p = getenv(env_name);
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
		puts(argv[0]);
		fio_write(2, ": command not found\n", 20);
	}
}

/* Command "cat" */
static void cmd_cat(int argc, char *argv[])
{
	char buf[128];
	size_t count;
	int fd;
	int i;

	for (i = 1; i < argc; i++) {
		sprintf(buf, "%s/%s", cwd, argv[i]);
		fd = fs_open(buf, 0, O_RDONLY);

		if (fd < 0) {
			fio_write(2, "cat: ", 5);
			fio_perror(argv[i]);
			return;
		}
		else {
			do {
				/* Read from /romfs/test.txt to buffer */
				count = fio_read(fd, buf, sizeof(buf));

				/* Write buffer to fd 1 (stdout, through UART) */
				fio_write(1, buf, count);
			} while (count);
		}

		fio_close(fd);
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
		puts(argv[i]);
		if (i < argc - 1)
			puts(" ");
	}

	if (~flag & _n)
		puts("\n");
}

/* Command "export" */
void cmd_export(int argc, char *argv[])
{
	int i;

	for (i = 1; i < argc; i++)
		putenv(argv[i]);
}

/* Command "help" */
static void cmd_help(int argc, char* argv[])
{
	int i;

	puts("This system has commands as follow\n");
	for (i = 0; i < CMD_COUNT; i++)
		printf("%s: %s\n", cmd_data[i].cmd, cmd_data[i].description);
}

/* Command "history" */
void cmd_history(int argc, char *argv[])
{
	int i;

	for (i = cur_his + 1; i <= cur_his + HISTORY_COUNT; i++) {
		if (cmd[i % HISTORY_COUNT][0])
			printf("%s\n", cmd[i % HISTORY_COUNT]);
	}
}

/* Show 3 characters representing reading, writing and excuting. */
static inline void show_access_rights(mode_t m, mode_t r, mode_t w, mode_t x)
{
	puts(m & r ? "r" : "-");
	puts(m & w ? "w" : "-");
	puts(m & x ? "x" : "-");
}

/* Command "ls" */
void cmd_ls(int argc, char *argv[])
{
	const int _a = 2; /* Flag for "-a" option. */
	const int _l = 1; /* Flag for "-l" option. */
	int flag = 0;
	file_attr_t entry[8];
	size_t n;
	size_t i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-l"))
			flag |= _l;
		else if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--all"))
			flag |= _a;
		else
			break;
	}

	n = fio_list(cwd, entry, 8);
	for (i = 0; i < n; i++) {
		if (entry[i].name[0] == '.' && !(flag & _a))
			continue;

		if (flag & _l) {
			puts(S_ISDIR(entry[i].mode) ? "d" : "-");
			show_access_rights(entry[i].mode, S_IRUSR, S_IWUSR, S_IXUSR);
			show_access_rights(entry[i].mode, S_IRGRP, S_IWGRP, S_IXGRP);
			show_access_rights(entry[i].mode, S_IROTH, S_IWOTH, S_IXOTH);
			printf(" %s %u\n", entry[i].name, entry[i].size);
		}
		else
			printf("%s ", entry[i].name);
	}
	puts("\n");
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

	puts("NAME: ");
	puts(cmd_data[i].cmd);
	puts("\n");
	puts("DESCRIPTION: ");
	puts(cmd_data[i].description);
	puts("\n");
}

/* Command "ps" */
static void cmd_ps(int argc, char* argv[])
{
	signed char buf[1024];

	vTaskList(buf);
	puts((char*)buf);
}

/* Command "su" */
static void cmd_su(int argc, char *argv[])
{
	/* Buffer includes "USER=" & account with an ending '\0'. */
	char buf[5 + MAX_ENVVALUE + 1];

	if (argc == 1 || !strcmp(argv[1], "-")) {
		strcpy(buf, "USER=root");
		putenv_internal(buf);
	}
	else {
		int ac_config;
		char *line;
		char user[MAX_ENVVALUE + 1];

		strcpy(user, getenv("USER"));
		if (!strcmp(user, argv[1]))
			return;

		/* Note: Only root has access rights to this file. */
		strcpy(buf, "USER=root");
		putenv_internal(buf);

		ac_config = fs_open("/romfs/.account_config", 0, O_RDONLY);
		strcpy(buf, "USER=");
		while (fio_getline(ac_config, buf + 5, MAX_ENVVALUE + 1)) {
			if (!strcmp(argv[1], buf + 5)) {
				putenv_internal(buf);
				user[0] = '\0';
				break;
			}
		}
		fio_close(ac_config);

		/* It is nonempty when no matching id is found. */
		if (user[0]) {
			strcpy(buf + 5, user);
			putenv_internal(buf);
			fio_write(2, "Unknown id: ", 12);
			fio_write(2, argv[1], strlen(argv[1]));
			fio_write(2, "\n", 1);
		}
	}
}

void shell_task(void *pvParameters)
{
	char line[CMDBUF_SIZE + 1];
	char *user = NULL;
	char *p = NULL;
	char c;

	sprintf(line, "USER=%s", "root");
	putenv_internal(line);
	user = getenv("USER");
	for (;; cur_his = (cur_his + 1) % HISTORY_COUNT) {
		p = cmd[cur_his];
		printf("%s@FreeRTOS:%s# ", user, cwd);

		while (1) {
			fio_read(0, &c, 1);

			if (c == '\r' || c == '\n') {
				if (p > cmd[cur_his]) {
					*p = '\0';
					puts("\n");
					break;
				}
			}
			else if (c == 127 || c == '\b') {
				if (p > cmd[cur_his]) {
					p--;
					puts("\b \b");
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
