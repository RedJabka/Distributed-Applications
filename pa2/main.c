#include "common.h"
#include "ipc.h"
#include "pa1.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>


static const char * const events_log;
static const char * const pipes_log;
static const char * const log_received_all_started_fmt;
static const char * const log_received_all_done_fmt;
static const char * const log_done_fmt;
static const char * const log_started_fmt;

static char first_bffr[10] = "";//creating a buffer bffr/buffer
static char second_bffr[10]; // creating a second buffer buff
static int fd[12][12][2]; //file descriptor
MessageType lrm[13]; //last message that was recieved by a process
static int closed_pipes[12][12] = {{0}}; //Describing status of a pipe beetwen process i and j. 0 - closed, 1 - onlu writing and 2 - only reading
static int big_pp_2;
static int num_pipes = 0;

static const char * const pipe_closed_read_from_for =
    "Pipe closed in process %d for reading into %d process from %d process.\n";
    
static const char * const pipe_closed_write_from_into =
    "Pipe closed in process %d for writing from %d process into %d process.\n";

static const char * const pipe_opened =
    "Pipes opened for writing/reading %d.\n";

//structure to work with our send/recieve funcs
struct Processs{
    local_id id;
    pid_t pid;
    pid_t parent_pid;
    int child_num;
    int parallel_pro;
};

//closing pipes
void close_pipes(local_id l_id, int child_num){
    for(int i = 0; i <= child_num; i++){
        for(int j = 0; j <= child_num; j++){
            if(i != j){
                if(closed_pipes[i][j] == 2 || closed_pipes[i][j] == 0){
                    if(close(fd[i][j][0] != 0)) {exit(33);}
                    closed_pipes[i][j] += 1;
                    //log info
                    sprintf(second_bffr, pipe_closed_read_from_for, l_id, i, j);
                    write(big_pp_2, second_bffr, strlen(second_bffr));
                }
                if(closed_pipes[i][j] == 1 || closed_pipes[i][j] == 0){
                    if(close(fd[i][j][1]) != 0) {exit(34);}
                    closed_pipes[i][j] += 2;
                    //log info
                    sprintf(second_bffr, pipe_closed_write_from_into, l_id, i, j);
                    write(big_pp_2, second_bffr, strlen(second_bffr));
                }
            }
        }
    }

}

//send implementation
int send(void * self, local_id dst, const Message * msg) {
    struct Processs *sender = (struct Processs *)self;
    int wfd = fd[sender->id][dst][1];
    if (write(wfd, msg, sizeof(MessageHeader) + (msg->s_header.s_payload_len)) == -1) {perror("problem on send\n"); return -1;}
    return 0;
}

//send_all implementation
int send_multicast(void * self, const Message * msg){
    struct Processs *sender = (struct Processs *)self;
    int senders = sender->parallel_pro+1;
    if (sender->child_num != 0) {senders = sender->child_num;}
    for(local_id i = 0; i <= senders; i++){
        if(send(sender, i, msg) == -1){perror("problem on send_all\n");}
    }
    return 0;
}

//recieve implementation
int receive(void * self, local_id from, Message * msg) {
    struct Processs *receiver = (struct Processs *)self;
    if (receiver->id == from) {perror("error on recieve\n"); return -1;}
    int read_fd = fd[from][receiver->id][0];
    if (read(read_fd, msg, sizeof(MessageHeader)) == -1) {perror("error on recieve\n"); return -1;}
    lrm[from] = msg->s_header.s_type;
    if (read(read_fd, msg, msg->s_header.s_payload_len) == -1) {perror("error on reading tail\n"); return -1;}
    return 0;
}

//recicve any implementation
int receive_any(void * self, Message * msg);

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

int work(struct Processs *parent, struct Processs *child){
    sprintf(first_bffr, log_done_fmt, child->id);
    printf(log_done_fmt, child->id);
    return 0;
}

//syncronize after work
int after_work(struct Processs *parent, struct Processs *child){
    Message done_message = create_msg(DONE, first_bffr);
    if (send_multicast(child, &done_message) != 0) {perror("send_all DONE msg problem\n"); return 1;}
    int z = 0;
    while (z <= 999) {
        int recipients = child->parallel_pro+1;
        for (int i = 1; i <= recipients; i++) {
            if (child->id != i) {
                if (receive(child, i, &done_message) != 0) {
                    return -1;
                }
            }
        }
        z++;
        int rd = 0;
        for (int i = 1; i <= (child->parallel_pro)+1; i++) {
            if (lrm[i] == DONE && i != child->id)
                rd++;
        }
        if (rd == child -> parallel_pro) {
            sprintf(first_bffr, log_received_all_done_fmt, child->id);
            printf(log_received_all_done_fmt, child->id);
            return 0;
        }
    }
    return -1;
}

int ready_to_end(struct Processs *parent){
    Message done_msg = create_msg(DONE, first_bffr);
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
        if (rd == parent->child_num) {
            return 0;
            }
        }
    return -1;
}

