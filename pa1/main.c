#include "pa1.h"
#include "ipc.h"
#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <syscall.h>

static const char * const events_log;
static const char * const pipes_log;
static const char * const log_received_all_started_fmt;
static const char * const log_received_all_done_fmt;
static const char * const log_done_fmt;
static const char * const log_started_fmt;
static char bffr[50] = "";//creating buffer
static int fd[12][12][2]; //file descriptor
MessageType lrm[13]; //last message that was recieved by a process
static int closed_pipes[12][12] = {{0}}; //Describing status of a pipe beetwen process i and j. 0 - closed, 1 - onlu writing and 2 - only reading


Message create_msg(MessageType msgType, const char *msg) {
    Message message;

    message.s_header.s_magic = MESSAGE_MAGIC;
    message.s_header.s_payload_len = strlen(msg);
    message.s_header.s_local_time = time(NULL);
    
    switch (msgType) {
        case STARTED:
            message.s_header.s_type = STARTED;
            break;
        case DONE:
            message.s_header.s_type = DONE;
            break;
        default:
            message.s_header.s_type = ACK; 
            break;
    }
    strncpy(message.s_payload, msg, sizeof(message.s_payload));
    return message;
}

//structure to work with our send/recieve funcs
struct Processs{
    
    local_id id;
    pid_t pid;
    pid_t parent_pid;
    int child_num;
    int parallel_pro;
};

//initialization of a parent Processs
static void processs_parent(struct Processs *proc, pid_t t, int child_num){
    proc->id = PARENT_ID;
    proc->pid = t;
    proc->parent_pid = -1;
    proc->child_num = child_num;
    proc->parallel_pro = 0;

}

//initialization of a child Processs
static void processs_child(struct Processs *child, local_id id, pid_t pid, pid_t parent_pid, int32_t child_num) {
    child->id = id;
    child->pid = pid;
    child->parent_pid = parent_pid;
    child->child_num = 0;
    child->parallel_pro = child_num-1;
}

//send implementation
int send(void * self, local_id dst, const Message * msg) {
    struct Processs *sender = (struct Processs *)self;
    int wfd = fd[sender->id][dst][1];
    if (write(wfd, msg, sizeof(MessageHeader) + (msg->s_header.s_payload_len)) == -1) {perror("error on sending"); return -1;}
    return 0;
}

//send_all implementation
int send_multicast(void * self, const Message * msg){
    struct Processs *sender = (struct Processs *)self;
    int senders = sender->parallel_pro+1;;
    if (sender->child_num != 0) {senders = sender->child_num;}
    for(local_id i = 0; i <= senders; i++){
        send(sender, i, msg);
    }
    return 0;
}

//recieve implementation
int receive(void * self, local_id from, Message * msg) {
    struct Processs *receiver = (struct Processs *)self;
    if (receiver->id == from) {perror("error on recieve"); return -1;}
    int read_fd = fd[from][receiver->id][0];
    if (read(read_fd, msg, sizeof(MessageHeader)) == -1) {perror("error on recieve"); return -1;}
    lrm[from] = msg->s_header.s_type;
    if (read(read_fd, msg, msg->s_header.s_payload_len) == -1) {perror("error on reading tail"); return -1;}
    return 0;
}

//recicve any implementation
int receive_any(void * self, Message * msg);

//Synchronizing before doing work
int ready_to_go(struct Processs *child){
    Message msg = create_msg(STARTED, bffr);
    if (send_multicast(child, &msg) != 0){printf("Sendmulticast in read_to_go error\n"); return 1;
    }

    int z = 0;
    while (z < 1000) {
        int32_t recievers;
        recievers = child->parallel_pro+1;
        for (int32_t i = 1; i <= recievers; i++) {
            if (child->id != i) {
                if (receive(child, i, &msg) != 0) {
                    return -1;
                }
            }
        }
        z++;
        int srted = 0;
        for (int i = 1; i <= (child->parallel_pro)+1; i++) {
            if (lrm[i] == STARTED && i != child->id) {
                srted++;
            }
        }
    
        if (srted == child -> parallel_pro) {
            sprintf(bffr, log_received_all_started_fmt, child->id);
            printf(log_received_all_started_fmt, child->id);
            return 0;
        }
    }
    return -1;
}


//work
int work(struct Processs *parent, struct Processs *child){
    sprintf(bffr, log_done_fmt, child->id);
    printf(log_done_fmt, child->id);
    return 0;
}

//syncronize after work
int after_work(struct Processs *parent, struct Processs *child){
    Message done_msg = create_msg(DONE, bffr);
    int z = 0;
    while (z < 1000) {
        int32_t recievers= child->parallel_pro+1;
        for (int32_t i = 1; i <= recievers; i++) {
            if (child->id != i) {
                if (receive(child, i, &done_msg) != 0) {
                    return -1;
                }
            }
        }
        z++;
        int recivied_done = 0;
        for (int i = 1; i <= (child->parallel_pro)+1; i++) {
            if (lrm[i] == DONE && i != child->id)
                recivied_done++;
        }
        if (recivied_done == child->parallel_pro) {
            sprintf(bffr, log_received_all_done_fmt, child->id);
            printf(log_received_all_done_fmt, child->id);
            return 0;
        }
    }
    return -1;
}

