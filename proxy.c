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

#define INIT_BUF_SIZE 4096
#define LISTENING_PORT 3000
#define TARGET_PORT "3001"
#define TARGET_HOST "localhost"

int main()
{

    int localFd = socket_listen(LISTENING_PORT);
    struct connection *accepted_con = connection_accept(localFd);
    int init;

    printf("accepted con on fd: %d\n", accepted_con->fd);

    init = connection_init(accepted_con, INIT_BUF_SIZE);
    
    if (init == -1)
    {
        fprintf(stderr, "Failed to initialize connection\n");
        exit(1);
    }

    connection_read_request(accepted_con);
    
    struct connection *target_con = connection_connect(TARGET_HOST, TARGET_PORT);
    printf("target fd: %d\n", target_con->fd);
    init = connection_init(target_con, INIT_BUF_SIZE);
    if (init == -1)
    {
        fprintf(stderr, "Failed to initialize connection\n");
        exit(1);
    }

    printf("%s\n", accepted_con->rbuf);
    int written = write(target_con->fd, accepted_con->rbuf, accepted_con->rbuf_count);

    if (written == -1)
    {
        fprintf(stderr, "Write went wrong\n");
        exit(1);
    }

    connection_read_response(target_con);

    written = write(accepted_con->fd, target_con->rbuf, target_con->rbuf_count);
    if (written == -1)
    {
        fprintf(stderr, "Write failed\n");
    }
    close(accepted_con->fd);
    close(target_con->fd);
    free(target_con->rbuf);
    free(target_con);
    free(accepted_con->rbuf);
    free(accepted_con);
    return 0;
}