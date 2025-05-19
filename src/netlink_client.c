#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>
#include <time.h>

#define NETLINK_ROUTE 0
#define BUFFER_SIZE 1024
#define NUM_MESSAGES 10000

struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
int sock_fd;
struct msghdr msg;

int main() {
    sock_fd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE); // 使用 SOCK_DGRAM 和 NETLINK_ROUTE
    if (sock_fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid(); // 客户端的 PID
    src_addr.nl_groups = 0;

    if (bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
        perror("bind");
        close(sock_fd);
        return -1;
    }

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0; // 发送到内核
    dest_addr.nl_groups = 0;

    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(BUFFER_SIZE));
    memset(nlh, 0, NLMSG_SPACE(BUFFER_SIZE));
    nlh->nlmsg_len = NLMSG_SPACE(BUFFER_SIZE);
    nlh->nlmsg_pid = getpid(); // 客户端的 PID
    nlh->nlmsg_flags = 0;

    strcpy(NLMSG_DATA(nlh), "Test message");

    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    struct timespec start, end;

    // 发送和接收数据
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < NUM_MESSAGES; i++) {
        printf("Sending message %d...\n", i + 1);
        if (sendmsg(sock_fd, &msg, 0) < 0) {
            perror("sendmsg");
            close(sock_fd);
            return -1;
        }
        if (recvmsg(sock_fd, &msg, 0) < 0) {
            perror("recvmsg");
            close(sock_fd);
            return -1;
        }
        printf("Received response %d: %s\n", i + 1, (char *)NLMSG_DATA(nlh));
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    // 计算传输延迟
    double elapsed_time = (end.tv_sec - start.tv_sec) * 1e3 + (end.tv_nsec - start.tv_nsec) / 1e6;
    printf("Total time for %d messages: %.2f ms\n", NUM_MESSAGES, elapsed_time);
    printf("Average time per message: %.2f ms\n", elapsed_time / NUM_MESSAGES);

    close(sock_fd);
    return 0;
}