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
#define log printf


void die(bool, const char *);

/** *** *** *** *** *** DATA SECTION *** *** *** *** *** **/


bool should_quit;
char *server_port_str;
char *server_host_str;
char buffer[2 + MAX_MESSAGE_LENGTH + 1]; //+1 for trailing zero
struct pollfd descriptors[2]; //0 - server socket, 1 - stdin

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
        die(false, "invalid number of arguments");

    log("Arguments parsed\n");
}

void init_globals()
{
    should_quit = false;
    descriptors[0].events = POLLIN | POLLHUP | POLLERR;
    descriptors[1].events = POLLIN; //dont bother with stdin errors
    descriptors[0].fd = -1;
    descriptors[1].fd = STDIN_FILENO;

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

    log("Connecting to %s\n", server_host_str);

    negative_is_bad(connect(descriptors[0].fd, addr_result->ai_addr, addr_result->ai_addrlen),
                    "cannot connect to the server");
    freeaddrinfo(addr_result);

    log("Client connected with server\n");
}

//todo write send message(buf, dsc)

void handle_stdin()
{
    short text_len;
    short net_text_len;

    fgets(buffer + 2, MAX_MESSAGE_LENGTH, stdin);
    text_len = (short) (strnlen(buffer + 2, MAX_MESSAGE_LENGTH));
    net_text_len = htons((uint16_t) text_len);
    memcpy(buffer, &net_text_len, 2);

    log("Read %d-len message\n", text_len);

    safe_all_write(descriptors[0].fd, buffer, (size_t) text_len + 2);
    log("Message sent to server\n");
}

void handle_server_message()
{
    short length;
    safe_all_read(descriptors[0].fd, (char*) &length, 2); //len
    //server also can send one byte and hang client, but it is acceptable to wait forever for second byte
    length = ntohs((uint16_t) length);

    log("Received message length %d\n", length);
    if(!(0 < length && length <= MAX_MESSAGE_LENGTH))
        die(false, "message size");

    safe_all_read(descriptors[0].fd, buffer, (size_t) length);
    log("Received message text: ");
    printf("%.*s", length, buffer);

}

void main_loop()
{
    int poll_value;
    while(!should_quit)
    {
        //clear revents
        descriptors[0].revents = 0;
        descriptors[1].revents = 0;


        log("Awaiting for input/data\n");
        negative_is_bad(poll_value = poll(descriptors, 2, -1), "pool failure");
        if(poll_value == 0) continue;
        assert(poll_value > 0); //check if pool is not for ever

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
    die(true, NULL);

}

void die(bool success, const char *reason)
{
    static bool been_here = false;
    if(been_here) fatal("OS failure");
    been_here = true;
    printf("Client is turning off: %s\n", success ? "successfully" : reason);

    if(server_port_str != NULL)
        free(server_port_str);
    if(server_host_str != NULL)
        free(server_host_str);

    if(descriptors[0].fd != -1)
        negative_is_bad(close(descriptors[0].fd), "could not close socket");


    exit(success ? EXIT_SUCCESS : EXIT_FAILURE);


}