//Synchronizing before doing work
int ready_to_go(struct Processs *prop, int recievers){//sending started msg to all processes
    Message msg = create_msg(STARTED, "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP");
    int z = 0;
    if(prop->parent_pid != -1){if (send_multicast(prop, &msg) != 0){perror("Sendmulticast in read_to_go error\n"); return 1;}}
    while (z <= 999 ){
        for (int i = 1; i <= recievers; i++) {
            if (prop->id != i) {
                if (receive(prop, i, &msg) != 0) {return -1;}
            }
        
        z++;
        int rs = 0;
        for (int i = 1; i <= recievers; i++) {if (lrm[i] == STARTED && i != prop->id) {rs++;}}

        if ((rs == recievers && prop->parent_pid == -1)) {return 0;}
        else {if (rs == recievers - 1 && prop->parent_pid != -1) {
            sprintf(first_bffr, log_received_all_started_fmt, prop->id);
            printf(log_received_all_started_fmt, prop->id );
            return 0;
        }}
    }
    }
    return -1;
}

int main(int argc, char *argv[]) {

    //filling array with deafault STOP
    for (int i = 0; i < 16; i++) {
        lrm[i] = STOP;
    }

    //check parameters
    int child_num;
    if (argc != 3 || strcmp(argv[1], "-p") != 0 || atoi(argv[2]) == 0){
        exit(8);
    }
    else{
        if (atoi(argv[2]) > 9 || atoi(argv[2]) <= PARENT_ID){
            exit(9);
        }   
        else
            child_num = atoi(argv[2]);
    }

    //creating and opening files
    int events_file = open(events_log, O_WRONLY | O_APPEND | O_CREAT);
    int pipes_file = open(pipes_log, O_WRONLY | O_APPEND | O_CREAT); 
    if (events_file == -1)
        exit(10);
    if (pipes_file == -1)
        exit(11);

    //open pipes
    dup2(pipes_file, big_pp_2);
    for (int i = 0; i < (child_num + 1); i++){
        for (int j = 0; j < (child_num + 1); j++){
            if (i != j){ if(pipe(fd[i][j]) == -1) {perror("opening pipes"); exit(4);} num_pipes+=2;}
        }
    }
    //log info for pipes
    sprintf(second_bffr, pipe_opened, num_pipes);
    write(big_pp_2, second_bffr, strlen(second_bffr));

    close(pipes_file);

    //creating proccesses structures
    struct Processs parent;
    struct Processs child;
    processs_parent(&parent, getpid(), child_num);

    //forking proccesses
    int internal_id = 0;
    int fid = 1;
    for (int i = 0; i < child_num; i++){
        if (fid != 0){internal_id++; fid = fork();}

        if ((fid == 0) && (i + 1 == internal_id)){processs_child(&child, internal_id, getpid(), parent.pid, child_num);}
    }

    if(fid != 0){internal_id = 0;}
    
    //close unnecessary pipes
    for (int i = 0; i < (child_num + 1); i++){
        for (int j = 0; j < (child_num + 1); j++){
            if(i != j){
            if (internal_id != i) {close(fd[i][j][1]); closed_pipes[i][j] += 2; num_pipes--; sprintf(first_bffr, pipe_closed_write_from_into, internal_id, i, j); write(big_pp_2, first_bffr, strlen(first_bffr));} 
            if (internal_id != j) {close(fd[i][j][0]); closed_pipes[i][j] += 1; num_pipes--; sprintf(first_bffr, pipe_closed_read_from_for, internal_id, i, j); write(big_pp_2, first_bffr, strlen(first_bffr));}
            }
        }
    }

    if (fid == 0){

        //write log
        write(events_file, first_bffr, strlen(first_bffr));
        
        //synchronize before work and write log
        if (ready_to_go(&child, child_num) == 0) {write(events_file, first_bffr, strlen(first_bffr));} 
        else {exit(12);}

        //do work and log it
        if (work(&parent, &child) == 0) {write(events_file, first_bffr, strlen(first_bffr));}
        else {exit(13);}
        
        //synchronize after work and write log
        if (after_work(&parent, &child) == 0) {write(events_file, first_bffr, strlen(first_bffr));}
        else {exit(14);}
        
        //closing pipes
        close_pipes(child.id, child_num);

        //close files
        close(events_file);
        close(big_pp_2);

        exit(0);
    }
    else{
        
        int a;

        //sinchronization
        while (ready_to_go(&parent, child_num) != 0) {continue;}

        //waiting for finishing of children processes
        while (child_num != 0) {if ((a = waitpid(-1, NULL, WNOHANG)) <= 0)child_num--;}

        //waiting for done messages
        while(ready_to_end(&parent) != 0) {continue;}

        //close pipes
        close_pipes(parent.id, child_num);
        
        //close files
        close(events_file);
        close(big_pp_2);
    } 
    return 0;
}
