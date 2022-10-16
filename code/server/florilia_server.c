// Todo: consider the following points for further development
//
// - Use a Hashmap instead of a Bitmap (i think)
//
// - UDP:
//   - for video/audio streaming
//
// - Encryption
//   - RSA for establishing secure connections
//   - AES for using secure connections (faster)
//
// - unique user identifiers (maybe through RSA public keys?)

#ifndef _POXIS_C_SOURCE
#define _POSIX_C_SOURCE 200809L // enable POSIX.1-2017
#endif

#include "../florilia_net.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <malloc.h>
#include <pthread.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <time.h>

#include "bitmap.h"
#include "bitmap.c"

typedef struct {
    s32 fd;
    u32 name_len;
    char name[32]; // will be a unique identifier later (public key?)
} Client;

typedef struct {
    Bitmap bitmap;
    Client clients[512];
} Client_Manager;


internal void
send_package(s32 socket, u8 *package, u32 size)
{
    printf("sending %d bytes to socket %d\n", size, socket);

    ssize_t sent = send(socket, package, size, 0);
    if (sent == -1)
    {
        // Todo: is there proper error handling to be done here?
        perror("send");
    }
}

// Todo: make a 'broadcast_except' variant when needed
internal void
broadcast_package(Client_Manager *manager, u8 *package, u32 size)
{
    Client *clients = manager->clients;
    u32 max_cnt = ARRAY_COUNT(manager->clients);
    for (u32 it = 0; it < max_cnt; it++)
    {
        if (clients[it].fd)
        {
            send_package(clients[it].fd, package, size);
        }
    }
}

internal void
send_other_connected_user_statuses_to(Client_Manager *manager, Client *to)
{
    u32 desc_size = sizeof(FNS_User_Status);
    u32 package_size = desc_size + FN_MAX_USERNAME_LEN;
    u8 package[package_size];

    FNS_User_Status *desc = (FNS_User_Status*)package;
    desc->type = FNS_USER_STATUS;
    desc->status = FNS_USER_STATUS_ONLINE;

    Client *client_start = manager->clients;
    Client *client_end   = client_start + ARRAY_COUNT(manager->clients) - 1;

    Client *client = client_start;
    while (client < to)
    {
        if (client->fd)
        {
            desc->name_len = client->name_len;

            memcpy(package, desc, desc_size);
            memcpy(package+desc_size, client->name, client->name_len);

            u32 package_size_used = desc_size + client->name_len;
            send_package(to->fd, package, package_size_used);
        }

        client++;
    }

    client++; // skip 'to'
    while (client <= client_end)
    {
        if (client->fd)
        {
            desc->name_len = client->name_len;

            memcpy(package, desc, desc_size);
            memcpy(package+desc_size, client->name, client->name_len);

            u32 package_size_used = desc_size + client->name_len;
            send_package(to->fd, package, package_size_used);
        }

        client++;
    }
}

internal void
broadcast_user_status(Client_Manager *manager, Client *client, u16 status)
{
    FNS_User_Status desc;
    desc.type     = FNS_USER_STATUS;
    desc.status   = status;
    desc.name_len = client->name_len;

    u32 package_size = sizeof(desc) + desc.name_len;
    u8 package[package_size];

    u32 desc_size = sizeof(desc);
    memcpy(package, &desc, desc_size);
    memcpy(package+desc_size, client->name, client->name_len);

    broadcast_package(manager, package, package_size);
}

internal b32
serve_client(Client_Manager *manager, Client *client)
{
    u8 buff[1024];
    ssize_t recvd = recv(client->fd, buff, 1024, 0);
    printf("recvd = %zd\n", recvd);

    if (recvd > 2)
    {
        u16 type = *(u16*)buff;
        if (type == FNC_LOGIN)
        {
            FNS_Login s_login;
            s_login.type   = FNS_LOGIN;
            s_login.status = FNS_LOGIN_ERROR;

            // check for an error
            FNC_Login *c_login = (FNC_Login*)buff;
            s32 desc_size = sizeof(FNC_Login);
            if (recvd < desc_size || recvd < desc_size + c_login->name_len || c_login->name_len > 31)
            {
                printf("error: FNC_Login message invalid\n");
                send_package(client->fd, (u8*)&s_login, sizeof(s_login));
                return false;
            }

            // update client
            client->name_len = c_login->name_len;
            memcpy(client->name, c_login+1, c_login->name_len);
            client->name[client->name_len] = '\0';

            // send login success
            s_login.status = FNS_LOGIN_SUCCESS;
            send_package(client->fd, (u8*)&s_login, sizeof(s_login));

            // broadcast new client status to all clients
            broadcast_user_status(manager, client, FNS_USER_STATUS_ONLINE);

            // send all other clients status's to new client
            send_other_connected_user_statuses_to(manager, client);

            printf("login successful\n");
            return true;
        }
        else if (type == FNC_CHAT_MESSAGE)
        {
            FNC_Chat_Message *c_desc =(FNC_Chat_Message*)buff;
            u32 c_desc_size = sizeof(FNC_Chat_Message);

            FNS_Chat_Message s_desc;
            s_desc.type        = FNS_CHAT_MESSAGE;
            s_desc.sender_len  = client->name_len;
            s_desc.message_len = c_desc->message_len;

            u32 package_size = sizeof(s_desc) + s_desc.sender_len + s_desc.message_len;
            u8 package[package_size];

            u32 offset_sender = sizeof(s_desc);
            u32 offset_message = offset_sender + client->name_len;

            memcpy(package, &s_desc, sizeof(s_desc));
            memcpy(package+offset_sender, client->name, client->name_len);
            memcpy(package+offset_message, buff+c_desc_size, c_desc->message_len);

            broadcast_package(manager, package, package_size);

            return true;
        }
        else
        {
            printf("type invalid\n");
        }
    }
    else if (recvd > 0)
    {
        printf("message invalid\n");
    }
    else if (recvd == 0)
    {
        printf("recvd = 0\n");
    }
    else if (recvd == -1)
    {
        printf("recvd error: %s\n", strerror(errno));
    }

    return false;
}

