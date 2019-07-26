#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
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
#include <sys/mman.h>
#include "dbg_macro.h"
#include <sys/time.h>

#define DBG_PRT(COND, ...) \
    __CONDDEF(COND, fprintf(stderr, __VA_ARGS__);)

#define DBG_PARAM_INITIALIZER(COND) \
    __CONDDEF(COND, \
          struct timeval start_time, end_time; \
          long elap_sec, elap_usec; \
         )

#define DBG_EXEC_TIME(COND,FUNC) \
    __CONDDEF(COND, \
                gettimeofday(&start_time, NULL); \
                FUNC; \
                gettimeofday(&end_time, NULL); \
                elap_sec = end_time.tv_sec - start_time.tv_sec; \
                elap_usec = end_time.tv_usec - start_time.tv_usec; \
                if(elap_usec < 0) { \
                        elap_usec += 1000000; \
                        elap_sec--; \
                } \
                fprintf(stderr, "%ld.%06ld ", elap_sec, elap_usec); \
        ) \
    __CONDNDEF(COND,FUNC)

#define DBG_NVME FALSE
#define DBG_IP FALSE


#define BUFSIZE (4096 + 10)

int BLOCK_SIZE = 512;
int MTU_SIZE = 1500;
static inline int _nblocks() { return ((MTU_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE) - 1; }

#define NBLOCKS _nblocks()

enum nvme_opcode {
    nvme_cmd_flush         = 0x00,
    nvme_cmd_write         = 0x01,
    nvme_cmd_read          = 0x02,
    nvme_cmd_write_uncor   = 0x04,
    nvme_cmd_compare       = 0x05,
    nvme_cmd_write_zeroes  = 0x08,
    nvme_cmd_dsm           = 0x09,
    nvme_cmd_resv_register = 0x0d,
    nvme_cmd_resv_report   = 0x0e,
    nvme_cmd_resv_acquire  = 0x11,
    nvme_cmd_resv_release  = 0x15,
};

static int tun_fd;
static char tun_name[IFNAMSIZ] = {'\0'};

static int nvme_fd;
static char nvme_dev[256] = "/dev/nvme0n1";

static int th_exit = 0;

void sigint_hadler(int signo)
{
    puts("exiting");

    close(tun_fd);
    close(nvme_fd);

    th_exit = 1;

    exit(1);
}

void *nvme_to_ip(void *arg)
{
    int rc;
    int len;
    int nwrite;
    char *buf;
    struct nvme_user_io io = {
        .opcode   = nvme_cmd_read,
        .flags    = 0,
        .control  = 0,
        .nblocks  = 0,
        .rsvd     = 0,
        .metadata = 0,
        .addr     = 0,
        .slba     = 9223372036854775807L,
        .dsmgmt   = 0,
        .reftag   = 0,
        .appmask  = 0,
        .apptag   = 0,
    };
    struct ip_hdr *iphdr;
    DBG_PARAM_INITIALIZER(DBG_NVME);

    io.nblocks = NBLOCKS;

    buf = mmap(0, BUFSIZE, PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
    mlock(buf, BUFSIZE);

    while (!th_exit) {
        /* read from nvme */
        io.addr = (uintptr_t)buf;
        DBG_EXEC_TIME(DBG_NVME,
            rc = ioctl(nvme_fd, NVME_IOCTL_SUBMIT_IO, &io);
            if (rc != 0) {
                usleep(1);
                continue;
            }
            DBG_PRT(DBG_NVME, "%s:", __func__);
        );

        /* check length and format */
        iphdr = (void *)buf;
        len = ntohs(iphdr->len);

        if (len <= 20) {
            fprintf(stderr, "n2i : wrong ip format\n");
            continue;
        }

        if (iphdr->version != IPV4) {
            fprintf(stderr, "n2i : not IPV4\n");
            continue;
        }

        /* write to tun */
        DBG_EXEC_TIME(DBG_NVME,
            nwrite = write(tun_fd, buf, len);
        );
        DBG_PRT(DBG_NVME, "\n");

        if (nwrite < len) {
            fprintf(stderr, "n2i : write error\n");
        }
    }
    return NULL;
}

void *ip_to_nvme(void *arg)
{
    int rc;
    int nread;
    char *buf;
    struct nvme_user_io io = {
        .opcode = nvme_cmd_write,
        .flags    = 0,
        .control  = 0,
        .nblocks  = 0,
        .rsvd     = 0,
        .metadata = 0,
        .addr     = 0,
        .slba     = 9223372036854775807L,
        .dsmgmt   = 0,
        .reftag   = 0,
        .appmask  = 0,
        .apptag   = 0,
    };
    DBG_PARAM_INITIALIZER(DBG_IP);

    io.nblocks = NBLOCKS;

    buf = mmap(0, BUFSIZE, PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
    mlock(buf, BUFSIZE);

    while (!th_exit) {
        /* read from tun */
        DBG_PRT(DBG_IP, "%s:", __func__);

        DBG_EXEC_TIME(DBG_IP,
            nread = read(tun_fd, buf, BUFSIZE);
        );
        if (nread < 0) {
            fprintf(stderr, "i2n : read error\n");
            close(tun_fd);
            return NULL;
        }
        /* write to nvme */
        io.addr = (uintptr_t)buf;
        DBG_EXEC_TIME(DBG_IP,
            rc = ioctl(nvme_fd, NVME_IOCTL_SUBMIT_IO, &io);
        );

        DBG_PRT(DBG_IP, "\n");
        if (rc != 0) {
            if (rc < 0)
                fprintf(stderr, "i2n : ioctl errorno %s\n", strerror(errno));
            else
                fprintf(stderr, "i2n : ioctl error (%d)\n", rc);
            continue;
        }
    }
    return NULL;
}

int main(int argc, char **argv)
{
    int opt;
    pthread_t tid, tid2;
    char addr[16] = {'\0'};
    char dst_addr[16] = {'\0'};
    char netmask[16] = {'\0'};

    signal(SIGINT, sigint_hadler);

    while ((opt = getopt(argc, argv, "t:i:p:n:m:h")) != -1) {
        switch (opt) {
        case 't':
            strncpy(tun_name, optarg, IFNAMSIZ - 1);
            break;
        case 'i':
            strncpy(addr, optarg, 15);
            strcpy(netmask, "255.255.255.0");
            break;
        case 'p':
            strncpy(dst_addr, optarg, 15);
            break;
        case 'n':
            strncpy(nvme_dev, optarg, 255);
            break;
        case 'm':
            MTU_SIZE = atoi(optarg);
            break;
        case 'h':
        default:
            goto usage;
        }
    }

    /* allocate tun device */
    tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI);
    if (tun_fd < 0) {
        fprintf(stderr, "tun allocation failed\n");
        exit(1);
    }

    printf("TUN device : %s\n", tun_name);

    /* set ip address */
    if ((*dst_addr != '\0' && if_addr_p2p(tun_name, addr, dst_addr)) ||
        (*addr != '\0' && if_addr(tun_name, addr, netmask) < 0)) {
        fprintf(stderr, "ip addr set failed\n");
        exit(1);
    }

    /* set mtu size */
    if (if_mtu(tun_name, MTU_SIZE)) {
        fprintf(stderr, "mtu size set failed\n");
        exit(1);
    }

    /* open nvme device */
    nvme_fd = open(nvme_dev, O_RDONLY);
    if (nvme_fd < 0) {
        fprintf(stderr, "nvme open error\n");
        exit(1);
    }

    if (pthread_create(&tid, NULL, &nvme_to_ip, NULL) != 0) {
        fprintf(stderr, "pthread create error t1\n");
        exit(1);
    }
    if (pthread_create(&tid2, NULL, &ip_to_nvme, NULL) != 0) {
        fprintf(stderr, "pthread create error t2\n");
        exit(1);
    }

    pthread_join(tid, NULL);
    pthread_join(tid2, NULL);

    return 0;

usage:
    fprintf(stderr, "Usage: %s [OPTION]\n", argv[0]);
    fprintf(stderr, " -t <IF_NAME>        specify tun device name\n");
    fprintf(stderr, " -i <ADDRESS>        set ip address\n");
    fprintf(stderr, " -p <ADDRESS>        point-to-point mode. set destination ip address\n");
    fprintf(stderr, " -n <DEV_NVME>       nvme device\n");
    fprintf(stderr, " -m <SIZE>           MTU size\n");
    fprintf(stderr, " -h                  display this help info\n");

    return 1;
}

/* vim: set sts=4 sw=4 et: */
