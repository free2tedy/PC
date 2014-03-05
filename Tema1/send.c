#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "lib.h"


int main(int argc, char *argv[])
{
	int i;
	msg t,r;
	my_pkt p;
	char *filename;
	int task_index, speed, delay;

	task_index = atoi(argv[1]);
	filename = argv[2];
	speed = atoi(argv[3]);
	delay = atoi(argv[4]);

	int BDP = speed * delay;
	int window = BDP * 1000 / 8 / 1404;
	int timeout;

	if (2 * delay < 1000) {
		timeout = 1000;
	} else {
		timeout = 2 * delay;
	}

	printf("[SENDER] Sender starts.\n");
	printf("[SENDER] Filename=%s, task_index=%d, speed=%d, delay=%d\n", filename, task_index, speed, delay);

	init(HOST, PORT1);

	char newFilename[100], recv[100];
	strcpy(recv, "recv_");
	strcpy(newFilename, filename);
	strcat(recv, newFilename);

	/* Sending filename */

	memset(t.payload, 0, sizeof(t.payload));
	memset(p.payload, 0, sizeof(p.payload));
	p.seq = -2;
	p.detection_index = calculate_detection_index(recv, strlen(recv));
	memcpy(p.payload, &recv, strlen(recv));
	t.len = 3 * sizeof(int) + strlen(recv) + 1;
	memcpy(t.payload, &p, sizeof(t.payload));
	send_message(&t);

	/* Waiting ACK for filename */


	while (recv_message_timeout(&r, timeout) < 0) {
		send_message(&t);
	}


	int fd, count, filesize;
	struct stat f_status;
	fd = open(filename, O_RDONLY);
	fstat(fd, &f_status);

	filesize = (int) f_status.st_size;
	count = filesize / 1392 + 1;

	/* Sending number of frames */

	memset(t.payload, 0, sizeof(t.payload));
	memset(p.payload, 0, sizeof(p.payload));
	p.seq = count;
	//p.detection_index = calculate_detection_index(count, 4);
	memcpy(p.payload, &count, sizeof(int));
	t.len = 3 * sizeof(int);
	memcpy(t.payload, &p, sizeof(t.payload));
	send_message(&t);

	/* Waiting for ACK for number of frames */

	while (recv_message_timeout(&r, timeout) < 0) {
		send_message(&t);
	}

	printf("[SENDER] Got reply with payload: %s\n", t.payload);

	/* ---- TASK 0 ---- */

	if (task_index == 0) {

		/* Sending first window */

		int byteCount;
		char buffer[1392];

		for (i = 0; i < window; i++) {

			byteCount = read(fd, buffer, 1392);
			memset(t.payload, 0, sizeof(t.payload));
			memset(p.payload, 0, sizeof(p.payload));
			p.seq = i;
			memcpy(p.payload, buffer, byteCount);
			t.len = 2 * sizeof(int) + byteCount;
			memcpy(t.payload, &p, t.len);

			if (send_message(&t) < 0) {
				perror("[SENDER] Send error. Exiting.\n");
				return -1;
			}
			printf("Sent frame no. %d -- bytes:%d \n", p.seq, byteCount);
		}

		for (i = 0; i < count - window; i++) {
			if (recv_message(&r) < 0) {
				perror("[SENDER] Receive error. Exiting.\n");
				return -1;
			}
			printf("Got msg with payload %s\n", r.payload);


			byteCount = read(fd, buffer, 1392);
			memset(t.payload, 0, sizeof(t.payload));
			memset(p.payload, 0, sizeof(p.payload));
			p.seq = i + window;
			memcpy(p.payload, buffer, byteCount);
			t.len = 2 * sizeof(int) + byteCount;
			memcpy(t.payload, &p, t.len);

			if (send_message(&t) < 0) {
				perror("[SENDER] Send error. Exiting.\n");
				return -1;
			}
			printf("Sent frame no. %d -- bytes:%d \n", p.seq, byteCount);
		}

		for (i = 0; i < window; i++) {
			if (recv_message(&r) < 0) {
				perror("[SENDER] Receive error. Exiting.\n");
				return -1;
			}
			printf("Got msg with payload %s\n", r.payload);
		}

	}

	/* ---- TASK 1 ---- */

	if (task_index == 1) {

		int byteCount, ACK[count];
		memset(ACK, 0, count);
		msg frame[count];
		char buffer[1392];
		int first = 0, last = window;
		my_pkt p1;


		for (i = 0; i < window; i++) {

			byteCount = read(fd, buffer, 1392);
			memset(t.payload, 0, sizeof(t.payload));
			memset(p.payload, 0, sizeof(p.payload));
			p.seq = i;
			memcpy(p.payload, buffer, byteCount);
			t.len = 2 * sizeof(int) + byteCount;
			memcpy(t.payload, &p, t.len);
			frame[i] = t;

			if (send_message(&t) < 0) {
				perror("[SENDER] Send error. Exiting.\n");
				return -1;
			}
			printf("Sent frame no. %d -- bytes:%d \n", p.seq, byteCount);
		}

		while (last < count) {
			if (recv_message_timeout(&r, timeout) > 0) {
				p1 = *((my_pkt*) r.payload);
				printf("Got ACK(%d)\n", p1.seq);
				if (p1.seq == first) {
					ACK[first++] = 1;
					byteCount = read(fd, buffer, 1392);
					memset(t.payload, 0, sizeof(t.payload));
					memset(p.payload, 0, sizeof(p.payload));
					p.seq = last;
					memcpy(p.payload, buffer, byteCount);
					t.len = 2 * sizeof(int) + byteCount;
					memcpy(t.payload, &p, t.len);
					frame[last++] = t;

					if (send_message(&t) < 0) {
						perror("[SENDER] Send error. Exiting.\n");
						return -1;
					}
					printf("Sent frame no. %d -- bytes:%d \n", p.seq, byteCount);
				}
				if(p1.seq == last - 1) {
				    printf("RESENDING before TIMEOUT\n");
                    for (i = first; i < last; i++) {
                        if (send_message(&frame[i]) < 0) {
                            perror("[SENDER] Send error. Exiting.\n");
                            return -1;
                        }
                        my_pkt p2 = *((my_pkt*) frame[i].payload);
                        printf("++ Sent frame no. %d -- bytes:%d \n", p2.seq, byteCount);
                    }
				}
			} else {
                printf("[TIMEOUT] RESENDING\n");
                for (i = first; i < last; i++) {
					if (send_message(&frame[i]) < 0) {
						perror("[SENDER] Send error. Exiting.\n");
						return -1;
					}
					my_pkt p2 = *((my_pkt*) frame[i].payload);
					printf("++ Sent frame no. %d -- bytes:%d \n", p2.seq, byteCount);
				}

			}
		}

        while (first < count) {
            if (recv_message_timeout(&r, timeout) > 0) {
				p1 = *((my_pkt*) r.payload);
				printf("Got ACK(%d)\n", p1.seq);
				if(p1.seq == first) {
                    first++;
				} else if (p1.seq == count - 1) {
                    printf("RESENDING before TIMEOUT\n");
                    for (i = first; i < count; i++) {
                        if (send_message(&frame[i]) < 0) {
                            perror("[SENDER] Send error. Exiting.\n");
                            return -1;
                        }
                        my_pkt p2 = *((my_pkt*) frame[i].payload);
                        printf("++ Sent frame no. %d -- bytes:%d \n", p2.seq, byteCount);
                    }
				}
            } else {
                printf("[TIMEOUT] RESENDING\n");
                for (i = first; i < count; i++) {
					if (send_message(&frame[i]) < 0) {
						perror("[SENDER] Send error. Exiting.\n");
						return -1;
					}
					my_pkt p2 = *((my_pkt*) frame[i].payload);
					printf("++ Sent frame no. %d -- bytes:%d \n", p2.seq, byteCount);
				}
            }
        }

	}

    /* ---- TASK 2 & 3 ---- */

    if (task_index == 2 || task_index == 3) {
        int byteCount;
		msg frame[count];
		int ACK[count];
		memset(ACK, 0, count);
		char buffer[1392];
		int last = window, first = 0;

		for (i = 0; i < window; i++) {

			byteCount = read(fd, buffer, 1392);
			memset(t.payload, 0, sizeof(t.payload));
			memset(p.payload, 0, sizeof(p.payload));
			p.seq = i;
			memcpy(p.payload, buffer, byteCount);
            p.detection_index = calculate_detection_index(buffer, byteCount);
			t.len = 3 * sizeof(int) + byteCount;
			memcpy(t.payload, &p, sizeof(t.payload));
			frame[i] = t;

			if (send_message(&t) < 0) {
				perror("[SENDER] Send error. Exiting.\n");
				return -1;
			}
			printf("Sent frame no. %d -- bytes:%d \n", p.seq, byteCount);
		}

		while (last < count) {
            if (recv_message_timeout(&r, timeout) > 0) {
                my_pkt p1 = *((my_pkt*) r.payload);
                if (p1.seq == first) {
                    ACK[first++] = 1;
                    while(ACK[first] == 1) {
                        first++;
                    }
                    if(last - first == window - 1) {
                        byteCount = read(fd, buffer, 1392);
                        memset(t.payload, 0, sizeof(t.payload));
                        memset(p.payload, 0, sizeof(p.payload));
                        p.seq = last;

                        memcpy(p.payload, buffer, byteCount);
                        p.detection_index = calculate_detection_index(buffer, byteCount);

                        t.len = 3 * sizeof(int) + byteCount;
                        memcpy(t.payload, &p, sizeof(t.payload));
                        frame[last++] = t;

                        if (send_message(&t) < 0) {
                            perror("[SENDER] Send error. Exiting.\n");
                            return -1;
                        }
                        printf("[FIRST] Sent frame no. %d -- bytes:%d \n", p.seq, byteCount);
                    }
                    if (first == last) {
                        if (last + window < count) {
                            last = last + window;
                        } else {
                            last = count;
                        }
                        for (i = first; i < last; i++) {
                            byteCount = read(fd, buffer, 1392);
                            memset(t.payload, 0, sizeof(t.payload));
                            memset(p.payload, 0, sizeof(p.payload));
                            p.seq = i;

                            memcpy(p.payload, buffer, byteCount);
                            p.detection_index = calculate_detection_index(buffer, byteCount);

                            t.len = 3 * sizeof(int) + byteCount;
                            memcpy(t.payload, &p, sizeof(t.payload));
                            frame[i] = t;

                            if (send_message(&t) < 0) {
                                perror("[SENDER] Send error. Exiting.\n");
                                return -1;
                            }

                            printf("[FIRST==LAST] Sent frame no. %d -- bytes:%d \n", p.seq, byteCount);
                        }
                    }
                } else {
                    ACK[p1.seq] = 1;
                    if (p1.seq == last - 1) {
                        for (i = first; i < p.seq; i++) {
                            if (ACK[i] == 0) {
                                if (send_message(&frame[i]) < 0) {
                                    perror("[SENDER] Send error. Exiting.\n");
                                    return -1;
                                }
                                my_pkt p2 = *((my_pkt*) frame[i].payload);
                                printf("[RESEND W-O LAST] Sent frame no. %d -- bytes:%d \n", p2.seq, byteCount);
                            }
                        }
                    }
                }
            } else {
                for (i = first; i < last; i++) {
                    if (ACK[i] == 0) {
                        if (send_message(&frame[i]) < 0) {
                            perror("[SENDER] Send error. Exiting.\n");
                            return -1;
                        }
                        my_pkt p2 = *((my_pkt*) frame[i].payload);
                        printf("[RESEND W LAST] Sent frame no. %d -- bytes:%d -- Index:%d \n", p2.seq, frame[i].len - 3 * sizeof(int), p2.detection_index);
                    }
                }
            }
		}

		while (first < count) {
            if (recv_message_timeout(&r, timeout) > 0) {
                my_pkt p1 = *((my_pkt*) r.payload);
                if (p1.seq == first) {
                    ACK[first++] = 1;
                    while(ACK[first] == 1) {
                        first++;
                    }
                } else {
                    ACK[p1.seq] = 1;
                    if (p1.seq == count -1) {
                        for (i = first; i < count; i++) {
                            if (ACK[i] == 0) {
                                if (send_message(&frame[i]) < 0) {
                                    perror("[SENDER] Send error. Exiting.\n");
                                    return -1;
                                }
                                my_pkt p2 = *((my_pkt*) frame[i].payload);
                                printf("[RESEND W-O LAST] Sent frame no. %d -- bytes:%d \n", p2.seq, byteCount);
                            }
                        }
                    }
                }
            } else {
                for (i = first; i < count; i++) {
                    if (ACK[i] == 0) {
                        if (send_message(&frame[i]) < 0) {
                            perror("[SENDER] Send error. Exiting.\n");
                            return -1;
                        }
                    my_pkt p2 = *((my_pkt*) frame[i].payload);
                    printf("[RESEND W LAST] Sent frame no. %d -- bytes:%d -- Index:%d\n", p2.seq, frame[i].len - 3 * sizeof(int), p2.detection_index);
                    }
                }
            }
		}
    }

	printf("[SENDER] Job done.\n");

	return 0;
}
