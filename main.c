/* https://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <linux/errqueue.h>
#include <sys/types.h>
#include <sys/socket.h>

int main(void)
{
	int ret = 0;
	int sockfd = 0;

	sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

	if (-1 == sockfd) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	/* see timestamping.txt in kernel-doc
	 * SO_TIMESTAMP: provides usec precision, not Y2038 safe
	 * SO_TIMESTAMP_NEW: provides usec precision, Y2038 safe
	 * SO_TIMESTAMPNS: provides ns precision, not Y2038 safe
	 * SO_TIMESTAMPNS_NEW: provides ns precision, Y2038 safe
	 */

	const int enable = 1;
	ret = setsockopt(sockfd, SOL_SOCKET, SO_TIMESTAMPNS_NEW, &enable, sizeof(enable));

	if (-1 == ret) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in listen_addr = { 0 };
	listen_addr.sin_family = AF_INET;
	listen_addr.sin_addr.s_addr = INADDR_ANY;
	listen_addr.sin_port = htons(9001);

	ret = bind(sockfd, (const struct sockaddr*) &listen_addr, sizeof(listen_addr));

	if (-1 == ret) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

	/* in the cmsg we expect to receive either:
	 * usec: `struct scm_timestamping` which is an array of 3 `struct timeval`
	 * ns: `struct scm_timestamping64` which is an array of 3 `strict timespec
	 */

	struct msghdr my_msgh = { 0 };
	uint8_t my_ctrlbuf[CMSG_SPACE(sizeof(struct scm_timestamping64))];

	my_msgh.msg_control = my_ctrlbuf;
	my_msgh.msg_controllen = sizeof(my_ctrlbuf);

	/* the packet data payload will be received into the iov  */

	struct iovec my_iov = { 0 };
	uint8_t my_databuf[1024] = { 0 };

	my_iov.iov_base = my_databuf;
	my_iov.iov_len = sizeof(my_databuf);

	my_msgh.msg_iov = &my_iov;
	my_msgh.msg_iovlen = 1;

	/* the whole purpose of this is to measure the latency between packet
	 * recv at the NIC and poll return, so poll the socket */

	struct pollfd fds[1] = { 0 };
	fds[0].fd = sockfd;
	fds[0].events = POLLIN;

	struct timespec realtime_before_poll = { 0 };
	struct timespec monotime_after_poll = { 0 };
	struct timespec realtime_after_poll = { 0 };
	struct timespec realtime_after_recv = { 0 };
	//struct scm_timestamping packet_time = { 0 };
	struct scm_timestamping64 packet_time = { 0 };

	while (true) {
		clock_gettime(CLOCK_REALTIME, &realtime_before_poll);

		ret = poll(fds, 1, 60000);

		clock_gettime(CLOCK_REALTIME,  &realtime_after_poll);
		clock_gettime(CLOCK_MONOTONIC, &monotime_after_poll);

		if (0 == ret) {
			printf("poll timed out\n");
		} else if (-1 == ret) {
			perror("poll");
			exit(EXIT_FAILURE);
		} else {
			/* message waiting */
			ret = recvmsg(sockfd, &my_msgh, 0);
			if (-1 == ret) {
				perror("recvmsg");
				exit(EXIT_FAILURE);
			}

			clock_gettime(CLOCK_REALTIME, &realtime_after_recv);

			struct cmsghdr* cmsg = NULL;

			for (cmsg = CMSG_FIRSTHDR(&my_msgh); NULL != cmsg; cmsg = CMSG_NXTHDR(&my_msgh, cmsg)) {
				if (SOL_SOCKET == cmsg->cmsg_level && SO_TIMESTAMPNS_NEW == cmsg->cmsg_type) {
					memcpy(&packet_time, CMSG_DATA(cmsg), sizeof(packet_time));
					long diff_ns = (realtime_after_recv.tv_sec - packet_time.ts[0].tv_sec) * 1000000 + (realtime_after_recv.tv_nsec - packet_time.ts[0].tv_nsec);

					if (diff_ns > 1) {
						printf("Latency between packet and poll return: %ld ns\n" \
						       " realtime before poll = %10jd s %09ld ns\n" \
						       " packet time          = %10jd s %09lld ns\n" \
						       " realtime after poll  = %10jd s %09ld ns\n" \
						       " monotime after poll  = %10jd s %09ld ns\n" \
						       " realtime after recv  = %10jd s %09ld ns\n\n", \
						       diff_ns,
						       (intmax_t) realtime_before_poll.tv_sec, realtime_before_poll.tv_nsec,
						       (intmax_t) packet_time.ts[0].tv_sec, packet_time.ts[0].tv_nsec,
						       (intmax_t) realtime_after_poll.tv_sec, realtime_after_poll.tv_nsec,
						       (intmax_t) monotime_after_poll.tv_sec, monotime_after_poll.tv_nsec,
						       (intmax_t) realtime_after_recv.tv_sec, realtime_after_recv.tv_nsec);
					}
				}
			}
		}

		memset(&my_ctrlbuf, 0, sizeof(my_ctrlbuf));
		memset(&my_databuf, 0, sizeof(my_databuf));
		memset(&realtime_before_poll, 0, sizeof(realtime_before_poll));
		memset(&monotime_after_poll, 0, sizeof(monotime_after_poll));
		memset(&realtime_after_poll, 0, sizeof(realtime_after_poll));
		memset(&realtime_after_recv, 0, sizeof(realtime_after_recv));
		memset(&packet_time, 0, sizeof(packet_time));
	}

	return EXIT_SUCCESS;
}

