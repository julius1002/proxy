#include "connection.h"
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

static int contains(char *method, char **methods, int n)
{
    for (int i = 0; i < n; i++)
    {
        if (!strcmp(method, methods[i]))
        {
            return 1;
        }
    }
    return 0;
}

void connection_read_response(struct connection *target_con)
{
    int bytes_read;
    char *separator;
    char *str;

    bytes_read = read(target_con->fd, target_con->rbuf, 16);
    target_con->rbuf_count += bytes_read;

    str = strchr(target_con->rbuf, ' ');
    while (bytes_read > 0)
    {
        if (target_con->rbuf_count + 16 >= target_con->rbuf_size)
        {
            target_con->rbuf_size *= 2;
            target_con->rbuf = (char *)realloc(target_con->rbuf, target_con->rbuf_size);
        }

        bytes_read = read(target_con->fd, &target_con->rbuf[target_con->rbuf_count], 16);
        target_con->rbuf_count += bytes_read;

        separator = strstr(target_con->rbuf, "\r\n\r\n");

        if (separator) // we found a separator, headers are done
        {
            char *endptr;
            char *content_length;
            int content_len;
            content_length = strstr(target_con->rbuf, "Content-Length: ");
            if (content_length == NULL)
            {
                fprintf(stderr, "Could not find Content-Length header in headers\n");
                exit(1);
            }

            content_len = strtol(&content_length[strlen("Content-Length: ")], &endptr, 10);
            if (content_len == 0)
            {
                fprintf(stderr, "Could not parse content length\n");
                exit(1);
            }

            int already_read = &target_con->rbuf[target_con->rbuf_count] - separator - strlen("\r\n\r\n");
            int remaining = content_len - already_read;

            while (target_con->rbuf_count + remaining >= target_con->rbuf_size)
            {
                target_con->rbuf_size *= 2;
                target_con->rbuf = (char *)realloc(target_con->rbuf, target_con->rbuf_size);
            }

            bytes_read = read(target_con->fd, &target_con->rbuf[target_con->rbuf_count], remaining);
            target_con->rbuf_count += bytes_read;
            break;
        }
    }

    if (bytes_read == -1)
    {
        fprintf(stderr, "Read went wrong\n");
        exit(1);
    }

    close(target_con->fd);
}

void connection_read_request(struct connection *accepted_con)
{
    char *idx;
    int len;
    int bytes_read;

    bytes_read = read(accepted_con->fd, accepted_con->rbuf, 16);
    accepted_con->rbuf_count += bytes_read;
    printf("current buffer: %s\n", accepted_con->rbuf);
    if (bytes_read < 16)
    {
        fprintf(stderr, "Request too short\n");
        exit(1);
    }

    idx = strchr(accepted_con->rbuf, ' ');
    if (idx == NULL)
    {
        fprintf(stderr, "Could not parse request, expected space\n");
        exit(1);
    }
    len = idx - accepted_con->rbuf;

    char method[len + 1];
    memcpy(method, accepted_con->rbuf, len);

    char *methods[] = {"GET", "PUT", "POST", "DELETE"};
    method[len] = '\0';
    if (!contains(method, methods, 4))
    {
        fprintf(stderr, "Invalid request method: %s\n", method);
        exit(1);
    }
    char *ch;
    while (bytes_read > 0)
    {
        if (accepted_con->rbuf_count >= accepted_con->rbuf_size)
        {
            accepted_con->rbuf_size *= 2;
            accepted_con->rbuf = realloc(accepted_con->rbuf, accepted_con->rbuf_size);
        }
        bytes_read = read(accepted_con->fd, &accepted_con->rbuf[accepted_con->rbuf_count], 16);
        accepted_con->rbuf_count += bytes_read;
        ch = strstr(accepted_con->rbuf, "\r\n\r\n");
        if (ch != NULL) // found the separator
        {
            char *endptr;
            char *content_length;
            int content_len;
            int body_length;
            if (0 == strncmp("GET", method, 3)) // if it is GET, we are ok
            {
                break;
            }
            content_length = strstr(accepted_con->rbuf, "Content-Length: ");
            if (content_length == NULL)
            {
                fprintf(stderr, "Could not find Content-Length header in headers\n");
                exit(1);
            }

            content_len = strtol(&content_length[strlen("Content-Length: ")], &endptr, 10);
            if (content_len == 0)
            {
                fprintf(stderr, "Could not parse content length\n");
                exit(1);
            }
            int already_read = &accepted_con->rbuf[accepted_con->rbuf_count] - ch - strlen("\r\n\r\n") - 1;
            int remaining = content_len - already_read;
            while (accepted_con->rbuf_count + remaining > accepted_con->rbuf_size)
            {
                accepted_con->rbuf_size *= 2;
                accepted_con->rbuf = (char *)realloc(accepted_con->rbuf, accepted_con->rbuf_size);
            }
            bytes_read = read(accepted_con->fd, &accepted_con->rbuf[accepted_con->rbuf_count], remaining);
            accepted_con->rbuf_count += bytes_read;
            break;
        }
    }
}

