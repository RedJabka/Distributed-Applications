#include "pa1.h"
#include "ipc.h"
#include "common.h"
#include <stdlib.h>
#include <stdio.h>

static const char * const events_log;
static const char * const pipes_log;
static const char * const log_received_all_started_fmt;
static const char * const log_received_all_done_fmt;
static const char * const log_done_fmt;
static const char * const log_started_fmt;


int main(int argc, char *argv[]) {
    // Check for parameters 
    int children_number = 0;
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
            children_number = atoi(argv[2]);
    }
    int id = 1;
    int massive_pp[children_number+1];
    int i = -1;
    int pipe_fd[2];

    for (int i = 1; i < children_number + 1; i++){
        if (id != 0){id = fork(); /*f++;*/}
    }

    i++;
    massive_pp[i] = getpid();


    //printf("%d", f);
    for (int i = 0; i < children_number + 1; i++) {
        if(id != 0){
        printf("%d\n", massive_pp[i]);
        }
    }

}