void close_pipes(local_id l_id, int child_num){
    for(int i = 0; i < child_num + 1; i++){
        for(int j = 0; j < child_num + 1; j++){
            if(i != j){
                if(closed_pipes[i][j] == 2 || closed_pipes[i][j] == 0){
                    if(close(fd[i][j][0] != 0)) {exit(33);}
                    closed_pipes[i][j] += 1;
                    //log info
                }
                if(closed_pipes[i][j] == 1 || closed_pipes[i][j] == 0){
                    if (close(fd[i][j][1]) != 0) {exit(34);}
                    closed_pipes[i][j] += 2;
                    //log info

                }
            }
        }
    }

}

int started_check(struct Processs *parent){
    Message msg = create_msg(STARTED, bffr);
    int z = 0;
    while (z <= 999 ) {
        int recievers = parent->child_num;
        for (int i = 1; i <= recievers; i++) {
            if (parent->id != i) {
                if (receive(parent, i, &msg) != 0) {return -1;}
            }
        }
        z++;
        int rs = 0;
        for (int i = 1; i <= (parent->child_num); i++) {if (lrm[i] == STARTED && i != PARENT_ID) {rs++;}}
        if (rs == parent->child_num) {return 0;}
        }

    return -1;
}

int ready_to_end (struct Processs *parent){
    Message done_msg = create_msg(DONE, bffr);
    int z = 0;
    while (z <= 999) {
        int recievers = parent->child_num;
        for (int i = 1; i <= recievers; i++) {
            if (parent->id != i) {
                if (receive(parent, i, &done_msg) != 0) {return -1;}
            }
        }
        z++;
        int rd = 0;
        for (int i = 1; i <= (parent->child_num); i++) {if (lrm[i] == DONE && i != PARENT_ID) {rd++;}}
        if (rd == parent->child_num) 
            return 0;
        }
    return -1;
}



int main(int argc, char *argv[]) {
    //check for parameters 
    int child_num = 0;
    if (argc != 3 || strcmp(argv[1], "-p") != 0 || atoi(argv[2]) == 0){
        //printf("Неопознанный ключ или неверное количество аргументов! Пример: -p <количество процессов>\n");
        exit(1);
    }
    else{
        if (atoi(argv[2]) > 9 || atoi(argv[2]) <= 0){
            //printf("Некорректное количество процессов для создания (0 < x <= 9)!\n");
            exit(1);
        }   
        else
            //Assigning number of processes to the variable 
            child_num = atoi(argv[2]);
    }

    //creating/opening files
    //int events_file = open(events_log, O_WRONLY | O_APPEND | O_CREAT);
    //int pipes_file = open(pipes_log, O_WRONLY | O_APPEND | O_CREAT); 
    /*if (events_file == -1)
        exit(2);
    if (pipes_file == -1)
        exit(3);*/

    int fid = 1;
    //int massive_pp[children_number + 1];
    int internal_id = 0;
    int pipe_fd[child_num+1][child_num+1][2];

    int num_pipes = 0;

    //open pipes
    for (int i = 0; i < (child_num + 1); i++){
        for (int j = 0; j < (child_num + 1); j++){
            if (i != j){ if(pipe(pipe_fd[i][j]) == -1) {perror("opening pipes"); exit(4);} num_pipes+=2;}
        }
    }

    struct Processs parent;
    struct Processs child;
    processs_parent(&parent, getpid(), child_num);

    //printf("%d\n", num_pipes);

    //create children 
    for (int i = 0; i < child_num; i++){
        if (fid != 0){internal_id++; fid = fork();}

        if ((fid == 0) && (i + 1 == internal_id)){processs_child(&child, internal_id, getpid(), parent.pid, child_num);}
    }

    if(fid != 0){internal_id = 0;}

    //close unnecessary pipes
    for (int i = 0; i < (child_num + 1); i++){
        for (int j = 0; j < (child_num + 1); j++){
            if(i != j){
            if (internal_id != i) {close(pipe_fd[i][j][1]); num_pipes--;} 
            if (internal_id != j) {close(pipe_fd[i][j][0]); num_pipes--;}
            }
        }
    }
    
    

    //printf("%d\n", fid);

    //printf("%d", f);
    //printf("%d\n", internal_id);

    if(fid == 0){//if it is a child process
        //logging info
        //

        if(ready_to_go(&child) == 0) {/*write log*/} //synchronize before work and write log
        else {exit(22);}

        if(work(&parent, &child) == 0) {/*write log*/} //do work and log it
        else {exit(23);}

        if(after_work(&parent, &child) == 0) {{/*write log*/}} //synchronize after work and write log
        else {exit(24);}

        close_pipes(child.id, child_num);
        //close file
        //close pipe file

        exit(0);
        }

    else{//if it is a parent process
        int status;
        int copy_num = child_num;

        while(ready_to_end(&parent) != 0 ) {continue;}

        while(copy_num != 0){if ((status = waitpid(-1, NULL, WNOHANG)) <= 0){copy_num--;}}

        while(ready_to_end(&parent) != 0) {continue;}

        close_pipes(PARENT_ID, parent.child_num);
        //close file
        //close pipe file
        exit(0);
    }
    return 0;
}
