#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/poll.h>
#include <assert.h>
#include "common.h"

#define PORT_DEFAULT 20160

void die(int, const char *);

/** *** *** *** *** *** DATA SECTION *** *** *** *** *** **/


bool should_quit;
char *server_port_str;
char *server_host_str;
struct pollfd descriptors[2]; //0 - server socket, 1 - stdin

char stdin_buffer[MAX_MESSAGE_LENGTH + 2]; //+1 cuz \n and _1 cuz \0

MessageBuffer socket_buffer;


/** *** *** **** *** *** CLIENT LOGIC SECTION *** *** *** *** *** **/

void parse_arguments(int argc, char **argv)
{
    if(argc >= 2)
    {
        server_host_str = (char *) safe_malloc(128);
        strcpy(server_host_str, argv[1]);

        server_port_str = (char *) safe_malloc(8);
        if(argc == 3)
            strcpy(server_port_str, argv[2]);
        else
            sprintf(server_port_str, "%d", PORT_DEFAULT);
    }
    else
        die(1, "invalid number of arguments");
}

void init_globals()
{
    should_quit = false;
    descriptors[0].events = POLLIN | POLLHUP | POLLERR;
    descriptors[1].events = POLLIN; //dont bother with stdin errors
    descriptors[0].fd = -1;
    descriptors[1].fd = STDIN_FILENO;

    socket_buffer.to_receive = -1;
    socket_buffer.received = 0;
    socket_buffer.rcvd_only_first_byte = false;
    socket_buffer.data = (char *) safe_malloc(sizeof(char) * MAX_MESSAGE_LENGTH);

}

void set_up()
{
    int rv;
    struct addrinfo addr_hints, *addr_result;

    negative_is_bad(descriptors[0].fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP), "cannot get a socket");

    memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_flags = 0;
    addr_hints.ai_family = AF_INET;
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_protocol = IPPROTO_TCP;

    zero_is_ok(rv = getaddrinfo(server_host_str, server_port_str, &addr_hints, &addr_result), gai_strerror(rv));

    negative_is_bad(connect(descriptors[0].fd, addr_result->ai_addr, addr_result->ai_addrlen),
                    "cannot connect to the server");
    freeaddrinfo(addr_result);
}

void handle_stdin()
{
    short length, net_length;
    fgets(stdin_buffer, MAX_MESSAGE_LENGTH+1, stdin); //finishes when it reads MML+1 chars, last is \n, MM+2 is \0
    length = (short) (strlen(stdin_buffer));
    if(stdin_buffer[length-1] == '\n')
        length--;
    else
        while(fgetc(stdin) != 10);

    //do not oount \n
    net_length = htons((uint16_t) length);

    safe_all_write(descriptors[0].fd, (char *) &net_length, sizeof(short)); //all writes end quickly!
    if(length > 0) safe_all_write(descriptors[0].fd, stdin_buffer, (size_t) length);
}

ssize_t safe_single_read(int fd, void *buf, size_t cnt)
{
    ssize_t read_rv;
    negative_is_bad(read_rv = read(fd, buf, cnt), "read failure");
    if(read_rv == 0)
        die(100, "server disconnected");
    return read_rv;
}


void received_full_message()
{
    printf("%.*s\n", socket_buffer.to_receive, socket_buffer.data);
    socket_buffer.to_receive = -1;
    socket_buffer.received = 0;
}

void received_full_length()
{
    if(!(0 <= socket_buffer.to_receive && socket_buffer.to_receive <= MAX_MESSAGE_LENGTH)) //incorrect length
        die(100, "message size");
    if(socket_buffer.to_receive == 0)
        received_full_message();
}


void handle_server_message()
{
    if(descriptors[0].revents & POLLERR)
        die(0, "poll error");
    else if(descriptors[0].revents & POLLHUP)
        die(0, "server disconnected");
    else if(descriptors[0].revents & POLLIN)
    {
        ssize_t read_rv;
        if(socket_buffer.rcvd_only_first_byte)
        {
            read_rv = safe_single_read(descriptors[0].fd, (char *) &(socket_buffer.to_receive) + 1, 1); //rcv second half to_receive
            assert(read_rv == 1);
            socket_buffer.rcvd_only_first_byte = false;
            socket_buffer.to_receive = ntohs((uint16_t) socket_buffer.to_receive);
            received_full_length();
        }
        else if(socket_buffer.to_receive == -1) //waiting for message length (new message)
        {
            read_rv = safe_single_read(descriptors[0].fd, &(socket_buffer.to_receive), 2);
            if(read_rv == 1) //received only first byte of length
                socket_buffer.rcvd_only_first_byte = true;
            else
            {
                socket_buffer.to_receive = ntohs((uint16_t) socket_buffer.to_receive);
                received_full_length();
            }
        }
        else //receiving next part of message
        {
            read_rv = safe_single_read(descriptors[0].fd, socket_buffer.data + socket_buffer.received,
                                       (size_t) socket_buffer.to_receive);
            socket_buffer.received += read_rv;
            if(socket_buffer.to_receive == socket_buffer.received)
                received_full_message();
        }
    }

}

void main_loop()
{
    while(!should_quit)
    {
        //clear revents
        descriptors[0].revents = 0;
        descriptors[1].revents = 0;

        negative_is_bad(poll(descriptors, 2, -1), "pool failure");

        if(descriptors[0].revents & (POLLIN | POLLERR | POLLHUP))
            handle_server_message();
        if(descriptors[1].revents & (POLLIN))
            handle_stdin();
    }
}


int main(int argc, char **argv)
{
    init_globals();
    parse_arguments(argc, argv);
    set_up();
    main_loop();
    die(0, NULL);

}

void die(int code, const char *reason)
{
    static bool been_here = false;
    if(been_here) fatal("OS failure");
    been_here = true;
    if(code != 0) //client cannot output nothing else than messages
        printf("Client is turning off: %s\n", reason);

    if(server_port_str != NULL)
        free(server_port_str);
    if(server_host_str != NULL)
        free(server_host_str);

    if(descriptors[0].fd != -1)
        negative_is_bad(close(descriptors[0].fd), "could not close socket");

    exit(code);


}