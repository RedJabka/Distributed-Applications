#include "unistd.h"
#include "stdlib.h"
#include "puk.h"

void clear_descriptors(int *fd, int size) {
	for (int i = 0; i < size; ++i) close(fd[i]);

	free(fd);
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