int connection_init(struct connection *connection, int buf_size)
{
    connection->rbuf_size = buf_size;
    connection->rbuf_count = 0;
    connection->rbuf = (char *)malloc(sizeof(char) * connection->rbuf_size);
    if (connection->rbuf == NULL)
    {
        return -1;
    }
    return 0;
}

struct connection *connection_connect(char *ip, char *port)
{
    struct addrinfo hints, *res;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int getaddrinfo_err = getaddrinfo(ip, port, &hints, &res);
    if (getaddrinfo_err)
    {
        fprintf(stderr, "Error in getaddrinfo\n");
        exit(1);
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    int err = connect(sockfd, res->ai_addr, res->ai_addrlen);
    if (err == -1)
    {
        fprintf(stderr, "Connect went wrong\n");
        exit(1);
    }
    struct connection *con = (struct connection *)malloc(sizeof(struct connection));
    if (con == NULL)
    {
        fprintf(stderr, "Could not allocate connection\n");
        exit(1);
    }
    con->fd = sockfd;
    con->res = res;
    return con;
}

int socket_listen(int port)
{
    struct sockaddr_in6 addr = {};
    addr.sin6_len = sizeof(addr);
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(port);

    int localFd = socket(addr.sin6_family, SOCK_STREAM, 0);
    int on;

    assert(localFd != -1);

    on = 1;
    setsockopt(localFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (bind(localFd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("bind");
        return 1;
    }
    assert(listen(localFd, BACKLOG) != -1);
    return localFd;
}

struct connection *connection_accept(int acceptor)
{
    struct sockaddr_storage addr1;
    socklen_t socklen = sizeof(addr1);
    int connfd;
    connfd = accept(acceptor, (struct sockaddr *)&addr1, &socklen);
    struct connection *accepted_con = (struct connection *)malloc(sizeof(struct connection));
    accepted_con->fd = connfd;
    return accepted_con;
}

int connection_add_connection(struct connection *connection, struct connection **head)
{
    if (head == NULL || connection == NULL)
    {
        return 0;
    }

    connection->next = *head;

    *head = connection;

    return 1;
}

int connection_delete(int fd, struct connection **head)
{
    if (*head == NULL)
    {
        return 0;
    }

    if ((*head)->fd == fd)
    {
        struct connection *h = *head;
        *head = (*head)->next;
        free(h->rbuf);
        if (h->res)
        {
            freeaddrinfo(h->res);
        }
        free(h);
        printf("fd: %d deleted (head)\n", fd);
        return 1;
    }

    struct connection *previous = *head;
    struct connection *current = (*head)->next;

    while (current)
    {
        if (current->fd == fd)
        {
            previous->next = current->next;
            free(current->rbuf);
            if (current->res)
            {
                freeaddrinfo(current->res);
            }
            free(current);
            printf("fd: %d deleted\n", fd);
            return 1;
        }
        previous = current;
        current = current->next;
    }

    return 0;
}

struct connection *connection_find_first_by_fd(int fd, struct connection *head)
{
    struct connection *current = head;
    while (current)
    {
        if (current->fd == fd)
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

struct connection *connection_find_first_by_target_fd(int target_fd, struct connection *head)
{
    struct connection *current = head;
    while (current)
    {
        if (current->target_fd == target_fd)
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}