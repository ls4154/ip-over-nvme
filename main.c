#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <linux/nvme_ioctl.h>

#define BUFSIZE 4096

static int tun_fd;
static char tun_name[IFNAMSIZ] = {'\0'};

static int nvme_fd;
static char *nvme_dev = "/dev/nvme0n1";

void sigint_hadler()
{
	if (tun_fd >= 0)
		close(tun_fd);
	exit(0);
}

void *nvme_to_ip(void *arg)
{
	int rc;
	int len;
	int nwrite;
	char *buf;
	struct nvme_user_io io;
	struct timespec tv;

	buf = malloc(BUFSIZE);

	tv.tv_sec = 1;
	tv.tv_nsec = 0;

	while (1) {
		/* read from nvme */
		rc = ioctl(fd, NVME_IOCTL_SUBMIT_IO, &io);
		if (rc != 0) {
			fprintf(stderr, "nothing to read\n");
			goto loop_sleep;
		}

		/* check length */

		/* write to tun */
		nwrite = write(tun_fd, buf, len);
		if (nwrite < len) {
			fprintf(stderr, "write error\n");
		}

loop_sleep:
		nanosleep(&tv, NULL);
	}
	return NULL;
}

void ip_to_nvme()
{
	int rc;
	int nread;
	char *buf;
	struct nvme_user_io io;

	buf = malloc(BUFSIZE);

	io = {
		.opcode = nvme_cmd_write,
		.flags		= 0,
		.control	= 0,
		.nblocks	= 7,
		.rsvd		= 0,
		.metadata	= 0,
		.addr		= 0,
		.slba		= 9223372036854775807L,
		.dsmgmt		= 0,
		.reftag		= 0,
		.appmask	= 0,
		.apptag		= 0,
	};

	while (1) {
		/* read from tun */
		nread = read(tun_fd, buf, BUFSIZE);
		if (nread < 0) {
			fprintf(stderr, "read error\n");
			close(tun_fd);
			exit(1);
		}
		printf("read %d bytes from tun\n", nread);

		/* check IPV4 */

		/* write to nvme */
		io.nblocks = 10;
		io.addr = (uintptr_t)buf;

		rc = ioctl(nvme_fd, NVME_IOCTL_SUBMIT_IO, &io);
		if (rc != 0) {
			fprintf(stderr, "ioctl error\n");
		}
		printf("write %d bytes to nvme\n", nread);
	}
}

int main(int argc, char **argv)
{
	int nread;
	pthread_t tid;

	signal(SIGINT, sigint_hadler);

	strcpy(tun_name, "tun77");

	tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI);
	if (tun_fd < 0) {
		fprintf(stderr, "tun allocation failed\n");
		exit(1);
	}

	printf("TUN device : %s\n", tun_name);

	if (pthread_create(&tid, NULL, &thread_main, NULL) != 0) {
		fprintf(stderr, "pthread create error\n");
		exit(1);
	}

	ip_to_nvme();

	return 0;
}

/* vim: set ts=8 sw=8 noet: */
