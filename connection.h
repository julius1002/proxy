
struct connection
{
    int fd;
    struct addrinfo *res;
    int rbuf_size;
    int rbuf_count;
    char *rbuf;
};

void connection_read_response(struct connection *target_con);

void connection_read_request(struct connection *accepted_con);

int connection_init(struct connection *connection, int buf_size);

struct connection *connection_connect(char *ip, char *port);

int socket_listen(int port);

struct connection *connection_accept(int acceptor);