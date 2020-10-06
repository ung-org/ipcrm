/*
 * UNG's Not GNU
 *
 * Copyright (c) 2020, Jakob Kaivo <jkk@ung.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _XOPEN_SOURCE 700
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>

typedef enum { MSGID, SHMID, SEMID, MSGKEY, SHMKEY, SEMKEY } ipc_type;

struct ipcrm {
	struct ipcrm *next;
	struct ipcrm *prev;
	ipc_type type;
	const char *s;
	int id;
};

static struct ipcrm *ipcrm_q(ipc_type type, const char *id, struct ipcrm *head)
{
	struct ipcrm *add = calloc(1, sizeof(*add));
	if (add == NULL) {
		perror("ipcrm");
		return NULL;
	}
	add->type = type;
	add->s = id;

	errno = 0;
	char *end = NULL;
	long l = strtol(id, &end, 0);
	if ((end && *end) || (l > INT_MAX) || (l < 0)) {
		fprintf(stderr, "ipcrm: %s: %s\n", id, strerror(EINVAL));
		return NULL;
	}
	add->id = l;

	if (type == MSGKEY) {
		add->id = msgget(add->id, 0);
	} else if (type == SEMKEY) {
		add->id = semget(add->id, 0, 0);
	} else if (type == SHMKEY) {
		add->id = shmget(add->id, 0, 0);
	}

	if (add->id == -1) {
		fprintf(stderr, "ipcrm: %s: %s\n", id, strerror(EINVAL));
		return NULL;
	}

	if (head == NULL) {
		return add;
	}

	struct ipcrm *tail = head;
	while (tail && tail->next) {
		tail = tail->next;
	}
	insque(add, tail);

	return head;
}

static int ipcrm(struct ipcrm *ipc)
{
	int ret = 0;

	switch (ipc->type) {
	case MSGID:
	case MSGKEY:
		ret = msgctl(ipc->id, IPC_RMID, NULL);
		break;

	case SEMID:
	case SEMKEY:
		ret = semctl(ipc->id, 0, IPC_RMID);
		break;

	case SHMID:
	case SHMKEY:
		ret = shmctl(ipc->id, IPC_RMID, NULL);
		break;
	}

	if (ret == -1) {
		fprintf(stderr, "ipcrm: %s: %s\n", ipc->s, strerror(errno));
		return 1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");

	int c;
	int ret = 0;
	struct ipcrm *list = NULL;

	while ((c = getopt(argc, argv, "q:Q:s:S:m:M:")) != -1) {
		switch(c) {
		case 'q':
			list = ipcrm_q(MSGID, optarg, list);
			break;

		case 'Q':
			list = ipcrm_q(MSGKEY, optarg, list);
			break;

		case 's':
			list = ipcrm_q(SEMID, optarg, list);
			break;

		case 'S':
			list = ipcrm_q(SEMKEY, optarg, list);
			break;

		case 'm':
			list = ipcrm_q(SHMID, optarg, list);
			break;

		case 'M':
			list = ipcrm_q(SHMKEY, optarg, list);
			break;

		default:
			return 1;
		}

		if (list == NULL) {
			return 1;
		}
	}

	while (list) {
		ret |= ipcrm(list);
		list = list->next;
	}

	return ret;
}
