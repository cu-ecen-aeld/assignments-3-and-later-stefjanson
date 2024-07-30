#define _POSIX_C_SOURCE 200112L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/stat.h>
#include <syslog.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>

int srv_fd, b_res, c_res;
#define LISTEN_PORT "9000"
#define BUF_SIZE 128
#define FILE_PATH "/var/tmp/aesdsocketdata"

void array_to_str(char *src, int len, char *dest) {
    for(int i = 0; i < len; ++i) {
        dest[i] = src[i];
    }
    dest[len] = '\0';
}

struct data_node {
    char *data;
    int length;
    struct data_node *next;
};

void append_data_node(struct data_node **head, struct data_node **tail, char *buffer, int length) {
    struct data_node *node = malloc(sizeof(struct data_node));
    node->length = length;
    node->next = NULL;
    node->data = malloc(length);
    memcpy(node->data, buffer, length);

    if (*head == NULL) {
        *head = node;
        *tail = node;
    } else {
        (*tail)->next = node;
        *tail = node;
    }
}

void free_data_nodes(struct data_node *head) {
    while (head != NULL) {
        struct data_node *temp = head;
        head = head->next;
        free(temp->data);
        free(temp);
    }
}

struct worker {
    pthread_t thread_id;
    int socket_fd;
    int completed;
    struct sockaddr address;
    struct worker *next;
};

struct worker *worker_head = NULL;
struct worker *worker_tail = NULL;

void add_worker(pthread_t thread_id, int fd) {
    struct worker *node = malloc(sizeof(struct worker));
    node->thread_id = thread_id;
    node->socket_fd = fd;
    node->completed = 0;
    node->next = NULL;

    if (worker_head == NULL) {
        worker_head = node;
        worker_tail = node;
    } else {
        worker_tail->next = node;
        worker_tail = node;
    }
}

void cleanup_worker(struct worker *node) {
    close(node->socket_fd);
    pthread_cancel(node->thread_id);
    pthread_join(node->thread_id, NULL);
    free(node);
}

void clean_worker_list() {
    struct worker *current = worker_head;
    while (current != NULL) {
        if (current->completed == 1) {
            pthread_join(current->thread_id, NULL);
            current->completed = 2;
        }
        current = current->next;
    }
}

void mark_thread_completed(pthread_t thread_id) {
    struct worker *current = worker_head;
    while (current != NULL) {
        if (current->thread_id == thread_id) {
            current->completed = 1;
        }
        current = current->next;
    }
}

void remove_all_workers() {
    while (worker_head != NULL) {
        struct worker *temp = worker_head;
        worker_head = worker_head->next;
        if (temp->completed == 0) {
            close(temp->socket_fd);
            pthread_cancel(temp->thread_id);
        }
        if (temp->completed != 2) {
            close(temp->socket_fd);
            pthread_join(temp->thread_id, NULL);
        }
        free(temp);
    }
}

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

struct sockaddr server_addr;
struct addrinfo *addr_info;
struct addrinfo addr_hints;
socklen_t addr_size;
int data_file_desc;
pthread_t timestamp_thread = 0;

static void sig_handler(int sig_num);
void *client_handler(void *sock_fd);
void *timestamp_generator(void *arg);

int main(int argc, char *argv[]) {
    openlog(NULL, 0, LOG_USER);
    int run_as_daemon = 0;

    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        run_as_daemon = 1;
    }

    srv_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (srv_fd == -1) {
        syslog(LOG_ERR, "Socket creation failed.");
        return -1;
    }

    addr_hints.ai_family = AF_INET;
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_flags = AI_PASSIVE;

    int addr_result = getaddrinfo(NULL, LISTEN_PORT, &addr_hints, &addr_info);
    if (addr_result == -1) {
        syslog(LOG_ERR, "getaddrinfo error: %s", gai_strerror(addr_result));
        return -1;
    }

    int reuse = 1;
    int opt_res = setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (opt_res != 0) {
        syslog(LOG_ERR, "Error setting socket options");
    }

    b_res = bind(srv_fd, addr_info->ai_addr, addr_info->ai_addrlen);
    if (b_res == -1) {
        syslog(LOG_ERR, "Socket binding failed.");
        return -1;
    }

    if (run_as_daemon) {
        pid_t pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR, "Fork failed");
            return -1;
        } else if (pid > 0) {
            return 0;
        }
    }

    struct sigaction sig_act;
    memset(&sig_act, 0, sizeof(sig_act));
    sig_act.sa_handler = sig_handler;

    if (sigaction(SIGTERM, &sig_act, NULL) != 0) {
        syslog(LOG_ERR, "Error %d (%s) registering for SIGTERM", errno, strerror(errno));
        return -1;
    }
    if (sigaction(SIGINT, &sig_act, NULL) != 0) {
        syslog(LOG_ERR, "Error %d (%s) registering for SIGINT", errno, strerror(errno));
        return -1;
    }

    c_res = listen(srv_fd, 10);
    if (c_res == -1) {
        syslog(LOG_ERR, "Server unable to listen for connections.");
        return -1;
    }

    data_file_desc = open(FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH);
    if (data_file_desc < 0) {
        syslog(LOG_ERR, "Error creating file for writing");
        return -1;
    }

    pthread_create(&timestamp_thread, NULL, timestamp_generator, NULL);

    while (1) {
        struct worker *new_worker = malloc(sizeof(struct worker));
        addr_size = sizeof(new_worker->address);
        int client_fd = accept(srv_fd, &new_worker->address, &addr_size);
        if (client_fd == -1) {
            syslog(LOG_ERR, "Server unable to accept client connection.");
            return -1;
        }

        pthread_t worker_thread = 0;
        new_worker->socket_fd = client_fd;
        pthread_create(&worker_thread, NULL, client_handler, (void*)new_worker);
        add_worker(worker_thread, client_fd);

        clean_worker_list();
    }
    return 0;
}

