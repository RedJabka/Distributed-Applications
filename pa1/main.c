#include "pa1.h"
#include "ipc.h"
#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
//#include <sys/wait.h>

static const char * const events_log;
static const char * const pipes_log;
static const char * const log_received_all_started_fmt;
static const char * const log_received_all_done_fmt;
static const char * const log_done_fmt;
static const char * const log_started_fmt;


int main(int argc, char *argv[]) {
    // Check for parameters 
    int child_num = 0;
    if (argc != 3 || strcmp(argv[1], "-p") != 0 || atoi(argv[2]) == 0){
        // printf("Неопознанный ключ или неверное количество аргументов! Пример: -p <количество процессов>\n");
        exit(1);
    }
    else{
        if (atoi(argv[2]) > 9 || atoi(argv[2]) <= 0){
            // printf("Некорректное количество процессов для создания (0 < x <= 9)!\n");
            exit(1);
        }   
        else
            //Assigning number of processes to the variable 
            child_num = atoi(argv[2]);
    }
    int id = 1;
    //int massive_pp[children_number + 1];
    int internal_id = 0;
    int pipe_fd[child_num + 1][child_num + 1][2];

    //open pipes
    for (int i = 0; i < (child_num + 1); i++){
        for (int j = 0; j < (child_num + 1); j++){
            if (i != j){ pipe(pipe_fd[i][j]); }
        }
    }

    //create children
    for (int i = 0; i < child_num; i++){
        if (id != 0){internal_id++; id = fork();}
    }

    if(id != 0){internal_id = 0;}

    //close unnecessary pipes
    for (int i = 0; i < (child_num + 1); i++){
        for (int j = 0; j < (child_num + 1); j++){
            if(internal_id != i && internal_id != j){close(pipe_fd[i][j][0]); close(pipe_fd[i][j][1]);}
        }
    }

    

    //printf("%d", f);
    //printf("%d\n", internal_id);

}
