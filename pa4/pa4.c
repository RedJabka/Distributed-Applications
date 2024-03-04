#include "fuc.h"

static int  child_num, *fd;
static int id = PARENT_ID;
static Req qq[MAX_PROCESS_ID];
static CNDTN cond;
static int req_acc = 1;
static Choice timing = SENDING_STARTED;
static int stop_p[MAX_PROCESS_ID], mtxl_on;
static int req_st, req_dn, showed, req_num = 0;

int flose(){
	fclose(events);
	clear_descriptors(fd, 2*child_num*(child_num+1));
	fprintf(pipes, "[Closed for process %d all descriptors]\n", id);
	fclose(pipes);

	return 0;
}

void pee_ar(Req ekuset) {
	qq[req_num] = ekuset;
	int i = req_num;
	++req_num;
	while (i > 0 && cmp(qq[i], qq[(i-1)/2]) < 0) {
		int t = qq[i].time;
		qq[i].time = qq[(i-1)/2].time;
		qq[(i-1)/2].time = t;
		t = qq[i].id;
		qq[i].id = qq[(i-1)/2].id;
		qq[(i-1)/2].id = t;
		i = (i - 1) / 2;
	}
}

void rmreq(int index) {
	if (req_num > index) {
		--req_num;
		if (req_num > index) {
			qq[index] = qq[req_num];
			int i = index;
			while (1) {
				int x = 2 * i + 1;
				int y = 2 * i + 2;
				if (x >= req_num) break;
				if (y < req_num) {
					if (cmp(qq[x], qq[y]) > 0) {
						if (cmp(qq[i], qq[y]) > 0) {
							int t = qq[i].time;
							qq[i].time = qq[y].time;
							qq[y].time = t;
							t = qq[i].id;
							qq[i].id = qq[y].id;
							qq[y].id = t;
							i = y;
						} else break;
					} else if (cmp(qq[i], qq[x]) > 0) {
						int t = qq[i].time;
						qq[i].time = qq[x].time;
						qq[x].time = t;
						t = qq[i].id;
						qq[i].id = qq[x].id;
						qq[x].id = t;
						i = x;
					} else break;
				} else if (cmp(qq[i], qq[x]) > 0) {
					int t = qq[i].time;
					qq[i].time = qq[x].time;
					qq[x].time = t;
					t = qq[i].id;
					qq[i].id = qq[x].id;
					qq[x].id = t;
					i = x;
				} else break;
			}
		}
	}
}

void case_request(Req * new_request, Message * reply, Message * msg, int i){
	new_request->time = msg->s_header.s_local_time;
	new_request->id = i;
	pee_ar(*new_request);

	if(msg_init(reply, 0, CS_REPLY, get_lamport_time()) != 0){exit(97);}
						
	if (send(&cond, i, reply)) {
	fprintf(stderr, "[Child %d][Error sending CS_REPLY message]\n", cond.c_id);
	if(flose() != 0){exit(22);}
	exit(22);
	}
}

int case_done(int i){
	if (!stop_p[i-1]) {
		stop_p[i-1] = 1;
		++req_dn;
		return 1;
	}
	else return 0;
}

int mb_error_req(const void * self) {
	Req ekuset;
	ekuset.time = get_lamport_time();
	ekuset.id = cond.c_id;
	pee_ar(ekuset);
	Message csreq;

	if(msg_init(&csreq, 0, CS_REQUEST, ekuset.time) != 0){exit(97);}
	
	for (int i = 1; i <= cond.child_num; ++i) {
		if (i != cond.c_id && !stop_p[i-1]) {
			if (send(&cond, i, &csreq)) {
				fprintf(stderr, "[Child %d][Error sending CS_REQUEST message]\n", cond.c_id);
				if(flose() != 0){exit(31);}
				exit(32);
			}
		}
	}
	int replies_received = 0;
	int active_processes = cond.child_num - 1 - req_dn;
	while (replies_received < active_processes || qq[0].id != cond.c_id) {
		Message msg;
		for (int i = 1; i <= cond.child_num; ++i) {
			if (i != cond.c_id) {
				if (receive(&cond, i, &msg) == 0) {
					switch (msg.s_header.s_type) {
						Req new_request;
						Message reply;
					case CS_REQUEST:
						case_request(&new_request, &reply, &msg, i);
						break;
					case CS_REPLY:
						++replies_received;
						break;
					case CS_RELEASE:
						rmreq(0);
						break;
					case DONE:
						if(case_done(i) == 1) --active_processes;
						break;
					default:
						break;
					}
					break;
				}
			}
		}
	}
	return 0;
}

int mb_error_critsec(const void * self) {
	rmreq(0);

	Message cc;
	if(msg_init(&cc, 0, CS_RELEASE, get_lamport_time()) != 0){exit(97);}

	for (int i = 1; i <= cond.child_num; ++i) {
		if (i != cond.c_id && !stop_p[i-1]) {
			if (send(&cond, i, &cc)) {
				fprintf(stderr, "[Child %d][Error sending CS_RELEASE message]\n", cond.c_id);
				if(flose() != 0){exit(21);}
				exit(1);
			}
		}
	}
	return 0;
}

