#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <strings.h>
#include <sys/types.h>
#include <stdio.h>

#define NUM_PIPE 29  //the maximum number of pipes, which equals to the number of total child processes-1
#define NUM_AGUMENT 100 //the maximum number of each command arguments
#define INPUT_SIZE 1000 //the maximum size(bytes) of input instruction 
#define NUM_COMMAND 30 //the maximum number of commands that connected by pipes
#define BUFSIZE 1000

typedef struct COMMAND{
    char* header; //command header (e.g. "ls")
    char* com_arg[NUM_AGUMENT]; //command arguments
    int num_args; //number of arguments
}COMMAND;

void split_command(char *string, COMMAND* comm_line) {
    char *field;
    int i = 0;
    const char *delimiters = " ";
    bzero(comm_line,sizeof(COMMAND));
    while( (field = strsep(&string, delimiters)) != NULL ) {
        // Skip original multiple spaces
        if( *field == '\0' ) {
            continue;
        }
        if(i==0){comm_line->header=field;}
        comm_line->com_arg[i++] = field; 
    }
    comm_line->num_args=i;
    // execute the command
    if (execvp(comm_line->header, comm_line->com_arg) == -1) {
        printf("jsh error: Command not found: %s\n", comm_line->header);
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

int split_input(char *string, char *commands[NUM_COMMAND]){
    char *field;
    int i = 0;
    bzero(commands, NUM_COMMAND);
    const char *delimiters = "|";
    while( (field = strsep(&string, delimiters)) != NULL ) {
        commands[i++] = field;
    }
    return i;
}

void close_pipes(int num_commands, int fd[NUM_PIPE][2]){
    for(int i=0;i<num_commands-1;i++){
        close(fd[i][0]);
        close(fd[i][1]);
    }
}

int main(int argc, char* argv[])
{
    int fd[NUM_PIPE][2];
    char * comm_str = (char *)malloc(INPUT_SIZE * sizeof(char));
    char *commands[NUM_COMMAND]; //array to store commands seperated by pipes
    int pid;

    while (1){
        printf("jsh$ ");
        // get input using fgets() and remove appended "\n"
        bzero(comm_str,INPUT_SIZE * sizeof(char));
        fgets(comm_str, INPUT_SIZE-1, stdin);
        comm_str[strcspn(comm_str, "\n")] = 0;
        //if get "exit" command, then exit
        if(strcmp("exit",comm_str)==0){break;}
        // split standard input into commands
        int num_commands = split_input(comm_str, commands);
        //open pipes
        for(int i=0;i<num_commands-1;i++){
            if(pipe(fd[i])<0){
                perror("Pipe Error: ");
                exit(EXIT_FAILURE);
            }
        }

        //fork() to create child processes and excute the commands
        for(int j=0;j<num_commands;j++){
            //create a single child process and check error
            if ((pid = fork())<0)
            {
                perror("Fork Error: ");
                exit(EXIT_FAILURE);
            }
            else if (pid == 0)  //if in child process
            {   
                if(num_commands>1){  //when there is at least a pipe, do redirection
                    //if this is the first child process that processes the first command, then only replace the standard output
                    if(j==0){
                        close(STDOUT_FILENO);
                        dup(fd[0][1]);
                        close_pipes(num_commands, fd);
                    }
                    //if this is the last child process that processes the last command, then only replace the standard input
                    else if(j==num_commands-1){
                        close(STDIN_FILENO);
                        dup(fd[num_commands-2][0]);
                        close_pipes(num_commands, fd);
                    }
                    //if this is the child in the middle, then replace both standard input and standard output
                    else{
                        dup2(fd[j-1][0], STDIN_FILENO);
                        dup2(fd[j][1], STDOUT_FILENO);
                        close_pipes(num_commands, fd);
                    }
                }
                COMMAND* comm_line = malloc(sizeof(COMMAND)); //struct to store a single command information
                split_command(commands[j], comm_line);
                free(comm_line);
            }
        }
        
        //close all the pipes
        close_pipes(num_commands, fd);    
        //wait all child processes
        int wstatus;
        int wc;
        int statuscode;
        int last_pid=-1; //process id of the last command
        int last_status; //status of the last command
        
        while((wc=wait(&wstatus))!=-1){
            statuscode = WEXITSTATUS(wstatus);
            if(wc>last_pid){
                last_pid=wc; 
                last_status=statuscode;
            }
        }

        if(last_status==0){
            printf("jsh status: %d\n", last_status);
        }else if(last_status==1){
            printf("jsh status: 127\n");
        }
        else{
            printf("jsh status: %d\n", last_status);
        }
    }
    free(comm_str);
    return 0;
}

