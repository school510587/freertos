#ifndef SHELL_H
#define SHELL_H

#define MAX_CMDNAME 19
#define MAX_ARGC 19
#define MAX_CMDHELP 32
#define HISTORY_COUNT 8
#define CMDBUF_SIZE 40
#define MAX_ENVCOUNT 8
#define MAX_ENVNAME 15
#define MAX_ENVVALUE 15
#define CWD_LEN 16
#define MAX_ACCOUNT_LEN 10

/* Enumeration for command types. */
typedef enum {
	CMD_cat = 0,
	CMD_echo,
	CMD_export,
	CMD_help,
	CMD_history,
	CMD_ls,
	CMD_man,
	CMD_ps,
	CMD_su,
	CMD_COUNT
} CMD_TYPE;

/* Structure for environment variables. */
typedef struct {
	char name[MAX_ENVNAME + 1];
	char value[MAX_ENVVALUE + 1];
} evar_entry;

void shell_task(void *pvParameters);

#endif
