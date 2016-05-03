#include <cstdio>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <unistd.h>
#include <assert.h>
#include <ctime>
#include "common.h"

#define MAX_CLIENTS 20
#define MAX_PENDING_CLIENTS 10
#define log printf

void clear_client(int id);
void die(bool success, const char *reason);
void* safe_malloc(size_t bytes);
ssize_t safe_single_read(int fd, void *buf, size_t count, int who);
void init_globals();
void parse_arguments(int argc, char **argv);
void set_up_listener();
int get_unused_client_socket();
void accept_client();
void send_message(int from, int to);
void handle_client(int id);
void main_loop();


/** *** *** *** *** *** DATA SECTION *** *** *** *** *** **/


struct MessageBuffer
{
    bool rcvd_only_first_byte;
    short to_receive; //-1 means we didnt started receiving
    unsigned short received;
    char* data;
};

unsigned short server_port;
char *server_port_str;
bool should_quit;

MessageBuffer queue[MAX_CLIENTS + 1];
struct pollfd client[MAX_CLIENTS + 1];

/** *** *** *** *** *** AUX FUNCTIONS SECTION *** *** *** *** *** **/

//returns positive if succeeded
int get_unused_client_socket()
{
    for(int i = 1; i <= MAX_CLIENTS; i++)
        if(client[i].fd == -1)
            return i;
    return (-1);
}

//accept, close
void accept_client()
{
    int client_socket, socket_pos;
    negative_is_bad(client_socket = accept(client[0].fd, NULL, NULL), "accept failure");
    socket_pos = get_unused_client_socket();
    if(socket_pos <= 0) negative_is_bad(close(client_socket), "could not close new client socket");
    else client[socket_pos].fd = client_socket;
    log("Accepted client, socket position: %d\n", socket_pos);
}

//close
void clear_client(int id)
{
    if(client[id].fd != -1)
        negative_is_bad(close(client[id].fd), "could not close a socket");
    client[id].fd = -1;
    queue[id].to_receive = -1;
    queue[id].rcvd_only_first_byte = false;
    queue[id].received = 0;
}

//assumption: every write finishes immediately
//write
void send_message(int from, int to)
{
    queue[from].received = htons(queue[from].received); //convert to network order
    safe_all_write(client[to].fd, (char *) &(queue[from].received), 2); //send 2 bytes of length
    queue[from].received = ntohs(queue[from].received);
    safe_all_write(client[to].fd, queue[from].data, queue[from].received); //send message
}

//server safe-read
ssize_t safe_single_read(int fd, void *buf, size_t cnt, int who)
{
    ssize_t read_rv;
    negative_is_bad(read_rv = read(fd, buf, cnt), "read failure");
    if(read_rv == 0)
    {
        log("Connection %d closed while reading\n", who);
        clear_client(who);
    }
    return read_rv;
}

/** *** *** **** *** *** SERVER LOGIC SECTION *** *** *** *** *** **/

void init_globals()
{
    server_port = PORT_DEFAULT;
    for(int i = 0; i <= MAX_CLIENTS; i++)
    {
        client[i].fd = -1;
        client[i].events = POLLIN | POLLERR | POLLHUP;
        client[i].revents = 0;

        queue[i].rcvd_only_first_byte = false;
        queue[i].to_receive = -1;
        queue[i].received = 0;
        queue[i].data = NULL;
    }

    for(int i = 1; i <= MAX_CLIENTS; i++)
        queue[i].data = (char*) safe_malloc(sizeof(char) * MAX_MESSAGE_LENGTH);

    should_quit = false;
    log("Globals initialized\n");
}

void parse_arguments(int argc, char **argv)
{
    if(argc == 2 && sscanf(argv[1], "%hu", &server_port) != 1) //port 22 is ok for me
        die(false, "invalid server port number");
    if(argc > 2)
        die(false, "invalid number of arguments");

    server_port_str = (char*) safe_malloc(sizeof(char) * 8);
    sprintf(server_port_str, "%hu", server_port);
    log("Arguments parsed\n");
}

//socket
void set_up_listener()
{
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; //ipv4 wow wow
    hints.ai_socktype = SOCK_STREAM; //tcp so reliable wow
    ((sockaddr_in*) hints.ai_addr)->sin_addr.s_addr = htonl(INADDR_ANY); //remove it to listen on localhost only

    getaddrinfo(NULL, server_port_str, &hints, &result);
    negative_is_bad(client[0].fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol),
              "unable to get listen socket");
    negative_is_bad(bind(client[0].fd, result->ai_addr, result->ai_addrlen), "unable to bind listen socket");
    negative_is_bad(listen(client[0].fd, MAX_PENDING_CLIENTS), "unable to listen on listen socket");
    log("Server is now up on port %hu\n", server_port);
}