internal void
remove_client(Client_Manager *manager, Client *client, s32 epollfd)
{
    broadcast_user_status(manager, client, FNS_USER_STATUS_OFFLINE);

    s32 deleted = epoll_ctl(epollfd, EPOLL_CTL_DEL, client->fd, 0); 
    if (deleted == -1)
    {
        perror("epoll_ctl (delete)");
    }

    s32 was_index = client - manager->clients;
    free_bitmap_num(&manager->bitmap, was_index);

    s32 was_fd = client->fd;
    client->fd = 0;
    client->name[0] = '\0';
    client->name_len = 0;

    printf("removed client (index = %d, fd = %d)\n", was_index, was_fd);
}

internal void*
add_client(Client_Manager *manager, s32 fd)
{
    s32 index = alloc_bitmap_num(&manager->bitmap);
    if (index == -1)
    {
        printf("alloc_bitmap_num failed\n");
        return 0;
    }

    Client *client = manager->clients + index;
    client->fd = fd;
    client->name[0]  = '\0';
    client->name_len = 0;

    printf("added client (index = %d, fd = %d)\n", index, fd);

    return client;
}

internal s32
open_listening_socket(u16 port, int backlog)
{
    int listening_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listening_socket == -1)
    {
        printf("Could not create socket");
        return -1;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    if (bind(listening_socket, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("error: bind(...) failed\n");
        close(listening_socket);
        return -1;
    }

    listen(listening_socket, backlog);

    return listening_socket;
}

static s32 init_epolling(s32 listening_socket)
{
    s32 epollfd = epoll_create1(0);

    if (epollfd >= 0)
    {
        struct epoll_event ev;
        ev.events  = EPOLLIN;
        ev.data.fd = listening_socket;

        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listening_socket, &ev) == -1)
        {
            perror("epoll_ctl: add listening_socket");
            close(epollfd);
            return -1;
        }
    }
    else
    {
        perror("epoll_create1");
        return -1;
    }

    return epollfd;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("please set argv[1] to a port number\n");
        return 0;
    }
    u16 port = atoi(argv[1]);
    s32 backlog = 32;


    Client_Manager client_manager;
    memset(&client_manager, 0, sizeof(client_manager));

    u32 max_cnt_clients = sizeof(client_manager.clients);
    bool bitmap = alloc_and_init_bitmap(&client_manager.bitmap, max_cnt_clients);
    if (!bitmap) return EXIT_FAILURE;

    s32 listening_socket = open_listening_socket(port, backlog);
    if (listening_socket == -1) return EXIT_FAILURE;

    s32 epollfd = init_epolling(listening_socket);
    if (epollfd == -1) return EXIT_FAILURE;


    int max_epoll_events = backlog;
    struct epoll_event epoll_events[max_epoll_events];
    for (;;)
    {
        int cnt_events = epoll_wait(epollfd, epoll_events, max_epoll_events, -1);
        if (cnt_events == -1)
        {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < cnt_events; i++)
        {
            struct epoll_event *event = epoll_events + i;

            if (event->data.fd == listening_socket)
            {
                if (!(event->events & EPOLLIN))
                {
                    printf("listening socket: invalid event\n");
                    exit(EXIT_FAILURE);
                }

                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);
                int conn_socket = accept(listening_socket, (struct sockaddr*)&client_addr, &client_addr_len);
                if (conn_socket == -1)
                {
                    perror("accept");
                    continue;
                }

                Client *client = add_client(&client_manager, conn_socket);
                if (!client)
                {
                    printf("add_client: failed\n");
                    close(conn_socket);
                    continue;
                };

                struct epoll_event ev;
                ev.events   = EPOLLIN | EPOLLET;
                ev.data.fd  = listening_socket;
                ev.data.ptr = client;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_socket, &ev) == -1)
                {
                    perror("epoll_ctl: del conn_socket");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                Client *client = event->data.ptr;

                if (event->events & EPOLLERR)
                {
                    remove_client(&client_manager, client, epollfd);
                    continue;
                }

                if (event->events & EPOLLIN)
                {
                    bool served = serve_client(&client_manager, client);
                    if (!served)
                    {
                        remove_client(&client_manager, client, epollfd);
                        continue;
                    }
                }

                if (event->events & EPOLLHUP)
                {
                    remove_client(&client_manager, client, epollfd);
                    continue;
                }
            }
        }
    }

    return 0;
}
