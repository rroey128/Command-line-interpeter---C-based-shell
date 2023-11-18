#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include "LineParser.h"

#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0
#define HISTLEN 20

typedef struct process
{
    cmdLine *cmd;         /* the parsed command line*/
    pid_t pid;            /* the process id that is running the command*/
    int status;           /* status of the process: RUNNING/SUSPENDED/TERMINATED */
    struct process *next; /* next process in chain */
} process;

enum command_type
{
    COMMAND_SINGLE,
    COMMAND_LEFT,
    COMMAND_RIGHT
};

static int toggleDebug;
static process *p_list = NULL;

void freeProcess(process *process);
void updateProcessList(process **process_list);
void deleteTerminated(process **process_list);

void addProcess(process **process_list, cmdLine *cmd, pid_t pid)
{
    process *toInsert = malloc(sizeof(process));
    toInsert->cmd = cmd;
    toInsert->pid = pid;
    toInsert->status = RUNNING;

    if (*process_list != NULL)
        toInsert->next = *process_list;
    *process_list = toInsert;
}

void printProcessList(process **process_list)
{
    updateProcessList(process_list); // Task 3b
    if (*process_list == NULL)
    {
        printf("There are no processes RUNNING currently\n");
        return;
    }

    int index = 0;
    for (process *p = *process_list; p != NULL; p = p->next)
    {
        printf("%d ", index);
        printf("%d ", p->pid);
        printf("%s ", p->status == RUNNING ? "RUNNING" : p->status == SUSPENDED ? "SUSPENDED"
                                                                                : "TERMINATED");
        for (int i = 0; i < p->cmd->argCount; i++)
            printf("%s ", p->cmd->arguments[i]);
        printf("\n");
        index++;
        //<index in process list> <process id> <process status> <the command together with its arguments>
    }
    deleteTerminated(process_list);
}

void deleteTerminated(process **process_list)
{
    process *curr = *process_list;
    process *prev = NULL;
    while (curr != NULL)
    {
        if (curr->status == TERMINATED)
        {
            if (prev == NULL)
            {
                *process_list = curr->next;
            }
            else
            {
                prev->next = curr->next;
            }
            process *temp = curr;
            curr = curr->next;
            freeProcess(temp);
        }
        else
        {
            prev = curr;
            curr = curr->next;
        }
    }
}

void freeProcessList(process *process_list)
{
    while (process_list)
    {
        freeProcess(process_list);
        process_list = process_list->next;
    }
}

void freeProcess(process *process)
{
    if (process)
    {
        freeCmdLines(process->cmd); // Make sure this function actually needs to free cmd
        free(process);
    }
}

void updateProcessList(process **process_list)
{
    process *proc = *process_list;
    int status;
    pid_t res;
    while (proc)
    {
        res = waitpid(proc->pid, &status, WUNTRACED | WNOHANG | WCONTINUED); // VSCode does not like it but it compiles
        if (res == -1)
        {
            // No child with that pid (should not happen)
            proc->status = TERMINATED;
        }
        else if (res > 0)
        {
            if (res > 0 && (WIFEXITED(status) || WIFSIGNALED(status)))
            {
                proc->status = TERMINATED;
            }
            else if (WIFCONTINUED(status))
            {
                proc->status = RUNNING;
            }
            else if (WIFSTOPPED(status))
            {
                proc->status = SUSPENDED;
            }
        }
        proc = proc->next;
    }
}

void updateProcessStatus(process *process_list, int pid, int status)
{
    process *p = process_list;
    while (p && p->pid != pid)
        p = p->next;
    if (p)
        p->status = status;
}

void debugPrintf(const char *format, ...)
{
    va_list args;

    if (!toggleDebug)
        return;

    va_start(args, format);

    vfprintf(stderr, format, args);

    va_end(args);
}

void execute_program(cmdLine *pCmdLine, enum command_type type, int readEnd, int writeEnd)
{
    debugPrintf("%d\n", getpid());
    debugPrintf("%s\n", pCmdLine->arguments[0]);

    if (pCmdLine->inputRedirect != NULL)
    {
        int inputFileDescriptor = open(pCmdLine->inputRedirect, O_RDONLY);
        dup2(inputFileDescriptor, STDIN_FILENO);
        close(inputFileDescriptor);
    }

    if (type == COMMAND_LEFT)
    {
        close(readEnd);
        dup2(writeEnd, STDOUT_FILENO);
        close(writeEnd);
    }
    if (type == COMMAND_RIGHT)
    {
        close(writeEnd);
        dup2(readEnd, STDIN_FILENO);
        close(readEnd);
    }

    if (pCmdLine->outputRedirect != NULL)
    {
        int outputFileDescriptor = open(pCmdLine->outputRedirect,
                                        O_WRONLY | O_CREAT | O_TRUNC, 0666);
        dup2(outputFileDescriptor, STDOUT_FILENO);
        close(outputFileDescriptor);
    }

    execvp(pCmdLine->arguments[0], pCmdLine->arguments);
    // If reached, execvp failed
    perror("execv");
    _exit(0);
}

