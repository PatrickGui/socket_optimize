#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <stdint.h>
#include <linux/netlink.h>
#include <time.h>

//#define NETLINK_USER 31
#define NETLINK_USER NETLINK_USERSOCK

#define MAX_PAYLOAD 1024
#define ITERATIONS 100000  // 测试迭代次数

// 共享变量测试用的全局数据
typedef struct {
    int data;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int ready;
} SharedData;

SharedData shared_data;

// Netlink测试用的全局数据
int netlink_sock_fd;
uint32_t netlink_pid;

// 计时器函数
double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}


/* ===== 共享变量实现部分 ===== */

void* shared_producer(void* arg) {
    double* elapsed = (double*)arg;
    double start = get_time();

    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&shared_data.mutex);
        
        // 等待消费者准备好
        while (shared_data.ready) {
            pthread_cond_wait(&shared_data.cond, &shared_data.mutex);
        }
        
        // 生产数据
        shared_data.data = i;
        shared_data.ready = 1;
        
        // 通知消费者
        pthread_cond_signal(&shared_data.cond);
        pthread_mutex_unlock(&shared_data.mutex);
    }

    *elapsed = get_time() - start;
    return NULL;
}

void* shared_consumer(void* arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&shared_data.mutex);
        
        // 等待数据就绪
        while (!shared_data.ready) {
            pthread_cond_wait(&shared_data.cond, &shared_data.mutex);
        }
        
        // 消费数据（这里只是读取，不做实际处理）
        int data = shared_data.data;
        
        // 标记已消费
        shared_data.ready = 0;
        
        // 通知生产者
        pthread_cond_signal(&shared_data.cond);
        pthread_mutex_unlock(&shared_data.mutex);
    }
    return NULL;
}


/* ===== Netlink实现部分 ===== */

void* netlink_producer(void* arg) {
    double* elapsed = (double*)arg;
    double start = get_time();
    struct sockaddr_nl dest_addr;
    struct nlmsghdr *nlh = NULL;
    struct msghdr msg;
    struct iovec iov;

    // 设置目标地址
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = netlink_pid;
    dest_addr.nl_groups = 0;

    // 创建消息缓冲区
    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    if (!nlh) {
        perror("malloc failed");
        pthread_exit(NULL);
    }

    for (int i = 0; i < ITERATIONS; i++) {
        nlh->nlmsg_len = NLMSG_SPACE(sizeof(int));
        nlh->nlmsg_pid = getpid();
        nlh->nlmsg_flags = 0;
        
        // 设置消息内容
        memcpy(NLMSG_DATA(nlh), &i, sizeof(int));

        // 设置 I/O 向量和消息头
        iov.iov_base = (void *)nlh;
        iov.iov_len = nlh->nlmsg_len;

        memset(&msg, 0, sizeof(msg));
        msg.msg_name = (void *)&dest_addr;
        msg.msg_namelen = sizeof(dest_addr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        // 发送消息
        if (sendmsg(netlink_sock_fd, &msg, 0) < 0) {
            perror("sendmsg failed");
        }
    }

    free(nlh);
    *elapsed = get_time() - start;
    return NULL;
}

void* netlink_consumer(void* arg) {
    struct nlmsghdr *nlh = NULL;
    struct msghdr msg;
    struct iovec iov;
    char buffer[NLMSG_SPACE(MAX_PAYLOAD)];

    // 创建消息缓冲区
    nlh = (struct nlmsghdr *)buffer;

    // 设置 I/O 向量和消息头
    iov.iov_base = (void *)nlh;
    iov.iov_len = NLMSG_SPACE(MAX_PAYLOAD);

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    for (int i = 0; i < ITERATIONS; i++) {
        // 接收消息
        ssize_t bytes_received = recvmsg(netlink_sock_fd, &msg, 0);
        if (bytes_received <= 0) {
            perror("recvmsg failed");
            break;
        }
    }
    return NULL;
}


int main() {
    pthread_t producer_tid, consumer_tid;
    double shared_elapsed, netlink_elapsed;

    /* ===== 测试共享变量 ===== */
    printf("Testing shared variable communication...\n");
    
    // 初始化共享数据
    pthread_mutex_init(&shared_data.mutex, NULL);
    pthread_cond_init(&shared_data.cond, NULL);
    shared_data.ready = 0;

    // 创建生产者和消费者线程
    pthread_create(&producer_tid, NULL, shared_producer, &shared_elapsed);
    pthread_create(&consumer_tid, NULL, shared_consumer, NULL);

    // 等待线程结束
    pthread_join(producer_tid, NULL);
    pthread_join(consumer_tid, NULL);

    // 清理资源
    pthread_mutex_destroy(&shared_data.mutex);
    pthread_cond_destroy(&shared_data.cond);

    printf("Shared variable elapsed time: %.6f seconds\n", shared_elapsed);
    printf("Average time per iteration: %.9f seconds\n", shared_elapsed / ITERATIONS);


    /* ===== 测试 Netlink ===== */
    printf("\nTesting Netlink socket communication...\n");
    
    // 创建 Netlink Socket
    netlink_sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_USER);
    if (netlink_sock_fd < 0) {
        perror("socket creation failed");
        return -1;
    }

    // 绑定 Socket
    struct sockaddr_nl src_addr;
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();
    src_addr.nl_groups = 0;

    if (bind(netlink_sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
        perror("bind failed");
        close(netlink_sock_fd);
        return -1;
    }

    netlink_pid = src_addr.nl_pid;

    // 创建生产者和消费者线程
    pthread_create(&producer_tid, NULL, netlink_producer, &netlink_elapsed);
    pthread_create(&consumer_tid, NULL, netlink_consumer, NULL);

    // 等待线程结束
    pthread_join(producer_tid, NULL);
    pthread_join(consumer_tid, NULL);

    // 关闭 Socket
    close(netlink_sock_fd);

    printf("Netlink socket elapsed time: %.6f seconds\n", netlink_elapsed);
    printf("Average time per iteration: %.9f seconds\n", netlink_elapsed / ITERATIONS);


    /* ===== 结果对比 ===== */
    printf("\nPerformance comparison:\n");
    printf("Netlink is %.2f times slower than shared variable.\n", 
           netlink_elapsed / shared_elapsed);

    return 0;
}