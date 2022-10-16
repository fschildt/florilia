DEBUG_PLATFORM_CONNECT(win32_connect)
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        int error = WSAGetLastError();
        printf("socket() failed, error = %d\n", error);
        return -1;
    }
    printf("socket() success\n");

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(address);

    int connected = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (connected != 0)
    {
        int error = WSAGetLastError();
        printf("connect() failed, error = %d\n", error);
        closesocket(sock);
        return -1;
    }
    printf("connect(...) success\n");

    u_long mode = 1;
    int result = ioctlsocket(sock, FIONBIO, &mode);
    if (result != NO_ERROR)
    {
        int error = WSAGetLastError();
        printf("ioctlsocket failed, error = %d\n", error);
        closesocket(sock);
        return -1;
    }
    printf("fcntl() success\n");

    return sock;
}

DEBUG_PLATFORM_DISCONNECT(win32_disconnect)
{
    closesocket(netid);
}

internal
DEBUG_PLATFORM_SEND(win32_send)
{
    printf("sending...\n");
    int sent = send(netid, (const char*)buffer, size, 0);
    printf("sent %d/%u bytes\n", sent, size);
    return sent;
}

internal
DEBUG_PLATFORM_RECV(win32_recv)
{
    printf("receving...\n");
    int recvd = recv(netid, (char*)buffer, size, 0);
    printf("recvd %d bytes\n", recvd);
    if (recvd == SOCKET_ERROR)
    {
        return -1;
    }
    return recvd;
}

internal b32
win32_init_networking()
{
    static WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        wprintf(L"WSAStartup failed: %d\n", iResult);
        return false;
    }
    printf("WSAStartup done\n");
    return true;
}
