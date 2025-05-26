#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#define NETLINK_TEST NETLINK_USERSOCK
// #define NETLINK_TEST 31
#define MAX_PAYLOAD 1024
#define NUM_CLIENTS 3
#define NUM_MESSAGES 10

typedef struct {
    int client_id;
    int sock_fd;
    struct sockaddr_nl src_addr;
    struct sockaddr_nl dest_addr;
} ClientArgs;

// 服务器线程函数
void* server_thread(void* arg) {
    int sock_fd;
    struct sockaddr_nl src_addr, dest_addr;
    struct nlmsghdr* nlh = NULL;
    struct iovec iov;
    struct msghdr msg;

    // 创建 Netlink 套接字
    sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_TEST);
    if (sock_fd < 0) {
        perror("Server socket creation failed");
        pthread_exit(NULL);
    }

    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid(); // 服务器的 PID
    src_addr.nl_groups = 0;

    if (bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr)) < 0) {
        perror("Server bind failed");
        close(sock_fd);
        pthread_exit(NULL);
    }

    nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    iov.iov_base = (void*)nlh;
    iov.iov_len = NLMSG_SPACE(MAX_PAYLOAD);
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void*)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    printf("Server started...\n");

    int total_received = 0; // 成功接收的消息数量
    int total_lost = 0;     // 丢失的消息数量

    while (1) {
        printf("Waiting for message...\n");
        if (recvmsg(sock_fd, &msg, 0) < 0) {
            perror("Server recvmsg failed");
            total_lost++; // 增加丢失计数
            continue;
        }
        total_received++; // 增加成功接收计数
        printf("Server received: %s\n", (char*)NLMSG_DATA(nlh));

        // Echo the message back to the client
        if (sendmsg(sock_fd, &msg, 0) < 0) {
            perror("Server sendmsg failed");
        }

        // 打印当前丢失率
        printf("Total received: %d, Total lost: %d, Loss rate: %.2f%%\n",
            total_received, total_lost,
            (total_lost * 100.0) / (total_received + total_lost));
        }

        free(nlh);
        close(sock_fd);
        pthread_exit(NULL);
    }

// 客户端线程函数
void* client_thread(void* arg) {
    ClientArgs* client_args = (ClientArgs*)arg;
    struct nlmsghdr* nlh = NULL;
    struct iovec iov;
    struct msghdr msg;
    char buffer[MAX_PAYLOAD];

    nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = client_args->src_addr.nl_pid; // 客户端的 PID
    nlh->nlmsg_flags = 0;

    iov.iov_base = (void*)nlh;
    iov.iov_len = NLMSG_SPACE(MAX_PAYLOAD);
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void*)&client_args->dest_addr;
    msg.msg_namelen = sizeof(client_args->dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    int total_received = 0; // 成功接收的消息数量
    int total_lost = 0;     // 丢失的消息数量

    for (int i = 0; i < NUM_MESSAGES; i++) {
        snprintf(buffer, MAX_PAYLOAD, "Client %d: Message %d", client_args->client_id, i + 1);
        strcpy(NLMSG_DATA(nlh), buffer);

        if (sendmsg(client_args->sock_fd, &msg, 0) < 0) {
            perror("Client sendmsg failed");
            continue;
        }

        if (recvmsg(client_args->sock_fd, &msg, 0) < 0) {
            perror("Client recvmsg failed");
            total_lost++; // 增加丢失计数
            continue;
        }
        total_received++; // 增加成功接收计数
        printf("Client %d received: %s\n", client_args->client_id, (char*)NLMSG_DATA(nlh));

        // 打印当前丢失率
        printf("Client %d - Total received: %d, Total lost: %d, Loss rate: %.2f%%\n",
            client_args->client_id, total_received, total_lost,
            (total_lost * 100.0) / (total_received + total_lost));
        usleep(10000); // 模拟发送间隔
    }

    free(nlh);
    close(client_args->sock_fd);
    pthread_exit(NULL);
}

int main() {
    pthread_t server_tid, client_tids[NUM_CLIENTS];
    ClientArgs client_args[NUM_CLIENTS];

    // 创建服务器线程
    if (pthread_create(&server_tid, NULL, server_thread, NULL) != 0) {
        perror("Server thread creation failed");
        return -1;
    }

    sleep(1); // 确保服务器启动

    // 创建客户端线程
    for (int i = 0; i < NUM_CLIENTS; i++) {
        client_args[i].client_id = i + 1;
        client_args[i].sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_TEST);
        if (client_args[i].sock_fd < 0) {
            perror("Client socket creation failed");
            return -1;
        }

        memset(&client_args[i].src_addr, 0, sizeof(client_args[i].src_addr));
        client_args[i].src_addr.nl_family = AF_NETLINK;
        client_args[i].src_addr.nl_pid = getpid() + i + 1; // 每个客户端的唯一 PID
        client_args[i].src_addr.nl_groups = 0;

        if (bind(client_args[i].sock_fd, (struct sockaddr*)&client_args[i].src_addr, sizeof(client_args[i].src_addr)) < 0) {
            perror("Client bind failed");
            return -1;
        }

        memset(&client_args[i].dest_addr, 0, sizeof(client_args[i].dest_addr));
        client_args[i].dest_addr.nl_family = AF_NETLINK;
        client_args[i].dest_addr.nl_pid = getpid(); // 服务器的 PID
        client_args[i].dest_addr.nl_groups = 0;

        if (pthread_create(&client_tids[i], NULL, client_thread, &client_args[i]) != 0) {
            perror("Client thread creation failed");
            return -1;
        }
    }

    // 等待客户端线程完成
    for (int i = 0; i < NUM_CLIENTS; i++) {
        pthread_join(client_tids[i], NULL);
    }

    // 服务器线程不会退出，手动终止程序
    pthread_cancel(server_tid);
    pthread_join(server_tid, NULL);

    return 0;
}