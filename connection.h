#define BACKLOG 100

struct connection
{
    int fd;
    struct addrinfo *res;
    int rbuf_size;
    int rbuf_count;
    char *rbuf;
    struct connection *next;
    int target_fd;
    int done_writing;
};

void connection_read_response(struct connection *target_con);

void connection_read_request(struct connection *accepted_con);

int connection_init(struct connection *connection, int buf_size);

struct connection *connection_connect(char *ip, char *port);

int socket_listen(int port);

struct connection *connection_accept(int acceptor);

int connection_add_connection(struct connection *connection, struct connection **head);

int connection_delete(int fd, struct connection **head);

struct connection *connection_find_first_by_fd(int fd, struct connection *head);

struct connection *connection_find_first_by_target_fd(int target_fd, struct connection *head);