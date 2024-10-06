#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/un.h>
#include <sys/event.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include "connection.h"
#include <string.h>

#define INIT_BUF_SIZE 4096
#define LISTENING_PORT 3000
#define TARGET_PORT "3001"
#define TARGET_HOST "localhost"
#define EVLIST_SIZE 100

void kqueue_add_rw_event(int kq, int fd)
{
    struct kevent evSet;
    EV_SET(&evSet, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
    kevent(kq, &evSet, 1, NULL, 0, NULL);
    EV_SET(&evSet, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
    kevent(kq, &evSet, 1, NULL, 0, NULL);
}

void make_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    assert(flags >= 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void print_list(struct connection *connection)
{
    if (connection == NULL)
    {
        printf("list is empty\n");
    }
    while (connection)
    {
        if (connection->target_fd == -1)
        {
            printf("target fd: ");
        }
        else
        {
            printf("source fd: ");
        }
        printf("%d: %s\n", connection->fd, connection->rbuf);
        connection = connection->next;
    }
    printf("\n");
}

int main()
{
    int localFd = socket_listen(LISTENING_PORT);

    int kq = kqueue();

    struct kevent evSet;
    EV_SET(&evSet, localFd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
    assert(-1 != kevent(kq, &evSet, 1, NULL, 0, NULL));

    struct kevent evList[EVLIST_SIZE];

    make_non_blocking(localFd);

    EV_SET(&evSet, localFd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
    kevent(kq, &evSet, 1, NULL, 0, NULL);

    struct connection *connections;

    while (1)
    {
        // usleep(1000 * 1000);
        int nev = kevent(kq, NULL, 0, evList, 32, NULL);
        for (int i = 0; i < nev; i++)
        {
            int fd = evList[i].ident;

            if (fd == localFd)
            {
                printf("ready to accept\n");
                // accept connection of source
                struct connection *accepted_con = connection_accept(localFd);
                int connfd = accepted_con->fd;
                printf("accepted source con on fd: %d\n", accepted_con->fd);

                if (connection_init(accepted_con, INIT_BUF_SIZE) == -1)
                {
                    fprintf(stderr, "Failed to initialize connection\n");
                    exit(1);
                }

                kqueue_add_rw_event(kq, connfd);
                printf("Got connection!\n");
                make_non_blocking(connfd);
                connection_add_connection(accepted_con, &connections);
                printf("added source con: %d\n", accepted_con->fd);
                // open connection to target
                struct connection *target_con = connection_connect(TARGET_HOST, TARGET_PORT);

                if (connection_init(target_con, INIT_BUF_SIZE) == -1)
                {
                    fprintf(stderr, "Failed to initialize connection\n");
                    exit(1);
                }
                make_non_blocking(target_con->fd);
                kqueue_add_rw_event(kq, target_con->fd);

                accepted_con->target_fd = target_con->fd;
                target_con->target_fd = -1;

                connection_add_connection(target_con, &connections);
                printf("added target con: %d\n", target_con->fd);
            }
            else if (evList[i].filter == EVFILT_READ)
            {
                printf("%lu ready to read\n", evList[i].ident);
                struct connection *found_con = connection_find_first_by_fd(fd, connections);
                if (found_con->target_fd == -1 && !found_con->rbuf_count)
                { // target fd
                    connection_read_response(found_con);
                    printf("reading response from target %s\n", found_con->rbuf);
                }
                else if (found_con->target_fd != -1 && !found_con->rbuf_count && !found_con->done_writing)
                { // must be source fd
                    connection_read_request(found_con);
                    printf("reading request from source %s\n", found_con->rbuf);
                }
            }
            else if (evList[i].filter == EVFILT_WRITE)
            {
                struct connection *found_con = connection_find_first_by_fd(fd, connections);
                if (found_con != NULL && found_con->target_fd == -1)
                {
                    printf("ready to write to fd %d\n", found_con->fd);
                    struct connection *source = connection_find_first_by_target_fd(found_con->fd, connections);
                    if (source != NULL && source->rbuf_count)
                    {
                        printf("writing to target server: %s\n", source->rbuf);
                        int bytes_written = write(found_con->fd, source->rbuf, source->rbuf_count);
                        memset(source->rbuf, 0, source->rbuf_size);
                        source->rbuf_count = 0;
                        source->done_writing = 1;
                        printf("written %d to target from source\n", bytes_written);
                    }
                }
                else
                {
                    struct connection *target = connection_find_first_by_fd(found_con->target_fd, connections);
                    if (target != NULL && target->rbuf_count)
                    {
                        printf("writing to source connection: %s\n", target->rbuf);
                        int bytes_written = write(found_con->fd, target->rbuf, target->rbuf_count);
                        close(found_con->fd);
                        close(target->fd);
                        printf("closing %d and %d\n", found_con->fd, target->fd);
                        memset(target->rbuf, 0, target->rbuf_size);
                        target->rbuf_count = 0;
                        connection_delete(found_con->fd, &connections);
                        connection_delete(target->fd, &connections);
                    }
                }
            }
            else if (evList[i].flags & EV_EOF)
            {
                printf("%lu disconnected\n", evList[i].ident);
                close(fd);
                connection_delete(fd, &connections);
            }
            print_list(connections);
        }
    }
    return 0;
}