int pupu() {
	while (timing != COMPLETED) {
		switch (timing) {
			pid_t pid, ppid;
			Message started, done;
			char logstr[64];
		case SENDING_STARTED:
			pid = getpid();
			ppid = getppid();
			sprintf(started.s_payload, log_started_fmt, get_lamport_time(), cond.c_id, pid, ppid, 0);
			fprintf(events, "%s", started.s_payload);

			if(msg_init(&started, strlen(started.s_payload), STARTED, get_lamport_time()) != 0){exit(97);}
			
			if (send_multicast(&cond, &started)) {
				fprintf(stderr, "[Child %d][Error sending STARTED message]\n", cond.c_id);
				if(flose() != 0){exit(17);}
				exit(7);
			}
			timing = RECEIVING_STARTED;
			break;
		case RECEIVING_STARTED:
			break;
		case PRINTING:
			sprintf(logstr, log_loop_operation_fmt, cond.c_id, showed + 1, cond.c_id * 5);
			if (mtxl_on) {
				if (mb_error_req(NULL)) {
					fprintf(stderr, "[Child %d][Error entering critical section]\n", cond.c_id);
					if(flose() != 0){exit(17);}
					exit(2);
				}
				print(logstr);
				if (mb_error_critsec(NULL)) {
					fprintf(stderr, "[Child %d][Error leaving critical section]\n", cond.c_id);
					if(flose() != 0){exit(18);}
					exit(3);
				}
			} else print(logstr);
			++showed;
			if (showed == cond.c_id * 5) timing = SENDING_DONE;
			break;
		case SENDING_DONE:
			sprintf(done.s_payload, log_done_fmt, get_lamport_time(), cond.c_id, 0);
			fprintf(events, "%s", done.s_payload);

			if(msg_init(&done, strlen(done.s_payload), DONE, get_lamport_time()) != 0){exit(97);}
			
			if (send_multicast(&cond, &done)) {
				fprintf(stderr, "[Child %d][Error sending DONE message]\n", cond.c_id);
				if(flose() != 0){exit(21);}
				exit(4);
			}
			req_acc = 0;
			timing = RECEIVING_DONE;
			break;
		case RECEIVING_DONE:
			if ((timing == RECEIVING_DONE) && (req_dn >= cond.child_num - 1)) {
				fprintf(events, log_received_all_done_fmt, get_lamport_time(), cond.c_id);
				timing = COMPLETED;
			}
			break;
		default:
			break;
		}
		for (int i = 1; i <= cond.child_num; ++i) {
			if (i != cond.c_id) {
				Message msg;
				if (receive(&cond, i, &msg) == 0) {
					switch (msg.s_header.s_type) {
						Req new_request;
						Message reply;
					case STARTED:
						if (timing == RECEIVING_STARTED) {
							++req_st;
							if (req_st >= cond.child_num - 1) {
								fprintf(events, log_received_all_started_fmt, get_lamport_time(), cond.c_id);
								timing = PRINTING;
							}
						}
						break;
					case CS_REQUEST:
						if (mtxl_on && req_acc) {
							case_request(&new_request, &reply, &msg, i);
						}
						break;
					case CS_RELEASE:
						if (mtxl_on && req_acc) rmreq(0);
						break;
					case DONE:
						case_done(i);
						break;
					default:
						break;
					}
					break;
				}
			}
		}
	}
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
		fprintf(pipes, "[Opened for all processes descriptor for receiving messages from process %d by process %d]\n", f, t);
		fprintf(pipes, "[Opened for all processes descriptor for sending messages to process %d by process %d]\n", t, f);
	}
	fclose(pipes);
	return 0;
}

int main(int argc, char **argv) {
	int mutexl = (strcmp(argv[1], "--mutexl") == 0);

	if (argc < 3 || argc < 3 + mutexl || strcmp(argv[1+mutexl], "-p")) {
		perror("Usage:\npa4 -p <N>\npa4 -p <N> --mutexl\npa4 --mutexl -p <N>\n");
		exit(1);
	}

	child_num = atoi(argv[2+mutexl]);
	if (child_num <= 0 || child_num > MAX_PROCESS_ID) {
		fprintf(stderr, "[Illegal number of child_num %d, expecting 1 to %d]\n", child_num, MAX_PROCESS_ID);
		return 4;
	}
	if (!mutexl && argc > 3) mutexl = (strcmp(argv[3], "--mutexl") == 0);
	mtxl_on = mutexl;
	id = PARENT_ID;
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
	if(close_pipes(child_num, pipes, fd, id) != 0){exit(32);}

	if(cond_init(&cond, id, child_num, 0, fd) != 0){exit(44);}

	if (id == PARENT_ID) {
		for (int i = 1; i <= child_num; ++i) {
			while (receive_and_check_quiet(&cond, i, STARTED)) {}
		}
		for (int i = 1; i <= child_num; ++i) {
			while (receive_and_check_quiet(&cond, i, DONE)) {}
		}
		for (int i = 0; i < child_num; ++i) wait(NULL);
	} else {
		for (int i = 0; i < child_num; ++i) stop_p[i] = 0;

		if (pupu()) {
			fprintf(stderr, "[Child %d][Process returned status %d]\n", id, pupu());
			if(flose() != 0){exit(13);}
			exit(9);
		}
	}
	if(flose() != 0){exit(14);}

	return 0;
}
