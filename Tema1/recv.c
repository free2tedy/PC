#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "lib.h"

int main(int argc, char *argv[])
{
	msg r, t;
	my_pkt p;
	int task_index;
	int i, fd;
	char filename[100];
	int x;

	task_index = atoi(argv[1]);
	printf("[RECEIVER] Receiver starts.\n");
	printf("[RECEIVER] Task index=%d\n", task_index);

	init(HOST, PORT2);

	/* Receiving filename */

	if (recv_message(&r) < 0) {
		perror("[RECEIVER] Error receive message");
		return -1;
	}

	p = *((my_pkt*) r.payload);
	memcpy(filename, p.payload, r.len - sizeof(int));
	x = calculate_detection_index(p.payload, strlen(p.payload));

    printf("[RECEIVER] Got msg with filename: %s\n", filename);

	/* Sending ACK for filename */

    while (x != p.detection_index) {
        if (recv_message(&r) < 0) {
            perror("[RECEIVER] Error receive message");
            return -1;
        }
        p = *((my_pkt*) r.payload);
        memcpy(filename, p.payload, r.len - sizeof(int));
        x = calculate_detection_index(p.payload, strlen(p.payload));

    }

    sprintf(t.payload, "ACK(filename)");
	t.len = strlen(t.payload + 1);
    send_message(&t);


	/* Receiving number of frames */

	if (recv_message(&r) < 0) {
		perror("[RECEIVER] Error receive message");
		return -1;
	}

	p = *((my_pkt*) r.payload);
	int count;
    count = p.seq;

	printf("[RECEIVER] Got msg with filesize: %d\n", count);

	/* Sending ACK for number of frames */

	sprintf(t.payload, "ACK(number of frames)");
	t.len = strlen(t.payload + 1);

	send_message(&t);


	fd = open(filename, O_CREAT | O_WRONLY |  O_TRUNC, 0644);

    /* ---- TASK 0 ---- */

	if (task_index == 0) {

		/* Writing payload in the file and sending ACK */

		for (i = 0; i < count; i++) {
			memset(r.payload, 0, sizeof(r.payload));
			memset(t.payload, 0, sizeof(t.payload));
			if (recv_message(&r) < 0) {
				perror("[RECEIVER] Error receive message");
				return -1;
			}
			p = *((my_pkt*) r.payload);
			int h = write(fd, p.payload, r.len - 2 * sizeof(int));
			printf("--[%d] %d \n", p.seq, h);
			sprintf(t.payload, "ACK(%d)", p.seq);
			t.len = strlen(p.payload) + sizeof(int);
			send_message(&t);
		}

	}

    /* ---- TASK 1 ---- */

    if (task_index == 1) {
        int next = 0;
        while (next < count) {
            memset(r.payload, 0, sizeof(r.payload));
			memset(t.payload, 0, sizeof(t.payload));
			if (recv_message(&r) < 0) {
				perror("[RECEIVER] Error receive message");
				return -1;
			}
			p = *((my_pkt*) r.payload);
            if(p.seq == next) {
                write(fd, p.payload, r.len - 2 * sizeof(int));
                next++;
                memset(p.payload, 0, sizeof(p.payload));
                memcpy(t.payload, &p, 4);
                send_message(&t);
            } else {
                memset(p.payload, 0, sizeof(p.payload));
                memcpy(t.payload, &p, 4);
                send_message(&t);
            }

        }

    }

    /* ---- TASK 2 & 3 ---- */

    if (task_index == 2 || task_index == 3) {
        int detection_index;
        int first = 0;
        msg frame[count];
        int ACK[count];
        memset(ACK, 0, count);
        while (first < count) {
            memset(r.payload, 0, sizeof(r.payload));
			memset(t.payload, 0, sizeof(t.payload));
			if (recv_message(&r) < 0) {
				perror("[RECEIVER] Error receive message");
				return -1;
			}
			p = *((my_pkt*) r.payload);
			detection_index = calculate_detection_index(p.payload, r.len - 3 * sizeof(int));
            //if(p.seq == count - 1)
                //printf("Index sent: %d ---- Index calculated: %d \n",p.detection_index, detection_index);
            if (p.detection_index == detection_index || task_index == 2 || first == count) {
                if (p.seq == first) {
                    ACK[first] = 1;
                    printf("[RECEIVER] Got pkt no. %d\n", first);
                    frame[first++] = r;
                    write(fd, p.payload, r.len - 3 * sizeof(int));
                    memset(p.payload, 0, sizeof(p.payload));
                    memcpy(t.payload, &p, 4);
                    send_message(&t);
                    while (ACK[first] == 1) {
                        my_pkt p1 = *((my_pkt*) frame[first].payload);
                        write(fd, p1.payload, frame[first].len - 3 * sizeof(int));
                        first++;
                    }
                } else {
                    ACK[p.seq] = 1;
                    frame[p.seq] = r;
                    printf("[RECEIVER] Got pkt no. %d\n", p.seq);
                    memset(p.payload, 0, sizeof(p.payload));
                    memcpy(t.payload, &p, 4);
                    send_message(&t);
                }
            }
        }

    }

	printf("[RECEIVER] All done.\n");

	return 0;
}
