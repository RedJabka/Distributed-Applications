#include "common.h"
#include "ipc.h"
#include "pa2345.h"
#include "banking.h"
#include "fuc.h"

static int child_num, *fd;
static FILE *events, *pipes;
static int id = PARENT_ID;

int flose(){
	fclose(events);
	clear_descriptors(fd, 2*child_num*(child_num+1));
	fprintf(pipes, "[Closed for process %d all descriptors]\n", id);
	fclose(pipes);

	return 0;
}

int open_pipes(){

	for (int i = 0; i < child_num*(child_num+1); ++i) {
		if (pipe(fd+2*i)) {
			perror("[Error creating pipe]\n");
			clear_descriptors(fd, 2*child_num*(child_num+1));
			fprintf(pipes, "[Closed for all processes all descriptors]\n");
			return 5;
		}
		if (fcntl(fd[2*i], F_SETFL, fcntl(fd[2*i], F_GETFL, 0) | O_NONBLOCK)) {
			perror("[Error making descriptor for reading non-blocking]\n");
			clear_descriptors(fd, 2*child_num*(child_num+1));
			fprintf(pipes, "[Closed for all processes all descriptors]\n");
			return 6;
		}
		if (fcntl(fd[2*i+1], F_SETFL, fcntl(fd[2*i+1], F_GETFL, 0) | O_NONBLOCK)) {
			perror("[Error making descriptor for writing non-blocking]\n");
			clear_descriptors(fd, 2*child_num*(child_num+1));
			fprintf(pipes, "[Closed for all processes all descriptors]\n");
			return 7;
		}
		int f = i / child_num;
		int t = i + 1 - f * child_num;
		if (f == t) t = PARENT_ID;
		fprintf(pipes, "[Opened for all processes non-blocking descriptor for receiving messages from process %d by process %d]\n", f, t);
		fprintf(pipes, "[Opened for all processes non-blocking descriptor for sending messages to process %d by process %d]\n", t, f);
	}
	fclose(pipes);
	return 0;
	}

int give_birth_to_children(Processs parent){
		for (int i = 0; i < child_num; ++i) {
			while (check_on_receive(&parent, STARTED)) {}
		}
		fprintf(events, log_received_all_started_fmt, get_physical_time(), id);
		bank_robbery(&parent, child_num);

		Message stop;
		if(msg_init(&stop, 0, STOP, get_physical_time() != 0)){exit(99);}
		
		if (send_multicast(&parent, &stop)) {
			perror("[Parent][Error sending STOP message]\n");
			if(flose() != 0){exit(9);}
			return 9;
		}
		return 0;
	}

void transfer(void * parent_data, local_id from, local_id to, balance_t amnt) {
	Processs *parent = (Processs*)parent_data;

	TransferOrder order;
	order.s_src = from;
	order.s_dst = to;
	order.s_amount = amnt;

	Message tsf;
	if(msg_init(&tsf, sizeof(order), TRANSFER, get_physical_time()) != 0){exit(93);}

	const char *payload = (const char*)&order;

	for (int i = 0; i < sizeof(order); ++i) {
		tsf.s_payload[i] = payload[i];
	}

	if (send(parent, from, &tsf)) {
		fprintf(stderr, "[Parent][Error sending TRANSFER message to child %d]\n", from);
	}

	while (check_on_receive_pa2(parent, ACK, TRANSFER)) {
		continue;
	}
}

