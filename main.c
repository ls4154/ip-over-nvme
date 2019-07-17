#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/nvme_ioctl.h>
#include "tun.h"
#include "ip.h"

#define BUFSIZE (4096 + 10)

enum nvme_opcode {
	nvme_cmd_flush		= 0x00,
	nvme_cmd_write		= 0x01,
	nvme_cmd_read		= 0x02,
	nvme_cmd_write_uncor	= 0x04,
	nvme_cmd_compare	= 0x05,
	nvme_cmd_write_zeroes	= 0x08,
	nvme_cmd_dsm		= 0x09,
	nvme_cmd_resv_register	= 0x0d,
	nvme_cmd_resv_report	= 0x0e,
	nvme_cmd_resv_acquire	= 0x11,
	nvme_cmd_resv_release	= 0x15,
};

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
	struct nvme_user_io io = {
		.opcode = nvme_cmd_read,
		.flags		= 0,
		.control	= 0,
		.nblocks	= 2,
		.rsvd		= 0,
		.metadata	= 0,
		.addr		= 0,
		.slba		= 9223372036854775807L,
		.dsmgmt		= 0,
		.reftag		= 0,
		.appmask	= 0,
		.apptag		= 0,
	};
	struct timespec tv;
	struct ip_hdr *iphdr;

	buf = malloc(BUFSIZE);

	tv.tv_sec = 0;
	tv.tv_nsec = 1000000;

	while (1) {
		/* read from nvme */
		io.addr = (uintptr_t)buf;
		rc = ioctl(nvme_fd, NVME_IOCTL_SUBMIT_IO, &io);
		if (rc != 0) {
			/* fprintf(stderr, "n2i : nothing to read (%d)\n", rc); */
			goto loop_sleep;
		}
		printf("n2i : read from nvme\n");

		/* check length and format */
		iphdr = (void *)buf;
		len = ntohs(iphdr->len);

		if (len <= 20) {
			fprintf(stderr, "n2i : wrong ip format\n");
			goto loop_sleep;
		}

		if (iphdr->version != IPV4) {
			fprintf(stderr, "n2i : not IPV4\n");
			goto loop_sleep;
		}

		/* write to tun */
		nwrite = write(tun_fd, buf, len);
		if (nwrite < len) {
			fprintf(stderr, "n2i : write error\n");
			goto loop_sleep;
		}
		printf("n2i : write %d bytes to tun\n", nwrite);
		for (int i = 0; i < nwrite; i += 19)
			printf("%2x", *(unsigned char *)(buf + i));
		puts("");

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
	struct nvme_user_io io = {
		.opcode = nvme_cmd_write,
		.flags		= 0,
		.control	= 0,
		.nblocks	= 2,
		.rsvd		= 0,
		.metadata	= 0,
		.addr		= 0,
		.slba		= 9223372036854775807L,
		.dsmgmt		= 0,
		.reftag		= 0,
		.appmask	= 0,
		.apptag		= 0,
	};

	buf = malloc(BUFSIZE);

	while (1) {
		/* read from tun */
		nread = read(tun_fd, buf, BUFSIZE);
		if (nread < 0) {
			fprintf(stderr, "i2n : read error\n");
			close(tun_fd);
			exit(1);
		}
		printf("i2n : read %d bytes from tun\n", nread);

		/* check IPV4 */

		/* write to nvme */
		/* io.nblocks = nread; */
		io.addr = (uintptr_t)buf;

		rc = ioctl(nvme_fd, NVME_IOCTL_SUBMIT_IO, &io);
		if (rc != 0) {
			if (rc < 0)
				fprintf(stderr, "i2n : ioctl errorno %s\n", strerror(errno));
			else 
				fprintf(stderr, "i2n : ioctl error (%d)\n", rc);
			continue;
		}
		printf("i2n : write %d bytes to nvme\n", nread);
		for (int i = 0; i < nread; i += 19)
			printf("%2x", *(unsigned char *)(buf + i));
		puts("");
	}
}

int main(int argc, char **argv)
{
	pthread_t tid;

	signal(SIGINT, sigint_hadler);

	/* allocate tun device */
	strcpy(tun_name, "tun77");

	tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI);
	if (tun_fd < 0) {
		fprintf(stderr, "tun allocation failed\n");
		exit(1);
	}

	printf("TUN device : %s\n", tun_name);

	/* open nvme device */
	nvme_fd = open(nvme_dev, O_RDONLY);
	if (nvme_fd < 0) {
		fprintf(stderr, "nvme open error\n");
		exit(1);
	}

	if (pthread_create(&tid, NULL, &nvme_to_ip, NULL) != 0) {
		fprintf(stderr, "pthread create error\n");
		exit(1);
	}

	ip_to_nvme();

	return 0;
}

/* vim: set ts=8 sw=8 noet: */
