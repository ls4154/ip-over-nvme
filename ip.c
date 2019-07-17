#include <string.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/ethernet.h>
#include "ip.h"

int ip_addr(const char *if_name, const char *ip_addr, const char *netmask) {
	struct ifreq ifr;
	struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
	int sockfd = socket(PF_INET, SOCK_RAW, IPPROTO_RAW);

	if (sockfd < 0) {
		return -1;
	}

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);
	ifr.ifr_addr.sa_family = AF_INET;

	inet_pton(AF_INET, ip_addr, &addr->sin_addr);
	if(ioctl(sockfd, SIOCSIFADDR, &ifr) < 0) {
		return -1;
	}

	inet_pton(AF_INET, netmask, &addr->sin_addr);
	if(ioctl(sockfd, SIOCSIFNETMASK, &ifr) < 0) {
		return -1;
	}

	if(ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
		return -1;
	}
	ifr.ifr_flags |= IFF_UP;

	if(ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
		return -1;
	}

	return 0;
}
