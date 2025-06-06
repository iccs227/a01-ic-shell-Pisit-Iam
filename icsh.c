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
struct job{
    int id;
    pid_t pid;
    char* state;
    char* command;
    struct job* next;
};
typedef struct job job;
job* head = NULL;

void add_new_job(char *command,char *state){
    job* new_job = (job*) malloc(sizeof(job));
    char *args[MAX_CMD_ARGS];

    new_job->id = job_id + 1; // Increment the job ID
    new_job->pid = pid;

    size_t len = strlen(command);
    new_job->command = (char*) malloc(sizeof(char) * len);
    memcpy(new_job->command, command, len);

    len = strlen(state);
    new_job->state = (char*) malloc(sizeof(char) * len);
    memcpy(new_job->state, state, len);

    new_job->next = NULL;

    // Insert the new job into the list in sorted order based on job id
    if (head == NULL || new_job->id < head->id) {
        new_job->next = head;
        head = new_job;
    } else {
        job* current = head;
        while (current->next != NULL && new_job->id > current->next->id) {
            current = current->next;
        }
        new_job->next = current->next;
        current->next = new_job;
    }
    printf("[%d] %d\n", new_job->id, new_job->pid);
    job_id++; // Increment the global job_id
}

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

void SIGCHLD_handler(int signum) {
    if (pid> 0) {
//        kill(pid, SIGTSTP);
        job* current = head;
        job* prev = NULL;

        while (current != NULL) {
            int status;
            pid_t result = waitpid(current->pid, &status, WNOHANG);

            if (result == current->pid) {
                // The background job has completed
                printf("\n[%d] Done                    %s\n", current->id, current->command);

                if (prev == NULL) {
                    // This is the first job in the list
                    head = current->next;
                    job_id--;
                } else {
                    prev->next = current->next;
                }

                job* temp = current;
                current = current->next;
                free(temp);
                break;
            } else {
                prev = current;
                current = current->next;
            }
        }
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
        if (bg_job) {
            add_new_job(command,"r");
            // job* current = head;
            bg_job = 0;
            
            memset(buffer, 0, sizeof(buffer));
        }
        else{
            signal(SIGTSTP, SIGTSTP_handler);

            waitpid(pid, &status, WUNTRACED);

            if (WIFSTOPPED(status)) {

                job* current = head;

                if (current == NULL)
                {
                    add_new_job(command,"s");
                    job * current = head;
                    printf("\n[%d] Stopped\t\t%s\n", current->id, current->command);
                }
                
                while (current != NULL) {
                    if (current->pid == pid) {
                        kill(pid, SIGTSTP);
                        current->state = "s";
                        printf("\n[%d] Stopped\t\t%s\n", current->id, current->command);
                    }
                    current = current->next;
                }
            } 
            else {
                exit_status = WEXITSTATUS(status);
            }
        }
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
    else if (!strncmp(buffer, "echo $?", 7)) {
            printf("%d\n", exit_status);
            strcpy(last_command, buffer);
    }
    else if (!strncmp(buffer, "sleep", 5)) {
        int i=0;
        while(args[i] != NULL ){
            if(!strcmp(args[i],"&") && i == 2){
                bg_job = 1;
                args[i] = NULL;
                break;
            }
            i++;
        }
        exec_com(buffer, args);
    }

    else if (!strncmp(buffer, "jobs", 4)) {
        job* current = head;
        if(current == NULL){
            printf("No running job right now\n");
        }else{
            while(current != NULL){
                if (!strcmp(current->state,"r"))
                {
                    printf("[%d] Running                 %s\n",current->id,current->command);
                }
                else if (!strcmp(current->state,"s"))
                {
                    printf("[%d] Stopped                 %s\n",current->id,current->command);
                }
                
                current = current->next;
            }
            if (current == NULL) {
                strcpy(buffer, ""); // Reset last_command
            }
        }
    }

    else if (!strncmp(buffer, "fg", 2)) {

        int job_id = atoi(&buffer[strlen(buffer)-1]);

        job* current = head;
        job* prev = NULL;

        while (current != NULL) {
            if (current->id == job_id) {
                printf("%s\n", current->command); // Print the command
                int status;
                waitpid(current->pid, &status, 0); // Wait for the job to complete
                exit_status = WEXITSTATUS(status);

                if (prev == NULL) {
                    // This is the first job in the list
                    head = current->next;
                    job_id--;
                } else {
                    prev->next = current->next;
                }
                free(current);
                break;
            } else {
                prev = current;
                current = current->next;
            }
        }
    }

    else if (!strncmp(buffer, "bg", 2)) {
        int job_id = atoi(&buffer[strlen(buffer) - 1]);

        job* current = head;

        while (current != NULL) {
            if (current->id == job_id && strcmp(current->state, "s") == 0) {
                kill(current->pid, SIGCONT);

                current->state = "r";

                printf("[%d] %s &\n", current->id, current->command);

                break;
            }
            current = current->next;
        }
    }
    else {
        for (i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], "<") == 0 || strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
                  redirect = 1;
                  break;
                  }
            args[count++] = args[i];
        }
        args[count] = NULL;

        if (redirect) {
            redirect_output(buffer, args);
        }
        else {
            exec_com(buffer, args);
        }
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

    sa2.sa_flags = 0;
    sa2.sa_handler = SIGCHLD_handler;
    sigemptyset(&sa2.sa_mask);
    sigaction(SIGCHLD, &sa2, NULL);
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
