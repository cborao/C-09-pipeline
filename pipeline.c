
// JULIO 2020. CÉSAR BORAO MORATINOS: pipeline.c

#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

enum {
	Max = 3000,
	First = 0,
	Middle = 1,
	Last = 2,
};

struct Cmd {
	int position;
	char path[Max];
	char *arguments[Max];
};

typedef struct Cmd Cmd;

void
tokenize(char *str, char *result[]) {

	char *token;
	int i = 0;

	while ((token = strtok_r(str, " ", &str)) != NULL && i < Max) {
		if ((result[i] = malloc(strlen(token)+1)) < 0)
			errx(EXIT_FAILURE,"malloc failed!");

		strncpy(result[i],token,strlen(token)+1);
		i++;
	}
	result[i] = NULL;
}

int
checkpath(char *program, char *buffer) {

	size_t size = strlen("/bin/")+strlen(program)+1;
	char name[size];

	snprintf(name,size,"/bin/%s",program);
	if (access(name,X_OK) == 0) {
		strncpy(buffer,name,size);
		return 1;
	}
	size = strlen("/usr/bin/")+strlen(program)+1;
	char newname[size];
	snprintf(newname,size,"/usr/bin/%s",program);

	if (access(newname,X_OK) == 0) {
		strncpy(buffer,newname,size);
		return 1;
	}
	return -1;
}

void
dupandclose(Cmd input, int fd[], int fd2[]) {

	switch (input.position) {
		case First:
			if (dup2(fd[1],1) < 0)
				errx(EXIT_FAILURE, "error: dup2 failed");
			break;

		case Last:
			if (dup2(fd2[0],0) < 0)
				errx(EXIT_FAILURE, "error: dup2 failed");
			break;

		case Middle:
			if (dup2(fd[0],0) < 0)
				errx(EXIT_FAILURE, "error: dup2 failed");

			if (dup2(fd2[1],1) < 0)
				errx(EXIT_FAILURE, "error: dup2 failed");
		}

		if (close(fd[1]))
			errx(EXIT_FAILURE,"error: cannot close fd %d",fd[1]);

		if (close(fd2[1]))
			errx(EXIT_FAILURE,"error: cannot close fd %d",fd2[1]);

		if (close(fd[0]))
			errx(EXIT_FAILURE,"error: cannot close fd %d",fd[0]);

		if (close(fd2[0]))
			errx(EXIT_FAILURE,"error: cannot close fd %d",fd2[0]);
}

void
freeinput(char *Data[]) {
	int i = 0;
	while (Data[i] != NULL) {
		free(Data[i]);
		i++;
	}
}

void
execmd(Cmd input,int fd[],int fd2[],int *pid) {

	switch (*pid = fork()) {
		case -1:
			errx(EXIT_FAILURE, "error: fork failed!");
		case 0:
			dupandclose(input,fd,fd2);
			execv(input.path,input.arguments);
			errx(EXIT_FAILURE, "error: cannot execv");
	}
	freeinput(input.arguments);
}


void
runpipeline(int argc, char *argv[], int fd[], int fd2[], int *pid) {

	for (size_t i = 0; i < argc; i++) {

		Cmd input;

		input.position = i;
		tokenize((char *)argv[i],input.arguments);

		if (checkpath(input.arguments[0],input.path) < 0)
			errx(EXIT_FAILURE, "access failed: command %s not found", input.arguments[0]);

		execmd(input,fd,fd2,pid);

	}
}

int
main(int argc, char *argv[]) {

	argv++;
	argc--;

	int pid;
	int status;
	int result;
	int fd[2];
	int fd2[2];
	int exit_val = 1;

	if (argc != 3)
		errx(EXIT_FAILURE, "usage: '[command1]' '[command2]' '[command3]'");

	// create pipes
	if (pipe(fd) < 0)
		errx(EXIT_FAILURE, "error: cannot make a pipe");

	if (pipe(fd2) < 0)
		errx(EXIT_FAILURE, "error: cannot make a pipe");

	runpipeline(argc,argv,fd,fd2,&pid);

	//father
	close(fd[0]);
	close(fd[1]);
	close(fd2[0]);
	close(fd2[1]);

	while ((result = wait(&status)) != -1) {
		if (result == pid && WIFEXITED(status))
			exit_val = WEXITSTATUS(status);
	}
	exit(exit_val);
}
