#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "banking.h"
#include "common.h"
#include "ipc.h"
#include "pa2345.h"

static timestamp_t lamport_time = 0;

timestamp_t get_lamport_time() {
	return lamport_time++;
}

typedef struct {
	int id; 
	int child_num;
	int *descriptors;
} Processs;

int proc_init(Processs * self, int id, int child_num, int *descriptors){
	self->id = id;
	self->child_num = child_num;
	self->descriptors = descriptors;
	return 0;
}

typedef struct {
	int c_id; 
	int child_num; 
	int active_transfers;
	int *descriptors;
} Condition;

int cond_init(Condition * self, int id, int child_num, int trans, int *descriptors){
	self->c_id = id;
	self->child_num = child_num;
	self->active_transfers = trans;
	self->descriptors = descriptors;
	return 0;
}

int send(void * self, local_id to, const Message * msg) {
	Condition *sender = (Condition*)self;
	int p1pe_numbers = 0;
	if (to == sender->c_id) {
		return -1;
		}

	if (to == PARENT_ID) {
		p1pe_numbers  = sender->c_id * (sender->child_num + 1);
		}
	else {
		p1pe_numbers  = sender->c_id * sender->child_num + to;
		}

	int size = sizeof(MessageHeader) + msg->s_header.s_payload_len;

	int written = write(sender->descriptors[2*p1pe_numbers -1], msg, size);

	return written != size;
}


int send_multicast(void * self, const Message * msg) {
	Condition *sender = (Condition*)self;
	int p1pe_numbers = sender->c_id * sender->child_num;
	int size = sizeof(MessageHeader) + msg->s_header.s_payload_len;
	
	for (int i = 0; i < sender->child_num; ++i) {
		if (write(sender->descriptors[2*(p1pe_numbers+i)+1], msg, size) != size) {
			return 1;
		}
	}
	
	return 0;
}

int receive(void * self, local_id from, Message * msg) {
	Condition *sender = (Condition*)self;
	int p1pe_numbers = 0;
	if (from == sender->c_id) {
		return -1;
	}

	if (sender->c_id == PARENT_ID) {
		p1pe_numbers = from * (sender->child_num + 1);
	}
	else {
		p1pe_numbers = from * sender->child_num + sender->c_id;
	}

	if (read(sender->descriptors[2*p1pe_numbers-2], msg, sizeof(MessageHeader)) < (int)sizeof(MessageHeader)) {
		return 1;
	}
	
	if (read(sender->descriptors[2*p1pe_numbers-2], msg->s_payload, msg->s_header.s_payload_len) < msg->s_header.s_payload_len) {
		 return 2;
	}

	if (msg->s_header.s_local_time > lamport_time) {
		lamport_time = msg->s_header.s_local_time;
	}

	++lamport_time;
	return 0;

}

int receive_any(void * self, Message * msg) {
	Condition *sender = (Condition*)self;

	for (int i = 0; i <= sender->child_num; ++i) {
		if (i != sender->c_id) {
			if (receive(sender, i, msg) == 0) {
				return 0;
			}
		}
	}

	return 1;
}
