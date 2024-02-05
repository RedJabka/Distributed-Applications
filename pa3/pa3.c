#include "common.h"
#include "ipc.h"
#include "pa2345.h"
#include "banking.h"
#include "fuc.h"

void print_history(const AllHistory * history);

static int child_num, *fd;
static FILE *events, *pipes;
static int id = PARENT_ID;


void transfer(void * parent_data, local_id from, local_id to, balance_t amount) {
	Condition *parent = (Condition*)parent_data;

	TransferOrder order;
	order.s_src = from;
	order.s_dst = to;
	order.s_amount = amount;

	Message tsf;
	if(msg_init(&tsf, sizeof(order), TRANSFER, get_lamport_time()) != 0){exit(93);}
	
	const char *payload = (const char*)&order;

	for (int i = 0; i < sizeof(order); ++i) {
		tsf.s_payload[i] = payload[i];
	}


	if (send(parent, from, &tsf)) {
		fprintf(stderr, "[Parent][Error sending TRANSFER message to child %d]\n", from);
	}

	Message result;
	while (receive_any(parent, &result)) {}

	if (result.s_header.s_magic != MESSAGE_MAGIC) perror("[Parent][Message is broken, invalid magic]\n");
	if (result.s_header.s_type != ACK && result.s_header.s_type != TRANSFER) perror("[Parent][Message is broken, invalid type]\n");
}

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

int give_birth_to_children(Condition cond){
		for (int i = 0; i < child_num; ++i) {
			Message msg;
			while (receive_any(&cond, &msg)) {}
			if (msg.s_header.s_magic != MESSAGE_MAGIC || msg.s_header.s_type != STARTED) {
				if(flose() != 0){exit(10);}
				exit(11);
			}
		}

		fprintf(events, log_received_all_started_fmt, get_lamport_time(), id);
		bank_robbery(&cond, child_num);

		Message stop;
		if(msg_init(&stop, 0, STOP, get_lamport_time() != 0)){exit(99);}
		
		if (send_multicast(&cond, &stop)) {
			if(flose() != 0){exit(12);}
			exit(13);
		}

		return 0;
}


