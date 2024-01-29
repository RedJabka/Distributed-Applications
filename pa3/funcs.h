#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "ipc.h"
#include "pa2345.h"
#include "banking.h"

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
//static int num_pipes = 0;

static const char * const pipe_closed_read_from_for =
    "Pipe closed in process %d for reading into %d process from %d process.\n";
    
static const char * const pipe_closed_write_from_into =
    "Pipe closed in process %d for writing from %d process into %d process.\n";

//static const char * const pipe_opened =
  //  "Pipes opened for writing/reading %d.\n";

//structure to work with our send/recieve funcs
struct Processs{
    local_id id;
    pid_t pid;
    pid_t parent_pid;
    int child_num;
    int parallel_pro;
    BalanceHistory acc_history;
    BalanceState acc_balance;
};

int unblocked_pipes(int file_descriptor){
    int flag = fcntl(file_descriptor, F_GETFL);

    if (flag == -1) {perror("unblovking pipes error"); return -1;}
    if (fcntl(file_descriptor, F_SETFL, flag | O_NONBLOCK) == -1) {perror("unblovking pipes error"); return -1;}

    return 0;
}

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

    if (sender->child_num != 0) senders = sender->child_num;

    for(local_id i = 0; i <= senders; i++){
        if(send(sender, i, msg) == -1) perror("problem on send_all\n"); 
    }
    return 0;
}

//recieve implementation
int receive(void * self, local_id from, Message * msg) {
    struct Processs *receiver = (struct Processs *)self;
    MessageHeader head;

    if (receiver->id == from) {perror("error on receive\n"); return -1;}
    int read_fd = fd[from][receiver->id][0];

    if (read(read_fd, &head, sizeof(MessageHeader)) < (int)sizeof(MessageHeader)) {perror("error on recieve\n"); return -1;}
    msg->s_header = head;

    if (read(read_fd, msg->s_payload, msg->s_header.s_payload_len) < msg->s_header.s_payload_len) {perror("error on reading tail\n"); return -1;}
    return 0;
}

//recicve any implementation
int receive_any(void * self, Message * msg){
    struct Processs *reciever = (struct Processs *)self;
    int num_recievers;

    if (reciever->child_num == 0) num_recievers = reciever->parallel_pro + 1;
    else num_recievers = reciever->child_num;

    for(int i = 0; i <= num_recievers; i++){
        if (reciever->id != i) {printf("receive anny"); if(receive(reciever, i, msg) == 0) return 0;}
    }
    return 1;
}

Message create_msg(MessageType msgType, const char *msg) {
    Message message;

    message.s_header.s_magic = MESSAGE_MAGIC;
    message.s_header.s_type = msgType;
    message.s_header.s_payload_len = strlen(msg);
    message.s_header.s_local_time = get_physical_time();
    
    memset(message.s_payload, 0, sizeof(message.s_payload));
    strncpy(message.s_payload, msg, sizeof(message.s_payload));
    return message;
}

Message create_msg_pa2(MessageType msgType, void* msg, size_t payload){

    Message message;
    message.s_header.s_magic = MESSAGE_MAGIC;
    message.s_header.s_payload_len = payload;
    message.s_header.s_type = msgType;

    message.s_header.s_local_time = get_physical_time();

    memset(message.s_payload, 0, sizeof(message.s_payload));
    memset(message.s_payload, *((int *)msg), sizeof(message.s_payload));

    return message;
}

int work(struct Processs *parent, struct Processs *child){
    Message msg = create_msg(ACK, "");
    BalanceState stateAtMoment;

    while(msg.s_header.s_type != STOP){

        msg = create_msg(ACK, "");
        receive_any(child, &msg);

        if(msg.s_header.s_type == TRANSFER){

            TransferOrder transferOrder;
            memcpy(&transferOrder, msg.s_payload, sizeof(transferOrder));
            stateAtMoment.s_balance = -1;
            stateAtMoment.s_time = -1;

            if(child->id == transferOrder.s_src) {
                child->acc_balance.s_balance -= transferOrder.s_amount;
                msg = create_msg_pa2(TRANSFER, &transferOrder, sizeof(TransferOrder));

                send(child, transferOrder.s_dst, &msg);
                stateAtMoment.s_time = get_physical_time();
                stateAtMoment.s_balance = child->acc_balance.s_balance;
                child->acc_history.s_history[stateAtMoment.s_time] = stateAtMoment;

                sprintf(first_bffr, log_transfer_out_fmt, stateAtMoment.s_time, transferOrder.s_src, transferOrder.s_amount, transferOrder.s_dst);
                printf(first_bffr, log_transfer_out_fmt, stateAtMoment.s_time, transferOrder.s_src, transferOrder.s_amount, transferOrder.s_dst);
                //write(events_log);
            }
        }
    }
    if(stateAtMoment.s_time == -1) child->acc_history.s_history_len = MAX_T;
    else child->acc_history.s_history_len = stateAtMoment.s_time + 1;

    sprintf(first_bffr, log_done_fmt, get_physical_time(), child->id, child->acc_balance.s_balance);
    return 0;
}

