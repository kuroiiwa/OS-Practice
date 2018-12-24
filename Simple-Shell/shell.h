// SPDX-License-Identifier: Columbia-kz2325
#ifndef SHELL_HEADER
#define SHELL_HEADER
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>

#define DEF_BUFSIZE 200
#define DEF_ARG_BUFSIZE 20
#define STACK_SIZE 100
#define NUM_CMD_FUNCTION (sizeof(builtinCmd)/sizeof(char *))


typedef struct {
	char **args;
} command;
typedef struct {
	char *hist[STACK_SIZE];
	int size,
	head,
	tail,
	histIndex;
} histStack;

extern int errno;

#endif
