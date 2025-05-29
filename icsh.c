/* ICCS227: Project 1: icsh
 * Name:Pisit Iampattanatham
 * StudentID:6280318
 */

#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "sys/types.h"
#include "sys/wait.h"
#include "unistd.h"
#include "errno.h"
#include "signal.h"
#include "fcntl.h"
#define MAX_CMD_BUFFER 255
#define MAX_CMD_ARGS 64
char buffer[MAX_CMD_BUFFER];
char last_command[MAX_CMD_BUFFER] = "";
int exit_command = 0;
int pid;
int exit_status = 0;
int bg_job = 0;
int job_id = 0;



void SIGINT_handler(int signum) {
    if (pid > 0) {
        kill(pid, SIGINT);
    }
}

void SIGTSTP_handler(int signum) {
    if (pid > 0) {
       kill(pid, SIGTSTP);   
    }
}

void exec_com(char *command, char **args) {
    pid = fork();
    int status;
    if (pid < 0) {
        perror("Fork failed");
        exit(errno);
    }

    if (!pid) {
        int can_exec = execvp(args[0], args);

        if (can_exec < 0) {
            printf("bad command\n");
        }
        exit(errno);
    }

    if (pid) {
        waitpid(pid, &status, WUNTRACED);
        exit_status = WEXITSTATUS(status);
    }
}

void redirect_output(char *command, char **args) {
    char *output_file = strchr(command, '>');
    char *input_file = strchr(command, '<');

    if (output_file != NULL) { // '>'
        char *command_end = output_file;
        while (*command_end == ' ' || *command_end == '>') {
            *command_end = '\0';
            command_end++;
        }
        char *filename = strtok(command_end, " ");
        int output_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (output_fd == -1) {
            perror("Failed to open output file");
            return;
        }

        int saved_stdout = dup(1);
        if (dup2(output_fd, 1) == -1) {
            perror("Failed to redirect output");
            return;
        }
        exec_com(command, args);

        dup2(saved_stdout, 1);
        close(output_fd);
    }
    else if (input_file != NULL) { // '<'
        char *command_end = input_file;
        while (*command_end == ' ' || *command_end == '<') {
            *command_end = '\0';
            command_end++;
        }
        char *filename = strtok(command_end, " ");
        int input_fd = open(filename, O_RDONLY);
        if (input_fd == -1) {
            perror("Failed to open input file");
            return;
        }

        int saved_stdin = dup(0);
        if (dup2(input_fd, 0) == -1) {
            perror("Failed to redirect input");
            return;
        }
        exec_com(command, args);

        dup2(saved_stdin, 0);
        close(input_fd);
    } else {
        // No redirection, execute the command as is
        exec_com(command, args);
    }
}

void command(char* buffer) {
    char **args = (char **)malloc(sizeof(char *) * MAX_CMD_BUFFER);
    char *command = (char *)malloc(sizeof(strlen(buffer) + 1));
    strcpy(command, buffer);
    char *sp_word = strtok(command, " ");
    int i = 0;

    int count = 0;
    int redirect = 0;

    while (sp_word != NULL) {
        args[i] = sp_word;
        sp_word = strtok(NULL, " ");
        i++;
    }
    args[i] = NULL;

    if (strlen(buffer) == 0) {
        return;
    }
    else if (!strncmp(buffer, "echo", 4)) {
        if (buffer[4] == '\0' || strcmp(buffer + 5, "") == 0) {
            printf("No text after echo command\n");
        } else {
            printf("%s\n", buffer + 5);
            strcpy(last_command, buffer);
        }
    }
    else if (strlen(buffer) == 2 && strncmp(buffer, "!!", 2) == 0) {
        if (strlen(last_command) > 0) {
            printf("%s\n", last_command);
            printf("%s\n", buffer + 5);
        }
        else {
            printf("No last command, back to the prompt\n");
        }
    }
    else if (!strncmp(buffer, "exit", 4)) {
        if (strlen(buffer) > 5) {
            exit_command = atoi(buffer + 5);

            if (exit_command>=0 && exit_command<=255) {
                printf("bye\n");
                exit(exit_command);
            }
            else{
                printf("Wrong exit command\n");
            }
        }
        else {
            printf("Missing exit command\n");
        }
    }
    else {
        printf("bad command\n);")
    }
    free(args);
}

void script_mode(char* filename) {
    FILE* read_file;
    char line[MAX_CMD_BUFFER];

    read_file = fopen(filename, "r");

    if (read_file == NULL) {
        printf("Cannot open file: %s\n", filename);
        return;
    }

    while (fgets(line, MAX_CMD_BUFFER, read_file) != NULL) {
        line[strcspn(line, "\n")] = 0;
        // printf("%s\n", line);
        command(line);
    }
    fclose(read_file);
}
int main(int argc, char *argv[]) {
    struct sigaction sa1, sa2;
    sa1.sa_flags = 0;
    sa1.sa_handler = SIGINT_handler;
    sigemptyset(&sa1.sa_mask);
    sigaction(SIGINT, &sa1, NULL);

    sa2.sa_flags = 0;
    sa2.sa_handler = SIGTSTP_handler;
    sigemptyset(&sa2.sa_mask);
    sigaction(SIGTSTP, &sa2, NULL);
    if (argc == 2) {
       script_mode(argv[1]);
    }
    else {
        printf("Starting IC Shell\n");
        while (1) {
            printf("icsh $ ");
            fgets(buffer, 255, stdin);

            buffer[strcspn(buffer, "\n")] = 0;
            command(buffer);
        }
    }
    return 0;
}
