#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <time.h>

#define SOCKET_PATH "/tmp/unix_socket"
#define BUFFER_SIZE 1024
#define NUM_MESSAGES 10000

int main() {
    int client_fd;
    struct sockaddr_un server_addr;
    char buffer[BUFFER_SIZE] = "Test message";
    struct timespec start, end;

    // 创建 UNIX socket
    client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 连接服务器
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    // 发送和接收数据
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < NUM_MESSAGES; i++) {
        send(client_fd, buffer, BUFFER_SIZE, 0);
        recv(client_fd, buffer, BUFFER_SIZE, 0);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    // 计算传输延迟
    double elapsed_time = (end.tv_sec - start.tv_sec) * 1e3 + (end.tv_nsec - start.tv_nsec) / 1e6;
    printf("Total time for %d messages: %.2f ms\n", NUM_MESSAGES, elapsed_time);
    printf("Average time per message: %.2f ms\n", elapsed_time / NUM_MESSAGES);

    close(client_fd);
    return 0;
}