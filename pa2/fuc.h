#include "unistd.h"
#include "stdlib.h"
#include "puk.h"

void clear_descriptors(int *fd, int size) {
	for (int i = 0; i < size; ++i) close(fd[i]);

	free(fd);
}

int history(AllHistory * his, balance_t * balance, int child_num){

	int max_history_len = 0;
		
		for (int i = 0; i < child_num; ++i) {
			if (his->s_history[i].s_history_len > max_history_len)
				max_history_len = his->s_history[i].s_history_len;
		}

		for (int i = 0; i < child_num; ++i) {
			balance = &his->s_history[i].s_history[his->s_history[i].s_history_len-1].s_balance;
			for (int j = his->s_history[i].s_history_len; j < max_history_len; ++j) {
				his->s_history[i].s_history[j].s_balance = *balance;
				his->s_history[i].s_history[j].s_time = j;
			}
			his->s_history[i].s_history_len = max_history_len;
		}
		print_history(his);
	return 0;
}

int check_on_receive(Processs *Processs, MessageType type) {
	Message msg;
	int status = receive_any(Processs, &msg);

	if ((status) || (msg.s_header.s_magic != MESSAGE_MAGIC) || (msg.s_header.s_type != type)) {
		return 1;
	}

	return 0;
}

int check_on_receive_pa2(Processs *Processs, MessageType type1, MessageType type2) {
	Message msg;
	int status = receive_any(Processs, &msg);

	if ((status) || (msg.s_header.s_magic != MESSAGE_MAGIC) || (msg.s_header.s_type != type1 && msg.s_header.s_type != type2)) {
		return 1;
	}

	return 0;
}

int msg_init(Message * msg, int len, MessageType type, timestamp_t time) {
	msg->s_header.s_magic = MESSAGE_MAGIC;
	msg->s_header.s_payload_len = len;
	msg->s_header.s_type = type;
	msg->s_header.s_local_time = time;

	return 0;
}

int msg_init_notime(Message * msg, int len, MessageType type){
	msg->s_header.s_magic = MESSAGE_MAGIC;
	msg->s_header.s_payload_len = len;
	msg->s_header.s_type = type;

	return 0;
}

int his_inti(BalanceHistory * history, int i, balance_t ball_old){
		history->s_history[i].s_balance = ball_old;
		history->s_history[i].s_time = i;
		history->s_history[i].s_balance_pending_in = 0;

		return 0;
}

int his_init(BalanceHistory * history, int id, timestamp_t time, balance_t ball){
	history->s_id = id;
	history->s_history_len = time + 1;

	for (int i = 0; i < time; ++i) {
		if(his_inti(history, i, 0) != 0){exit(99);}
	}

	history->s_history[time].s_balance = ball;
	history->s_history[time].s_time = time;
	history->s_history[time].s_balance_pending_in = 0;

	return 0;
}

int close_pipes(int child_num, FILE * pipes, int * fd, int id){
	for (int i = 0; i < child_num*(child_num+1); ++i) {
		int x = i / child_num;
		int y = i + 1 - x * child_num;

		if (x == y) {
			y = PARENT_ID;
			}

		if (x != id) {
			close(fd[2*i+1]);
			fprintf(pipes, "[Closed for process %d descriptor for sending messages to process %d by process %d]\n", id, y, x);
		}

		if (y != id) {
			close(fd[2*i]);
			fprintf(pipes, "[Closed for process %d descriptor for receiving messages from process %d by process %d]\n", id, x, y);
		}
	}
	return 0;
}