void *client_handler(void *sock_fd_ptr) {
    struct worker *worker_info = (struct worker*)sock_fd_ptr;
    int client_fd = worker_info->socket_fd;

    char ip_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &((struct sockaddr_in*)&worker_info->address)->sin_addr.s_addr, ip_str, INET_ADDRSTRLEN)) {
        syslog(LOG_SYSLOG, "Accepted connection from %s\n", ip_str);
    }

    free(worker_info);

    char buffer[BUF_SIZE];
    while (1) {
        int should_exit = 0;
        int prev_state;

        struct data_node *data_head = NULL;
        struct data_node *data_tail = NULL;

        ssize_t received_bytes;
        do {
            received_bytes = recv(client_fd, buffer, BUF_SIZE, 0);
            if (received_bytes <= 0) {
                if (received_bytes < 0) {
                    syslog(LOG_ERR, "Socket recv error: %ld errno: %d", received_bytes, errno);
                }
                should_exit = 1;
                break;
            }
            append_data_node(&data_head, &data_tail, buffer, received_bytes);
        } while (buffer[received_bytes - 1] != '\n');

        struct data_node *current_node = data_head;

        if (should_exit) {
            syslog(LOG_SYSLOG, "Closed connection from %s\n", ip_str);
            break;
        }

        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &prev_state);
        pthread_mutex_lock(&mtx);
        while (current_node != NULL) {
            write(data_file_desc, current_node->data, current_node->length);
            current_node = current_node->next;
        }
        pthread_mutex_unlock(&mtx);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &prev_state);

        free_data_nodes(data_head);

        int read_fd = open(FILE_PATH, O_RDONLY, 0);
        if (read_fd < 0) {
            syslog(LOG_ERR, "Error opening file for reading");
        }
        FILE *file_stream = fdopen(read_fd, "r");

        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &prev_state);
        while (fgets(buffer, sizeof(buffer), file_stream) != NULL) {
            int sent_bytes = send(client_fd, buffer, strlen(buffer), 0);
            if (sent_bytes < 0) {
                should_exit = 1;
                break;
            }
        }
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &prev_state);

        close(read_fd);

        if (should_exit) {
            syslog(LOG_SYSLOG, "Connection closed from %s\n", ip_str);
            break;
        }
    }

    close(client_fd);
    mark_thread_completed(pthread_self());
    pthread_exit(0);
}

void *timestamp_generator(void *arg) {
    (void)arg;
    struct timeval tv;
    struct tm* tm_info;
    char time_buffer[50];

    while (1) {
        gettimeofday(&tv, NULL);
        tm_info = localtime(&tv.tv_sec);
        strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);
        int len = strlen(time_buffer);
        time_buffer[len] = '\n';
        time_buffer[len+1] = '\0';

        pthread_mutex_lock(&mtx);
        write(data_file_desc, "timestamp:", 10);
        write(data_file_desc, time_buffer, len+1);
        pthread_mutex_unlock(&mtx);
        sleep(10);
    }
}

static void sig_handler(int sig_num) {
    (void)sig_num;
    syslog(LOG_SYSLOG, "Caught signal, exiting\n");

    remove_all_workers();
    close(srv_fd);
    close(data_file_desc);
    remove(FILE_PATH);

    pthread_cancel(timestamp_thread);
    pthread_join(timestamp_thread, NULL);

    exit(0);
}
