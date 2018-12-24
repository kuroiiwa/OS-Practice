// SPDX-License-Identifier: Columbia
#include "shell.h"


char *readLine(void);
command *parseLine(char *line);
static int isSpaceStr(char *str);
static int executeCtrl(command *cmd);
static int execute(command *cmd, int offset);
static int pipeExec(command *cmd, int offset);
static int generatePrc(command *cmd, int numPrc);
static int runExec(command *cmd);
static int runPipeExec(command *cmd);
static int exitExec(char **args);
static int cdExec(char **args);
static int historyExec(char **args);
char *replaceStr(const char *oldline, const char *newline, int start, int end);
char *trimLine(char *line);

static int flag = 1;
/**
 * history builtin function relevant function
 * Structure: bounded circled stack with default size
 */
histStack *histPtr;
histStack *histInit(void);
static void histReInit(histStack *ptr);
static void histPush(histStack *ptr, char *line);
static int histPopHead(histStack *ptr);
static void histList(histStack *ptr, int size);
/* function pointer jump table */
char *builtinCmd[] = {
"exit",
"cd",
"history"
};
static int (*builtinCmdFunc[])(char **) = {
exitExec,
cdExec,
historyExec
};
int main(int argc, char **argv)
{
	char *line = NULL;
	command *cmd = NULL;

	histPtr = histInit();
	while (flag) {
		printf("$");
		line = readLine();
		cmd = parseLine(line);
		flag = executeCtrl(cmd);
		if (!line)
			free(line);
		if (!cmd)
			free(cmd);
	}
	free(histPtr);
	return 0;
}
/**
 * readLine funciton will get the raw input of shell.
 * This function replaces !! and !string with correspending history
 */
