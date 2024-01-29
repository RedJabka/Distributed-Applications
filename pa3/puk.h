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

int send(void * self, local_id to, const Message * msg) {
	Processs *sender = (Processs*)self;
	int p1pe_numbers = 0;

	if (to == sender->id) {
		return -1;
	}

	if (to == PARENT_ID) {
		p1pe_numbers = sender->id * (sender->child_num + 1);
	}
	else {
		p1pe_numbers = sender->id * sender->child_num + to;
	}

	return write(sender->descriptors[2*p1pe_numbers-1], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) != sizeof(MessageHeader) + msg->s_header.s_payload_len;
}

int send_multicast(void * self, const Message * msg) {
	Processs *sender = (Processs*)self;
	int p1pe_numbers = sender->id * sender->child_num;
	int size = sizeof(MessageHeader) + msg->s_header.s_payload_len;
	
	for (int i = 0; i < sender->child_num; ++i) {
		if (write(sender->descriptors[2*(p1pe_numbers+i)+1], msg, size) != size) {
			return 1;
		}
	}
	
	return 0;
}

int receive(void * self, local_id from, Message * msg) {
	Processs *sender = (Processs*)self;
	int p1pe_numbers = 0;
	if (from == sender->id) {
		return -1;
	}

	if (sender->id == PARENT_ID) {
		p1pe_numbers = from * (sender->child_num + 1);
	}
	else {
		p1pe_numbers = from * sender->child_num + sender->id;
	}

	if (read(sender->descriptors[2*p1pe_numbers-2], msg, sizeof(MessageHeader)) < (int)sizeof(MessageHeader)) {
		return 1;
	}
	else{
		return (read(sender->descriptors[2*p1pe_numbers-2], msg->s_payload, msg->s_header.s_payload_len) < msg->s_header.s_payload_len) * 2;
	}
}

int receive_any(void * self, Message * msg) {
	Processs *sender = (Processs*)self;

	for (int i = 0; i <= sender->child_num; ++i) {
		if (i != sender->id) {
			if (receive(sender, i, msg) == 0) return 0;
		}
	}

	return 1;
}