void handle_client(int id)
{
    if(client[id].revents & POLLERR)
    {
        log("A kitty has eaten ethernet cable (client %d), closing socket\n", id);
        clear_client(id);
        return;
    }
    if(client[id].revents & POLLHUP)
    {
        log("Client %d closed connection\n", id);
        clear_client(id);
        return;
    }
    if(client[id].revents & POLLIN)
    {
        log("There are some data from client %d\n", id);
        ssize_t read_rv;

        /* IMPORTANT
         * Here i'm assuming that short is always 2-byte
         * which is unportable but more convenient
         *
         * If client sent new message, we try to receive 2-bytes length of the message
         * if succeed, then to_receive is set and we'll receive message (received must be zeroed)
         *
         * If we fail to receive 2 bytes of length and receive 1 instead
         * we leave it in to_receive and wait for the second
         * */

        //@todo check if order is right
        if(queue[id].rcvd_only_first_byte)
        {
            read_rv = safe_single_read(client[id].fd, (char *) &(queue[id].to_receive) + 1, 1, id); //rcv second half to_receive
            assert(read_rv == 1);
            queue[id].rcvd_only_first_byte = false;

            log("Received second length byte (client %d): %hhd\n", id, *((char*) &(queue[id].to_receive) + 1));
            queue[id].to_receive = ntohs((uint16_t) queue[id].to_receive);
            if(!(0 < queue[id].to_receive && queue[id].to_receive <= MAX_MESSAGE_LENGTH)) //incorrect length
                clear_client(id);
        }
        else if(queue[id].to_receive == -1) //waiting for message length (new message)
        {
            read_rv = safe_single_read(client[id].fd, &(queue[id].to_receive), 2, id);
            if(read_rv == 1) //received only first byte of length
            {
                queue[id].rcvd_only_first_byte = true;
                log("Received first length byte (client %d): %hhd\n", id, queue[id].to_receive);
            }
            else
            {
                queue[id].to_receive = ntohs((uint16_t) queue[id].to_receive);
                log("Received full length (client %d): %hd\n", id, queue[id].to_receive);
                if(!(0 < queue[id].to_receive && queue[id].to_receive <= MAX_MESSAGE_LENGTH)) //incorrect length
                    clear_client(id);
            }
        }
        else //receiving next part of message
        {
            read_rv = safe_single_read(client[id].fd, queue[id].data + queue[id].received,
                                       (size_t) queue[id].to_receive, id);
            queue[id].received += read_rv;
            log("Received a part of message (%d out of %d bytes)\n", queue[id].received, queue[id].to_receive);
            if(queue[id].to_receive == queue[id].received)
            {
                log("Broadcasting (%d): %.*s", queue[id].received, queue[id].received, queue[id].data);
                for(int i = 1; i <= MAX_CLIENTS; i++)
                    if(client[i].fd != -1 && i != id)
                        send_message(id, i);
                queue[id].to_receive = -1;
                queue[id].received = 0;
                log("Message has been broad-casted (client %d)\n", id);
            }
        }
    }
}

//@todo replace write/read with send/receive

void main_loop()
{
    int poll_value;
    while(!should_quit)
    {
        //clear revents
        for(int i = 0; i <= MAX_CLIENTS; ++i)
            client[i].revents = 0;

        log("Awaiting for data/connection\n");
        negative_is_bad(poll_value = poll(client, MAX_CLIENTS + 1, -1), "pool failure");
        assert(poll_value > 0); //check if pool is not for ever

        if(client[0].revents & POLLIN) //accepting client
            accept_client();

        for(int i = 1; i <= MAX_CLIENTS; i++)
            if(client[i].fd != -1 && client[i].revents & (POLLIN | POLLERR | POLLHUP))
                handle_client(i);
    }
}

int main(int argc, char **argv)
{
    init_globals();
    parse_arguments(argc, argv);
    set_up_listener();
    main_loop();
    die(true, NULL);

    return 0;
}


void die(bool success, const char* reason)
{
    static bool been_here = false;
    if(been_here) fatal("OS failure");
    been_here = true;
    printf("Server is turning off: %s\n", success ? "successfully" : reason);
    for(int i = 0; i <= MAX_CLIENTS; i++)
        if(client[i].fd >= 0)
            clear_client(i);

    for(int i = 1; i <= MAX_CLIENTS; i++)
        if(queue[i].data != NULL)
            free(queue[i].data);

    if(server_port_str != NULL)
        free(server_port_str);

    exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
}