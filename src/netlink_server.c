#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>

#define NETLINK_ROUTE 0
#define BUFFER_SIZE 1024

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
    src_addr.nl_pid = getpid(); // 服务器的 PID
    src_addr.nl_groups = 0;

    if (bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
        perror("bind");
        close(sock_fd);
        return -1;
    }

    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(BUFFER_SIZE));
    memset(nlh, 0, NLMSG_SPACE(BUFFER_SIZE));
    nlh->nlmsg_len = NLMSG_SPACE(BUFFER_SIZE);
    nlh->nlmsg_pid = getpid(); // 服务器的 PID
    nlh->nlmsg_flags = 0;

    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    printf("Netlink server started...\n");

    while (1) {
        printf("Waiting for message...\n");
        if (recvmsg(sock_fd, &msg, 0) < 0) {
            perror("recvmsg");
            continue;
        }
        printf("Received message: %s\n", (char *)NLMSG_DATA(nlh));
        if (sendmsg(sock_fd, &msg, 0) < 0) {
            perror("sendmsg");
        }
    }

    close(sock_fd);
    return 0;
}