int main(int argc, char **argv) {
	if (argc < 3 || strcmp(argv[1], "-p") || atoi(argv[2]) <= 0 || atoi(argv[2]) > MAX_PROCESS_ID || argc < 3 + atoi(argv[2])) {
		perror("[Usage: pa2 -p N B_1 B_2 B_3 ... B_N]\n");
		exit(1);
	}

	child_num = atoi(argv[2]);

	fd = malloc(2*child_num*(child_num+1)*sizeof(int));
	pipes = fopen(pipes_log, "w");

	events = fopen(events_log, "w");
	fclose(events);

	if(open_pipes() != 0) {exit(77);}

	events = fopen(events_log, "a");
	for (int i = 0; i < child_num; ++i) {
		pid_t pid = fork();
		if (pid == 0) {
			id = i + 1;
			break;
		} else if (pid < 0) {
			fprintf(stderr, "[Error creating child %d]\n", i + 1);
			clear_descriptors(fd, 2*child_num*(child_num+1));
			pipes = fopen(pipes_log, "a");
			fprintf(pipes, "[Closed for process %d all descriptors]\n", id);
			fclose(pipes);
			return 8;
		}
	}

	pipes = fopen(pipes_log, "a");
	for (int i = 0; i < child_num*(child_num+1); ++i) {
		int f = i / child_num;
		int t = i + 1 - f * child_num;
		if (f == t) t = PARENT_ID;
		if (f != id) {
			close(fd[2*i+1]);
			fprintf(pipes, "[Closed for process %d descriptor for sending messages to process %d by process %d]\n", id, t, f);
		}
		if (t != id) {
			close(fd[2*i]);
			fprintf(pipes, "[Closed for process %d descriptor for receiving messages from process %d by process %d]\n", id, f, t);
		}
	}

	Condition cond;
	if(cond_init(&cond, id, child_num, 0, fd) != 0){exit(44);}
	
	if (id == PARENT_ID) {
		for (int i = 0; i < child_num; ++i) {
			Message stmsg;
			while (receive_any(&cond, &stmsg)) {}
			if (stmsg.s_header.s_magic != MESSAGE_MAGIC || stmsg.s_header.s_type != STARTED) {
				perror("[Parent][Message is broken, invalid magic]\n");
				if(flose() != 0){exit(10);}
				exit(9);
			}
		}
		
		fprintf(events, log_received_all_started_fmt, get_lamport_time(), id);
		bank_robbery(&cond, child_num);
		Message stop;
		stop.s_header.s_magic = MESSAGE_MAGIC;
		stop.s_header.s_payload_len = 0;
		stop.s_header.s_type = STOP;
		stop.s_header.s_local_time = get_lamport_time();
		if (send_multicast(&cond, &stop)) {
			perror("[Parent][Error sending STOP message]\n");
			fclose(events);
			clear_descriptors(fd, 2*child_num*(child_num+1));
			fprintf(pipes, "[Closed for process %d all descriptors]\n", id);
			fclose(pipes);
			return 13;
		}
		AllHistory summary;
		summary.s_history_len = child_num;
		for (int i = 0; i < 2*child_num; ++i) {
			Message msg;
			while (receive_any(&cond, &msg)) {}
			if (msg.s_header.s_magic != MESSAGE_MAGIC) {
				perror("[Parent][Message is broken, invalid magic]\n");
				fclose(events);
				clear_descriptors(fd, 2*child_num*(child_num+1));
				fprintf(pipes, "[Closed for process %d all descriptors]\n", id);
				fclose(pipes);
				return 14;
			}
			switch (msg.s_header.s_type) {
				BalanceHistory *balhist;
			case DONE:
				break;
			case BALANCE_HISTORY:
				balhist = (BalanceHistory*)msg.s_payload;
				summary.s_history[balhist->s_id-1] = *balhist;
				break;
			default:
				perror("[Parent][Message is broken, invalid type]\n");
				fclose(events);
				clear_descriptors(fd, 2*child_num*(child_num+1));
				fprintf(pipes, "[Closed for process %d all descriptors]\n", id);
				fclose(pipes);
				return 15;
			}
		}

		fprintf(events, log_received_all_done_fmt, get_lamport_time(), id);

		int max_history_len = 0;

		for (int i = 0; i < child_num; ++i) {
			if (summary.s_history[i].s_history_len > max_history_len)
				max_history_len = summary.s_history[i].s_history_len;
		}
		
		for (int i = 0; i < child_num; ++i) {
			balance_t balance = summary.s_history[i].s_history[summary.s_history[i].s_history_len-1].s_balance;
			balance_t pending = summary.s_history[i].s_history[summary.s_history[i].s_history_len-1].s_balance_pending_in;
			for (int j = summary.s_history[i].s_history_len; j < max_history_len; ++j) {
				summary.s_history[i].s_history[j].s_balance = balance;
				summary.s_history[i].s_history[j].s_balance_pending_in = pending;
				summary.s_history[i].s_history[j].s_time = j;
			}
			summary.s_history[i].s_history_len = max_history_len;
		}
		print_history(&summary);

		for (int i = 0; i < child_num; ++i) wait(NULL);
	} else {
		balance_t balance = atoi(argv[2+id]);
		balance_t pending = 0;
		if (balance < 0) {
			fprintf(stderr, "[Child %d][Balance cannot be negative, %d provided]\n", id, balance);
			if(flose() != 0){exit(11);}
			exit(12);
		} // FIXME process is finished but other processes wait for messages from it so the program is never finished
		timestamp_t time = get_lamport_time();
		BalanceHistory balhist;
		if(his_init(&balhist, id, time, balance, 0) != 0){exit(75);}
		time = get_lamport_time();

		Message started;
		if(msg_init(&started, 0, STARTED, time) != 0){exit(98);}

		if (send(&cond, PARENT_ID, &started)) {
			fprintf(stderr, "[Child %d][Error sending STARTED message]\n", id);
			if(flose() != 0){exit(27);}
			exit(28);
		}

		fprintf(events, log_started_fmt, time, id, getpid(), getppid(), balance);

		int no_stop = 1;
		while (no_stop || pending > 0) {
			Message msg;
			while (receive_any(&cond, &msg)) {}
			if (msg.s_header.s_magic != MESSAGE_MAGIC)
				fprintf(stderr, "[Child %d][Message is broken, invalid magic]\n", id);
			switch (msg.s_header.s_type) {
				TransferOrder *order;
				balance_t amount;
			case ACK:
				time = msg.s_header.s_local_time;
				amount = atoi(msg.s_payload);
				balance_t old_pending = pending;
				pending -= amount;
				for (int i = balhist.s_history_len; i < time; ++i) {
					if(his_inti(&balhist, i, balance, old_pending) != 0){exit(99);}
				}
				if(his_inti(&balhist, time, balance, pending) != 0){exit(99);}
				balhist.s_history_len = time + 1;
				break;
			case TRANSFER:
				order = (TransferOrder*)msg.s_payload;
				local_id source = order->s_src;
				local_id destination = order->s_dst;
				amount = order->s_amount;
				get_lamport_time();
				time = get_lamport_time();
				msg.s_header.s_local_time = time;

				if (source == id && destination != id) {
					if (balance < amount) {
						fprintf(stderr, "[Child %d][Not enough money][Balance is %d, trying to transfer %d]\n", id, balance, amount);
						if (send(&cond, PARENT_ID, &msg)) {
							fprintf(stderr, "[Child %d][Error sending TRANSFER message to parent]\n", id);
							if(flose() != 0){exit(13);}
						}
					} else {
						balance_t old_balance = balance;
						balance_t old_pending = pending;
						balance -= amount;
						pending += amount;
						if (send(&cond, destination, &msg)) {
							fprintf(stderr, "[Child %d][Error sending TRANSFER message to child %d]\n", id, destination);
							if(flose() != 0){exit(14);}
							exit(19);
						}

						fprintf(events, log_transfer_out_fmt, time, id, amount, destination);

						for (int i = balhist.s_history_len; i < time; ++i) {
							if(his_inti(&balhist, i, old_balance, old_pending != 0)) {exit(88);}
						}
						
						balhist.s_history[time].s_balance = balance;
						balhist.s_history[time].s_time = time;
						balhist.s_history[time].s_balance_pending_in = pending;
						balhist.s_history_len = time + 1;
					}
				} else if (source != id && destination == id) {
					balance_t old_balance = balance;
					balance += amount;
					Message s_ack;
					Message ack;

					sprintf(s_ack.s_payload, "%d", amount);
					if(msg_init(&s_ack, strlen(s_ack.s_payload), ACK, time) != 0){exit(97);}
					if(msg_init(&ack, 0, ACK, time) != 0){exit(97);}

					if (send(&cond, source, &s_ack) || send(&cond, PARENT_ID, &ack)) {
						fprintf(stderr, "[Child %d][Error sending ACK message to child %d]\n", id, source);
						if(flose() != 0){exit(15);}
						exit(20);
					}

					fprintf(events, log_transfer_in_fmt, time, id, amount, source);

					for (int i = balhist.s_history_len; i < time; ++i) {
						if(his_inti(&balhist, i, old_balance, pending != 0)) {exit(88);}
					}
					balhist.s_history[time].s_balance = balance;
					balhist.s_history[time].s_time = time;
					balhist.s_history[time].s_balance_pending_in = pending;
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
		time = get_lamport_time();
		done.s_header.s_local_time = time;
		
		if (send(&cond, PARENT_ID, &done)) {
			fprintf(stderr, "[Child %d][Error sending DONE message]\n", id);
			if(flose() != 0){exit(16);}
			exit(21);
		}

		fprintf(events, log_done_fmt, time, id, balance);
		Message blmsg;
		if(msg_init(&blmsg, sizeof(balhist.s_id) + sizeof(balhist.s_history_len) + sizeof(BalanceState) * balhist.s_history_len, BALANCE_HISTORY, get_lamport_time()) != 0){exit(94);}

		const char *payload = (const char*)(&balhist);
		for (int i = 0; i < blmsg.s_header.s_payload_len; ++i) blmsg.s_payload[i] = payload[i];
		if (send(&cond, PARENT_ID, &blmsg)) {
			fprintf(stderr, "[Child %d][Error sending BALANCE_HISTORY message]\n", id);
			if(flose() != 0){exit(17);}
			exit(22);
		}
	}

	if(flose() != 0){exit(18);}

	return 0;
}