void execute(cmdLine *pCmdLine)
{
    int status;
    int readEnd = 0;
    int writeEnd = 0;

    if (pCmdLine->next != NULL &&
        (pCmdLine->next->inputRedirect != NULL || pCmdLine->outputRedirect != NULL))
    {
        fprintf(stderr, "ERROR - invalid redirection of output/input : cannot redirect chained commands");
        return;
    }

    if (pCmdLine->next != NULL)
    {
        int arr[2];
        pipe(arr);
        readEnd = arr[0];
        writeEnd = arr[1];
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(0);
    }

    if (pid == 0)
    {
        execute_program(pCmdLine,
                        pCmdLine->next == NULL ? COMMAND_SINGLE : COMMAND_LEFT,
                        readEnd, writeEnd);
        return;
    }

    addProcess(&p_list, pCmdLine, pid);

    if (pCmdLine->next != NULL)
    {
        close(writeEnd);
        pid_t pid2 = fork();

        if (pid2 < 0)
        {
            perror("fork");
            exit(0);
        }
        if (pid2 == 0)
        {
            execute_program(pCmdLine->next, COMMAND_RIGHT, readEnd, writeEnd);
        }
        addProcess(&p_list, pCmdLine->next, pid2);
        close(readEnd);
        waitpid(pid, &status, 0);
        waitpid(pid2, &status, 0);
    }

    if (pCmdLine->next == NULL && pCmdLine->blocking == 1)
    {
        waitpid(pid, &status, 0);
    }
}

void cd(cmdLine *pCmdLine)
{
    debugPrintf("%d\n", getpid());
    debugPrintf("%s\n", pCmdLine->arguments[0]);
    if (chdir(pCmdLine->arguments[1]) != 0)
    {
        fprintf(stderr, "No such file or directory");
    }
}

void sendSignal(char *command, pid_t pid)
{
    if (strcmp(command, "suspend") == 0)
    {
        if (kill(pid, SIGTSTP) == -1)
            printf("suspend failed");
        else
            printf("Process %d suspended\n", pid);
    }

    else if (strcmp(command, "kill") == 0)
    {
        if (kill(pid, SIGTERM) == -1)
            printf("kill failed");
        else
            printf("Process %d killed\n", pid);
    }

    else if (strcmp(command, "wake") == 0)
    {
        if (kill(pid, SIGCONT) == -1)
            printf("wake failed");
        else
            printf("Process %d awaked\n", pid);
    }
}

void addCmd(char **history, char *cmd, int *newest, int *oldest)
{
    if (strcmp(cmd, "\n") == 0)
        return;
    *newest = (*newest + 1) % HISTLEN;
    if (*oldest == -1)
        *oldest = 0;
    else if (*newest == *oldest)
        *oldest = (*oldest + 1) % HISTLEN;
    strncpy(history[*newest], cmd, 1024);
}

void printHistory(char **history, int *newest, int *oldest) // a b hist | newest = 2, oldest = 0
{
    int i, count = 1;
    for (i = *oldest; i != *newest; i = (i + 1) % HISTLEN)
    {
        if (strcmp(history[*oldest], "") != 0)
        {
            printf("%d %s\n", count++, history[i]);
        }
    }
    if (strcmp(history[*newest], "") != 0)
        printf("%d %s\n", count, history[*newest]);
}

int main(int argc, char **argv)
{
    int exec = 0;
    char cwd[1024];
    char inputFromUser[2048];
    FILE *file = stdin;
    char *history[HISTLEN];
    int newest = -1;
    int oldest = -1;

    for (int i = 0; i < HISTLEN; i++)
    {
        history[i] = malloc(1024 * sizeof(char));
    }

    for (int i = 0; i < argc; i++)
    {
        if (strstr(argv[i], "-d") != NULL)
            toggleDebug = 1;
    }

    while (1)
    {
        exec = 0;
        cmdLine *line;
        if (getcwd(cwd, sizeof(cwd)) == NULL)
            continue;

        printf("%s\n", cwd);
        fgets(inputFromUser, sizeof(inputFromUser), file);
        if (strcmp(inputFromUser, "quit\n") == 0)
            break;
        if (inputFromUser[0] != '!')
            addCmd(history, inputFromUser, &newest, &oldest);
    label:
        line = parseCmdLines(inputFromUser);
        if (line == NULL)
            continue;
        char *command = line->arguments[0];

        if (strcmp(command, "cd") == 0)
            cd(line);
        else if (strcmp(command, "suspend") == 0 ||
                 strcmp(command, "kill") == 0 ||
                 strcmp(command, "wake") == 0)
            sendSignal(command, atoi(line->arguments[1]));
        else if (strcmp(command, "procs") == 0)
            printProcessList(&p_list);
        else if (strcmp(command, "history") == 0)
            printHistory(history, &newest, &oldest);
        else if (strcmp(command, "!!") == 0)
        {
            strncpy(inputFromUser, history[newest], 1024);
            goto label;
        }
        else if (command[0] == '!')
        {
            char *nString = &command[1];
            int n = atoi(nString);
            if (n > HISTLEN || n < 1)
                printf("Index out of range: %d\n", n);
            else if (strcmp(history[(oldest + n - 1) % HISTLEN], "") == 0)
                printf("%d: Event not found\n", n);
            else
            {
                strncpy(inputFromUser, history[(oldest + n - 1) % HISTLEN], 1024);
                goto label;
            }
        }
        else
        {
            exec = 1;
            execute(line);
        }
        if (!exec)
            freeCmdLines(line);
    }
    for (int i = 0; i < HISTLEN; i++)
    {
        if (history[i])
            free(history[i]);
    }
    freeProcessList(p_list);
}