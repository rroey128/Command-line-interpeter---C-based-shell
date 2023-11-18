#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

int main()
{

    int arr[2];
    pipe(arr);
    int readEnd = arr[0];
    int writeEnd = arr[1];
    printf("parent proccess>forking...\n");
    int child1 = fork();
    int child2;

    if (child1 == 0)
    {        
        printf("(child1>redirecting stdout to the write end of the pipe…)\n");                 // child 1 proccess
        close(readEnd);
        dup2(writeEnd, STDOUT_FILENO);
        close(writeEnd);
        char *args[] = {"ls", "-l", NULL};
        printf("(child1>going to execute cmd: …)\n");
        execvp(args[0], args);
    }

        if(child1>0){
        printf("parent proccess>created a procces with id: %d\n", child1);
        printf("parent_process>closing the write end of the pipe…\n");    
        close(writeEnd);
        printf("parent proccess>forking...\n");
        child2 = fork();
        }
    

    if (child2 == 0)
    { // child2 proccess
        printf("(child2>redirecting stdin to the read end of the pipe…)\n");
        close(writeEnd);
        dup2(readEnd, STDIN_FILENO);
        close(readEnd);
        char *args[] = {"tail", "-n", "2", NULL};
        printf("(child2>going to execute cmd: …)\n");
        execvp(args[0], args);
    }

   if (child2>0){
    // parent proccess
        printf("parent proccess>created a procces with id: %d\n", child2);
        printf("parent_process>closing the read end of the pipe…\n");
        close(readEnd);
        printf("parent_process>waiting for child processes to terminate…\n");
        waitpid(child1, NULL, 0);
        waitpid(child2, NULL, 0);
        printf("parent_process>exiting…\n");
   }
    
}