char *readLine(void)
{
	char *line = NULL;
	char input;
	int pos = 0;

	line = (char *)malloc(DEF_BUFSIZE * sizeof(char));
	if (line == NULL) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	while ((input = getchar()) != EOF && input != '\n') {
		if (pos == (DEF_BUFSIZE)-1) {
			fprintf(stderr, "error: %s\n", "Command too long");
			exit(EXIT_FAILURE);
		}
	line[pos++] = input;
	}
	line[pos] = '\0';
	char *oldLine = line;

	line = trimLine(oldLine);
	free(oldLine);
	int countRec = 0, indRec[DEF_BUFSIZE],
	countRecStr = 0, indRecStr[DEF_BUFSIZE],
	indexR = 0, indexRS = 0;
	for (int i = 0; i < strlen(line); i++) {
		if (line[i] != '!')
			continue;
		int a = (i == 0 || line[i-1] == ' '
		|| line[i-1] == '|'),
		b = (line[i+1] == '!'),
		c = (line[i+2] == ' ' || line[i+2] == '|'
		|| line[i+2] == '\0'),
		d = (line[i+1] == ' ');
		if (a && b && c) {
			countRec++;
			indRec[indexR++] = i;
		} else if (a && !b && !d) {
			countRecStr++;
			indRecStr[indexRS++] = i;
			for (int j = i+1; j < strlen(line); j++) {
				if (line[j+1] == ' ' || line[j+1] == '|'
				|| line[j+1] == '\0') {
					indRecStr[indexRS++] = j;
					break;
				}
			}
		}
	}
	if (countRec) {
		if (histPtr->size <= 0) {
			fprintf(stderr, "error: %s\n", "History empty");
			histPush(histPtr, line);
			return NULL;
		}
		int index = histPtr->tail-1;

		if (index <= -1)
			index += 100;
		char *newL = histPtr->hist[index];
		char *oldL = line;

		for (int i = 0; i < countRec; i++) {
			line = replaceStr(oldL, newL, indRec[i], indRec[i]+1);
			free(oldL);
			histPush(histPtr, line);
		}
		return line;
	}
	if (countRecStr) {
		if (histPtr->size <= 0) {
			fprintf(stderr, "error: %s\n", "History empty");
			histPush(histPtr, line);
			return NULL;
		}
		char *oldL = line;

		for (int i = 0; i < countRecStr; i++) {
			int index = histPtr->tail;
			char *newL = NULL;
			int len = indRecStr[i*2+1] - indRecStr[i*2] + 1;
			char *strHead = (char *)malloc(sizeof(char) * len);

			if (strHead == NULL) {
				fprintf(stderr, "error: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			for (int j = 0; j < len-1; j++)
				strHead[j] = oldL[j+indRecStr[i*2]+1];
			strHead[len-1] = '\0';
			for (int j = 1; j <= histPtr->size; j++) {
				if ((index-j) <= -1)
					index += 100;
				char *hisL = histPtr->hist[index-j];

				if (!strncmp(hisL, strHead, strlen(strHead))) {
					newL = hisL;
					break;
				}
			}
			free(strHead);
			if (newL == NULL) {
				fprintf(stderr, "error: %s\n", "No matching");
				histPush(histPtr, line);
				return NULL;
			}
			int start = indRecStr[i],
			end = indRecStr[i+1];
			line = replaceStr(oldL, newL, start, end);
			free(oldL);
			histPush(histPtr, line);
			return line;
		}
	}
	histPush(histPtr, line);
	return line;
}
char *trimLine(char *line)
{
	if (line == NULL)
		return NULL;
	int start = 0;

	while (line[start] == ' ')
		start++;
	char *newLine;
	int len = strlen(line) - start + 1;

	newLine = (char *)malloc(sizeof(char) * len);
	if (newLine == NULL) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < len-1; i++)
		newLine[i] = line[i+start];
	newLine[len-1] = '\0';
	return newLine;
}
/**
 * parseLine function parses the line into command and arguments.
 * Command for pipes will also be put into separete command space
 */
command *parseLine(char *line)
{
	if (line == NULL)
		return NULL;
	if (line[0] == '|' || line[strlen(line)-1] == '|') {
		fprintf(stderr, "error: %s\n", "Pipe command void");
		return NULL;
	}
	for (int i = 0; i < strlen(line); i++) {
		if (line[i] == '|' && line[i+1] == '|') {
			fprintf(stderr, "error: %s\n", "Pipe command void");
			return NULL;
		}
	}
	char **lines;
	char *singleLine;
	const char delimPipe[2] = "|";

	lines = (char **)malloc(DEF_ARG_BUFSIZE * sizeof(char *));
	if (lines == NULL) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	int posLines = 0;

	singleLine = strtok(line, delimPipe);
	while (singleLine != NULL) {
		if (posLines == DEF_ARG_BUFSIZE) {
			fprintf(stderr, "error: %s\n", "Pipes overnum");
			exit(EXIT_FAILURE);
		}
		lines[posLines++] = singleLine;
		singleLine = strtok(NULL, delimPipe);
	}
	lines[posLines] = NULL;
	command *cmd = NULL;

	cmd = (command *)malloc(sizeof(command) * (posLines+1));
	if (cmd == NULL) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < posLines; i++) {
		if (isSpaceStr(lines[i]) == 1) {
			fprintf(stderr, "error: %s\n", "Pipe command void");
			return NULL;
		}
		char **args;
		char *arg;
		const char delim[2] = " ";

		args = (char **)malloc(DEF_ARG_BUFSIZE * sizeof(char *));
		if (args == NULL) {
			fprintf(stderr, "error: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		int posArg = 0;

		arg = strtok(lines[i], delim);
		while (arg != NULL) {
			if (posArg == DEF_ARG_BUFSIZE) {
				fprintf(stderr, "error: %s\n", "Arg overnum");
				exit(EXIT_FAILURE);
			}
			args[posArg++] = arg;
			arg = strtok(NULL, delim);
		}
		args[posArg] = NULL;
		cmd[i].args = args;
	}
	cmd[posLines].args = NULL;
	return cmd;
}
/*check if a string contains only whitespaces*/
int isSpaceStr(char *str)
{
	for (int i = 0; i < strlen(str); i++)
		if (str[i] != ' ')
			return 0;
	return 1;
}
/*execute the command with pipe and not pipe situation*/
int executeCtrl(command *cmd)
{
	if (cmd == NULL || cmd[0].args == NULL)
		return 1;
	if (cmd[1].args == NULL)
		return execute(cmd, 0);
	else
		return runPipeExec(cmd);
}
/*execute non pipe command*/
int execute(command *cmd, int offset)
{
	for (int i = 0; i < NUM_CMD_FUNCTION; i++) {
		if (!strcmp(cmd[offset].args[0], builtinCmd[i]))
			return (*builtinCmdFunc[i])(cmd[offset].args);
	}
	return runExec(cmd);
}
/*execute pipe command*/
int pipeExec(command *cmd, int offset)
{
	for (int i = 0; i < NUM_CMD_FUNCTION; i++) {
		if (!strcmp(cmd[offset].args[0], builtinCmd[i]))
			return (*builtinCmdFunc[i])(cmd[offset].args);
	}
	if (execv(cmd[offset].args[0], cmd[offset].args) < 0) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	return 1;
}
/*execute non pipe executable by creating child process*/
int runExec(command *cmd)
{
	pid_t pid;
	int status;

	pid = fork();
	if (pid < 0)
		fprintf(stderr, "error: %s\n", strerror(errno));
	else if (pid == 0) {
		if (execv(cmd[0].args[0], cmd[0].args) < 0) {
			fprintf(stderr, "error: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
	} else
		pid = wait(&status);
	return 1;
}
/*execute pipe command by creating pipes and processes*/
int runPipeExec(command *cmd)
{
	int numPrc = 0;

	while (cmd[numPrc].args != NULL)
		numPrc++;
	pid_t pid;
	int status;

	pid = fork();
	if (pid < 0)
		fprintf(stderr, "error: %s\n", strerror(errno));
	else if (pid == 0) {
		generatePrc(cmd, numPrc);
		exit(0);
	} else
		waitpid(pid, &status, 0);
	return 1;
}
/*create several child processes connected by pipes and last created */
/*process will exectue the first pipe command*/
int generatePrc(command *cmd, int numPrc)
{
	int fd[2], pids[numPrc], status, fdPipe[(numPrc-1)*2];
	pid_t pid;

	for (int i = 0; i < numPrc; i++) {
		if (i != (numPrc-1))
			pipe(fd);
		pid = fork();
		if (pid < 0) {
			fprintf(stderr, "error: %s\n", strerror(errno));
		} else if (pid == 0) {
			if (i == 0) {
				dup2(fd[0], 0);
				close(fd[1]);
			} else if (i == (numPrc-1)) {
				dup2(fd[1], 1);
				for (int j = 0; j < i*2; j++)
					if (j != 2*i-1)
						close(fdPipe[j]);
			} else {
				dup2(fdPipe[2*i-1], 1);
				dup2(fd[0], 0);
				close(fd[1]);
				for (int j = 0; j < i*2; j++)
					if (j != 2*i-1)
						close(fdPipe[j]);
			}
			pipeExec(cmd, numPrc-1-i);
		} else {
			pids[i] = pid;
			if (i != (numPrc-1)) {
				fdPipe[2*i] = fd[0];
				fdPipe[2*i+1] = fd[1];
			}
		}
	}
	for (int i = 0; i < (numPrc-1)*2; i++)
		close(fdPipe[i]);
	for (int i = 0; i < numPrc; i++)
		waitpid(pids[i], &status, 0);
	return 1;
}
/*run exit builtin function*/
int exitExec(char **args)
{
	if (args[1] == '\0')
		return 0;
	fprintf(stderr, "error: %s\n", "Use exit only to exit");
	return 1;
}
/*run cd builtin function*/
int cdExec(char **args)
{
	if (args[1] == '\0' || args[2] != '\0') {
		fprintf(stderr, "error: %s\n", "cd arg invalid");
		return 1;
	}
	if (chdir(args[1]) == -1)
		fprintf(stderr, "error: %s\n", strerror(errno));
	return 1;
}
/*run history builtin function*/
int historyExec(char **args)
{
	if (args[1] == '\0') {
		histList(histPtr, histPtr->size);
		return 1;
	}
	if (args[2] == '\0' && !strcmp(args[1], "-c")) {
		histReInit(histPtr);
		return 1;
	}
	if (args[2] == '\0') {
		int len = strlen(args[1]), num = 1;

		for (int i = 0; i < len; i++) {
			if (args[1][i] > '9' || args[1][i] < '0')
				num = 0;
		}
		if (!num) {
			fprintf(stderr, "error: %s\n", "Invalid argument");
			return 1;
		}
		int numOfHist = 0;

		for (int i = 0; i < len; i++)
			numOfHist += (args[1][len-i-1] - '0')*pow(10, i);
		histList(histPtr, numOfHist);
		return 1;
	}
	return 1;
}
/*history stack initialization*/
histStack *histInit(void)
{
	histStack *ptr;

	ptr = (histStack *)malloc(sizeof(histStack));
	if (ptr == NULL) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	ptr->size = 0;
	ptr->head = 0;
	ptr->tail = 0;
	ptr->histIndex = 0;
	return ptr;
}
/*history -c*/
void histReInit(histStack *ptr)
{
	for (int i = 0; i < ptr->size; i++) {
		free(ptr->hist[(ptr->head+i)%100]);
		ptr->histIndex++;
	}
	ptr->size = 0;
	ptr->head = 0;
	ptr->tail = 0;
}
/*push the raw input into history stack*/
void histPush(histStack *ptr, char *line)
{
	if (line == NULL)
		return;
	if (ptr->size == 100)
		histPopHead(ptr);
	int len = strlen(line)+1;

	ptr->hist[ptr->tail] = (char *)malloc(sizeof(char) * len);
	if (ptr->hist[ptr->tail] == NULL) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	strcpy(ptr->hist[ptr->tail++], line);
	if (ptr->tail == 100)
		ptr->tail -= 100;
	ptr->size++;
}
/*pop from head of history stack*/
int histPopHead(histStack *ptr)
{
	if (ptr->size <= 0)
		return 0;
	free(ptr->hist[ptr->head++]);
	if (ptr->head >= 100)
		ptr->head -= 100;
	ptr->size--;
	return 1;
}
/*history list*/
void histList(histStack *ptr, int size)
{
	if (ptr->size == 0) {
		printf("history empty\n");
		return;
	}
	if (size >= ptr->size)
		size = ptr->size;
	for (int i = 0; i < size; i++) {
		int index = (ptr->head+i)%100;

		printf("%d %s\n", (ptr->histIndex)+i, ptr->hist[index]);
	}
}

/*replace a string into another string*/
char *replaceStr(const char *oldLine, const char *newLine, int start, int end)
{
	char *line = NULL;
	int len = (end-start)+strlen(oldLine)+strlen(newLine)+2;

	line = (char *)malloc(sizeof(char) * len);
	for (int i = 0; i < start; i++)
		line[i] = oldLine[i];
	for (int i = 0; i < strlen(newLine); i++)
		line[i+start] = newLine[i];
	for (int i = 0; i < (strlen(oldLine)-end); i++)
		line[i+strlen(newLine)+start] = oldLine[i+end+1];
	line[len-1] = '\0';
	return line;
}