int main(int argc, char **argv) {
	if (argc < 3 || strcmp(argv[1], "-p") || atoi(argv[2]) <= 0 || atoi(argv[2]) > MAX_PROCESS_ID || argc < 3 + atoi(argv[2])) {
		perror("[Usage: pa2 -p N B_1 B_2 B_3 ... B_N]\n");
		exit(1);
	}

	child_num = atoi(argv[2]);
	fd = malloc(2*child_num*(child_num + 1)*sizeof(int));
	pipes = fopen(pipes_log, "w");

	events = fopen(events_log, "w");
	fclose(events);

	if(open_pipes() != 0) {exit(77);}

	events = fopen(events_log, "a");
	for (int i = 0; i < child_num; ++i) {
		pid_t pp = fork();
		if (pp == 0) {
			id = i + 1;
			break;
		} else if (pp < 0) {
			fprintf(stderr, "[Error creating child %d]\n", i + 1);
			clear_descriptors(fd, 2*child_num*(child_num+1));
			pipes = fopen(pipes_log, "a");
			fprintf(pipes, "[Closed for process %d all descriptors]\n", id);
			fclose(pipes);
			return 8;
		}
	}

	pipes = fopen(pipes_log, "a");
	if(close_pipes(child_num, pipes, fd, id) != 0){exit(32);}

	Processs parent;
	if(proc_init(&parent, id, child_num, fd) != 0){exit(87);}
	
	if (id == PARENT_ID) {
		give_birth_to_children(parent);

		AllHistory his;
		his.s_history_len = child_num;
		for (int i = 0; i < 2*child_num; ++i) {
			Message msg;
			while (receive_any(&parent, &msg)) {}
			if (msg.s_header.s_magic != MESSAGE_MAGIC) {
				perror("[Parent][Message is broken, invalid magic]\n");
				if(flose() != 0){exit(10);}
			}
			if (msg.s_header.s_type == BALANCE_HISTORY) {
				BalanceHistory *balhist = (BalanceHistory*)msg.s_payload;
				his.s_history[balhist->s_id-1] = *balhist;
			}
		}

		fprintf(events, log_received_all_done_fmt, get_physical_time(), id);


		balance_t blnc;
		if(history(&his, &blnc, child_num) != 0){exit(54);}

		for (int i = 0; i < child_num; ++i) wait(NULL);
	} else {
		balance_t blnc = atoi(argv[2+id]);

		if (blnc < 0) {
			fprintf(stderr, "[Child %d][Balance cannot be negative, %d provided]\n", id, blnc);
			if(flose() != 0){exit(11);}
		} 
		
		timestamp_t time = get_physical_time();
		BalanceHistory balhist;
		Message started;

		if(his_init(&balhist, id, time, blnc) != 0){exit(75);}

		if(msg_init(&started, 0, STARTED, time) != 0){exit(75);}

		if (send(&parent, PARENT_ID, &started)) {
			fprintf(stderr, "[Child %d][Error sending STARTED message]\n", id);
			if(flose() != 0){exit(12);}
		}

		fprintf(events, log_started_fmt, time, id, getpid(), getppid(), blnc);

		int no_stop = 1;

		while (no_stop) {
			Message msg;
			while (receive_any(&parent, &msg)) {}
			if (msg.s_header.s_magic != MESSAGE_MAGIC)
				fprintf(stderr, "[Child %d][Message is broken, invalid magic]\n", id);
			switch (msg.s_header.s_type) {
				TransferOrder *order;
			case TRANSFER:
				order = (TransferOrder*)msg.s_payload;
				local_id source = order->s_src;
				local_id destination = order->s_dst;
				balance_t amnt = order->s_amount;
				time = get_physical_time();
				msg.s_header.s_local_time = time;
				
				if (source == id && destination != id) {
					if (blnc < amnt) {
						fprintf(stderr, "[Child %d][Not enough money][Balance is %d, trying to transfer %d]\n", id, blnc, amnt);
						if (send(&parent, PARENT_ID, &msg)) {
							fprintf(stderr, "[Child %d][Error sending TRANSFER message to parent]\n", id);
							if(flose() != 0){exit(13);}
						}
					} else {
						balance_t prev_bal = blnc;
						blnc -= amnt;
						if (send(&parent, destination, &msg)) {
							fprintf(stderr, "[Child %d][Error sending TRANSFER message to child %d]\n", id, destination);
							if(flose() != 0){exit(14);}
						}

						fprintf(events, log_transfer_out_fmt, time, id, amnt, destination);

						for (int i = balhist.s_history_len; i < time; ++i) {
							if(his_inti(&balhist, i, prev_bal) != 0){exit(88);}
						}


						if(his_inti(&balhist, time, blnc) != 0){exit(58);}
						balhist.s_history_len = time + 1;
					}
				} else if (source != id && destination == id) {
					balance_t prev_bal = blnc;
					blnc += amnt;

					Message ack;
					if(msg_init(&ack, 0, ACK, time) != 0){exit(97);}

					if (send(&parent, PARENT_ID, &ack)) {
						fprintf(stderr, "[Child %d][Error sending ACK message]\n", id);
						if(flose() != 0){exit(15);}
					}

					fprintf(events, log_transfer_in_fmt, time, id, amnt, source);

					for (int i = balhist.s_history_len; i < time; ++i) {
						if(his_inti(&balhist, i, prev_bal) != 0){exit(88);}
					}

					if(his_inti(&balhist, time, blnc) != 0){exit(58);}
					balhist.s_history_len = time + 1;
				}
				break;
			case STOP:
				no_stop = 0;
				break;
			default:
				break;
			}
		}
		Message done;
		if(msg_init_notime(&done, 0, DONE) != 0){exit(96);}

		if (send(&parent, PARENT_ID, &done)) {
			fprintf(stderr, "[Child %d][Error sending DONE message]\n", id);
			if(flose() != 0){exit(16);}
		}
		fprintf(events, log_done_fmt, get_physical_time(), id, blnc);

		Message blmsg;
		if(msg_init_notime(&blmsg, sizeof(balhist.s_id) + sizeof(balhist.s_history_len) + sizeof(BalanceState) * balhist.s_history_len, BALANCE_HISTORY) != 0){exit(94);}

		const char *payload = (const char*)(&balhist);

		for (int i = 0; i < blmsg.s_header.s_payload_len; ++i) blmsg.s_payload[i] = payload[i];

		if (send(&parent, PARENT_ID, &blmsg)) {
			fprintf(stderr, "[Child %d][Error sending BALANCE_HISTORY message]\n", id);
			if(flose() != 0){exit(17);}
		}
	}
	
	if(flose() != 0){exit(18);}

	return 0;
}
