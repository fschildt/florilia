DEBUG_PLATFORM_CONNECT(DEBUG_lin_connect)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
    {
        printf("socket() failed\n");
        return -1;
	}
	

    // connect
	struct sockaddr_in sv_sockaddr;
	sv_sockaddr.sin_addr.s_addr = inet_addr(address);
	sv_sockaddr.sin_family = AF_INET;
	sv_sockaddr.sin_port = htons(port);

    int connected = connect(fd, (struct sockaddr *)&sv_sockaddr, sizeof(sv_sockaddr));
    if (connected == -1)
    {
		printf("connect() failed\n");
        close(fd);
        return -1;
    }


    // make socket non-blocking
    int set_nonblocking = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    if (set_nonblocking == -1)
    {
        printf("error: fcntl failed to set socket non-blocking\n");
        close(fd);
        return -1;
    }

    return fd;
}

DEBUG_PLATFORM_DISCONNECT(DEBUG_lin_disconnect)
{
    close(netid);
}

DEBUG_PLATFORM_SEND(DEBUG_lin_send)
{
    ssize_t sent = send(netid, buffer, size, MSG_NOSIGNAL);
    if (sent == -1)
    {
        return -1;
    }
    return sent;
}

DEBUG_PLATFORM_RECV(DEBUG_lin_recv)
{
    ssize_t recvd = recv(netid, buffer, size, 0);
    if (recvd == -1 && errno == EAGAIN)
    {
        return 0;
    }
    return recvd;
}