//syncronize after work
int after_work(struct Processs *parent, struct Processs *child){
    Message done_message = create_msg(DONE, first_bffr);
    if (send_multicast(child, &done_message) != 0) {perror("send_all DONE msg problem\n"); return 1;}

    int z = 0;
    while (z <= 9999) {
        int recipients = child->parallel_pro + 1;
        for (int i = 1; i <= recipients; i++) {
            if (child->id != i && lrm[i] != DONE) { 
                printf("aftre work");
                if (receive(child, i, &done_message) != 0) break;
                if(done_message.s_header.s_type == DONE) lrm[i] = DONE;

            }
        }
        z++;
        int rd = 0;
        for (int i = 1; i <= (child->parallel_pro) + 1; i++) {
            if (lrm[i] == DONE && i != child->id) rd++;
        }
        if (rd == child->parallel_pro) {
           // sprintf(first_bffr, log_received_all_done_fmt, child->id);
            //printf(log_received_all_done_fmt, child->id);
            return 0;
        }
    }
    return -1;
}

int send_acc_history(struct Processs *child){
    for(timestamp_t i = 1; i < child->acc_history.s_history_len; i++){
        if(child->acc_history.s_history[i].s_time == -1) {
            child->acc_history.s_history[i].s_time = i;
            child->acc_history.s_history[i].s_balance = child->acc_history.s_history[i - 1].s_balance;
        }
        child->acc_history.s_history[i].s_balance_pending_in = 0;
    }

    Message msg = create_msg_pa2(BALANCE_HISTORY, &(child->acc_history), sizeof(BalanceHistory));
    send(child, PARENT_ID, &msg);
    return 0;
}

int ready_to_end(struct Processs *parent){
    Message done_msg = create_msg(DONE, first_bffr);
    int z = 0;
    while (z <= 999) {
        int recievers = parent->child_num;
        for (int i = 1; i <= recievers; i++) {
            if (parent->id != i && lrm[i] != DONE) { 
                printf("ready_to_end");
                if (receive(parent, i, &done_msg) != 0) break;
                if(done_msg.s_header.s_type == DONE) lrm[i] = DONE;
            }
        }

        z++;
        int rd = 0;
        for (int i = 1; i <= (parent->child_num); i++) {if (lrm[i] == DONE && i != PARENT_ID) rd++;}

        if (rd == parent->child_num) return 0;
    }
    return -1;
}

//Synchronizing before doing work
int ready_to_go(struct Processs *prop, int recievers){//sending started msg to all processes
    Message msg = create_msg(STARTED, first_bffr);
    int z = 0;
    if(prop->parent_pid != -1){if (send_multicast(prop, &msg) != 0){perror("Sendmulticast in ready_to_go error\n"); return 1;}}
    while (z <= 999 ){
        for (int i = 1; i <= recievers; i++) {
            if (prop->id != i) {
                printf("ready_to_go");
                if (receive(prop, i, &msg) != 0) {return -1;}
            }
        
            z++;
            int rs = 0;
            for (int i = 1; i <= recievers; i++) {if (lrm[i] == STARTED && i != prop->id) {rs++;}}

            if (rs == prop->parallel_pro) { 
                sprintf(first_bffr, log_received_all_started_fmt, get_physical_time(), prop->id);
                printf(log_received_all_started_fmt, get_physical_time(), prop->id);
                write(big_pp_2, first_bffr, strlen(first_bffr));
                return 0;
            }
        }
    }
    return -1;
}

int recieve_history(struct Processs *parent){
    Message done_msg = create_msg(DONE, first_bffr);
    int z = 0;
    int len = 0;
    AllHistory all_acc_history;

    all_acc_history.s_history_len = parent->child_num;

    while(z <= 999){
        int recipients = parent->child_num;

        for(int i = 1; i < recipients + 1; i++) {
            if(parent->id != i && lrm[i] != BALANCE_HISTORY){
                printf("history");
                if(receive(parent, i, &done_msg) != 0) break;

                if(done_msg.s_header.s_type == BALANCE_HISTORY){
                    lrm[i] = BALANCE_HISTORY;
                    BalanceHistory atMoment;

                    memcpy(&atMoment, done_msg.s_payload, sizeof(BalanceHistory));
                    all_acc_history.s_history[atMoment.s_id - 1] = atMoment;

                    if(atMoment.s_history_len > len) len = atMoment.s_history_len;
                }
            }
        }

        
        z++;
        int rd = 0;

        for (int i = 1; i < parent->child_num + 1; i++){if(lrm[i] == BALANCE_HISTORY && i != PARENT_ID) rd ++;}

        if(rd == parent->child_num){
            //reformat history
            for(int i = 0; i < all_acc_history.s_history_len; i++){
                if(all_acc_history.s_history[i].s_history_len < len) {
                    for(int j = all_acc_history.s_history[i].s_history_len; j <= len; j++){all_acc_history.s_history[i].s_history[j].s_time = j; all_acc_history.s_history[i].s_history[j].s_balance = all_acc_history.s_history[i].s_history[j-1].s_balance; }
                    all_acc_history.s_history[i].s_history_len = len;
                }
            }

            //print out history
            print_history(&all_acc_history);
            return 0;
        }
    }

    return -1;
}
