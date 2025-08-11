// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2022 Julio Cesar Ziviani Alvarez

#pragma once

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
#elif __linux__
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <functional>
#include <mutex>
#include <queue>
#include <string>

#define SOCKADDR_IN sockaddr_in
#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif
#ifndef SOCKET
#define SOCKET int
#endif
#else
#error Operating System not supported
#endif

#include <queue>
#include <unordered_map>
#include <functional>

namespace makeland {
    class SocketTCP {
    public:
        typedef function<void(uint64_t socketId, const SOCKADDR_IN& address, void** customData)> AcceptCallback;
        typedef function<void(uint64_t socketId, size_t totalAvailable, void* customData)> ReadCallback;
        typedef function<void(Result result, uint64_t socketId)> ConnectCallback;
        typedef function<void(uint64_t socketId, void* customData)> CloseCallback;
        typedef function<void(Result result, uint64_t socketId, void* customData)> SocketWriteFinishCallback;

        SocketTCP() = default;
        SocketTCP(const SocketTCP&) = delete;
        SocketTCP& operator=(const SocketTCP&) = delete;
        SocketTCP(const SocketTCP&&) = delete;
        SocketTCP& operator=(const SocketTCP&&) = delete;
        ~SocketTCP() = default;

        Result initialize() {
#ifdef _WIN32
            int result;
            WSADATA wsaData;

            if ((result = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
                return Result("socket_error", "WSAStartup error {}", result);
            }
            mainSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            if (mainSocket == INVALID_SOCKET) {
                return Result("socket_error", "`socket()` method error: {}", getOSLastError());
            }
            u_long nonBlocking = 1;

            if (ioctlsocket(mainSocket, FIONBIO, &nonBlocking) == SOCKET_ERROR) {
                return Result("socket_error", "`ioctlsocket()` method error: {}", getOSLastError());
            }
#elif __linux__
            epollFd = epoll_create1(0);

            if (epollFd == -1) {
                return Result("socket_error", "`epoll_create1()` method error: {}", getOSLastError());
            }
            eventFd = eventfd(0, EFD_NONBLOCK);

            Result result = setEvent(eventFd, EPOLL_CTL_ADD, EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET);

            if (result) {
                return result;
            }
#endif
            return Result::ok();
        }

        void terminate() {
            signal();

            while (peerSockets.size() > 0) {
                close(peerSockets.begin()->second->socket);
            }
        }

        void onAccept(AcceptCallback callback) {
            acceptCallback = callback;
        }

        void onRead(ReadCallback callback) {
            readCallback = callback;
        }

        void onConnect(ConnectCallback callback) {
            connectCallback = callback;
        }

        void onClose(CloseCallback callback) {
            closeCallback = callback;
        }

        void close(uint64_t socketId) {
            closedSocket = true;
            auto it = peerSockets.find((SOCKET)socketId);

            if (it != peerSockets.end()) {
                PeerSocketInfo* peer = it->second;

                it = peerSockets.find((SOCKET)socketId);

                while (peer->writeBuffers.size() > 0) {
                    WriteBufferInfo& writeBuffer = peer->writeBuffers.front();

                    if (writeBuffer.writeFinishCallback) {
                        writeBuffer.writeFinishCallback(Result("socket_error", "Could not send data, socket is closed"), peer->socket, writeBuffer.customData);
                    }
                    peer->writeBuffers.pop();
                }
                peerSockets.erase(it);

                if (closeCallback) {
                    closeCallback(socketId, peer->customData);
                }
                delete peer;
            }
#ifdef _WIN32
            closesocket(socketId);
#elif __linux__
            setEvent((int)socketId, EPOLL_CTL_DEL, 0);
            ::close((int)socketId);
#endif
        }

        Result connect(const string& server, int port, uint64_t* socketId) {
#ifdef _WIN32
            if (!mainSocket) {
                return Result("socket_error", "Not initialized");
            }
            SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            if (clientSocket == INVALID_SOCKET) {
                return Result("socket_error", "`socket()` method error: {}", getOSLastError());
            }
            u_long nonBlocking = 1;

            if (ioctlsocket(clientSocket, FIONBIO, &nonBlocking) == SOCKET_ERROR) {
                return Result("socket_error", "`ioctlsocket()` method error: {}", getOSLastError());
            }
            sockaddr_in connectionAddress;
            memset(&connectionAddress, 0, sizeof(sockaddr_in));
            connectionAddress.sin_family = AF_INET;
            connectionAddress.sin_addr.s_addr = inet_addr(server.data());
            connectionAddress.sin_port = htons((u_short)port);

            if (::connect(clientSocket, (struct sockaddr*)&connectionAddress, sizeof(connectionAddress)) == SOCKET_ERROR) {
                if (WSAGetLastError() != WSAEWOULDBLOCK) {
                    return Result("socket_error", "`connect()` method error: {}", getOSLastError());
                }
            }
            PeerSocketInfo* peer = new PeerSocketInfo();
            peer->socket = clientSocket;
            peer->status = Status::Connecting;
            peerSockets[peer->socket] = peer;

            if (socketId) {
                *socketId = clientSocket;
            }
#elif __linux__
#endif
            return Result::ok();
        }

        Result listen(int port) {
#ifdef _WIN32
            sockaddr_in connectionAddress;
            memset(&connectionAddress, 0, sizeof(sockaddr_in));
            connectionAddress.sin_family = AF_INET;
            connectionAddress.sin_addr.s_addr = htonl(INADDR_ANY);
            connectionAddress.sin_port = htons((u_short)port);

            if (::bind(mainSocket, (PSOCKADDR)&connectionAddress, sizeof(sockaddr_in)) == SOCKET_ERROR) {
                return Result("socket_error", "`bind()` method error: {}", getOSLastError());
            }
            if (::listen(mainSocket, 50) == SOCKET_ERROR) {
                return Result("socket_error", "`listen()` method error: {}", getOSLastError());
            }
#elif __linux__
            if (listenSocket != -1) {
                return Result("socket_error", "main_socket already in use");
            }
            listenSocket = ::socket(AF_INET, SOCK_STREAM, 0);

            if (listenSocket == -1) {
                return Result("socket_error", "`socket()` method error: {}", getOSLastError());
            }
            int flags = fcntl(listenSocket, F_GETFL);

            if (flags == -1) {
                return Result("socket_error", "`fcntl()` method error: {}", getOSLastError());
            }
            if (fcntl(listenSocket, F_SETFL, flags | O_NONBLOCK) == -1) {
                return Result("socket_error", "`fcntl()` method error: {}", getOSLastError());
            }
            int opt = 1;

            if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
                return Result("socket_error", "`setsockopt()` method error: {}", getOSLastError());
            }
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
            addr.sin_port = htons(port);

            if (::bind(listenSocket, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
                return Result("socket_error", "`bind()` method error: {}", getOSLastError());
            }
            if (::listen(listenSocket, 1000) == -1) {
                return Result("socket_error", "`listen()` method error: {}", getOSLastError());
            }
            Result result = setEvent(listenSocket, EPOLL_CTL_ADD, EPOLLIN);

            if (result) {
                return result;
            }
#endif
            return Result::ok();
        }

        Result receive(uint64_t socketId, char* buf, size_t len, bool peek, size_t* totalRead) {
            int count = recv((SOCKET)socketId, buf, (int)len, peek ? MSG_PEEK : 0);

            if (count == -1) {
#ifdef _WIN32
                if (WSAGetLastError() == WSAEWOULDBLOCK) {
#elif __linux__
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
#endif
                    if (totalRead) {
                        *totalRead = 0;
                    }
                    return Result::ok();
                }
                return Result("socket_error", "`recv()` method error: {}", getOSLastError());
            }
            if (totalRead) {
                *totalRead = (size_t)count;
            }
            return Result::ok();
        }

        Result flush(uint64_t socketId) {
            auto it = peerSockets.find((SOCKET)socketId);

            if (it == peerSockets.end()) {
                return Result("socket_error", "invalid socket");
            }
            while (it != peerSockets.end() && it->second->writeBuffers.size() > 0) {
                select(0, nullptr);
                it = peerSockets.find((SOCKET)socketId);
            }
            return Result::ok();
        }

        Result select(int timeout, size_t * totalSockets) {
#ifdef _WIN32
            FD_SET readSet;
            FD_SET writeSet;
            FD_SET errorSet;

            FD_ZERO(&readSet);
            FD_ZERO(&writeSet);
            FD_ZERO(&errorSet);
            FD_SET(mainSocket, &readSet);

            for (auto& i : peerSockets) {
                PeerSocketInfo* peer = i.second;

                if (peer->status == Status::Connecting) {
                    FD_SET(peer->socket, &writeSet);
                    FD_SET(peer->socket, &errorSet);
                }
                else {
                    if (peer->writeBuffers.size() > 0) {
                        FD_SET(peer->socket, &writeSet);
                    }
                    else {
                        FD_SET(peer->socket, &readSet);
                    }
                }
            }
            timeval timeoutVal = { 0, timeout * 1000 };
            int total = ::select(0, &readSet, &writeSet, &errorSet, &timeoutVal);
            int ret = total;

            if (total == SOCKET_ERROR) {
                return Result("socket_error", "`select()` method error: {}", getOSLastError());
            }
            if (total == 0) {
                if (totalSockets) {
                    *totalSockets = 0;
                }
                return Result::ok();
            }
            if (FD_ISSET(mainSocket, &readSet)) {
                total--;
                PeerSocketInfo* peer = new PeerSocketInfo();
                peer->status = Status::Connected;

                SOCKADDR_IN addressIn;
                int addressInLength = sizeof(SOCKADDR_IN);

                if ((peer->socket = accept(mainSocket, (SOCKADDR*)&addressIn, &addressInLength)) != INVALID_SOCKET) {
                    u_long nonBlocking = 1;

                    if (ioctlsocket(peer->socket, FIONBIO, &nonBlocking) == SOCKET_ERROR) {
                        return Result("socket_error", "`accept()` method error: {}", getOSLastError());
                    }
                    peerSockets[peer->socket] = peer;

                    if (acceptCallback) {
                        acceptCallback(peer->socket, addressIn, &peer->customData);
                    }
                }
                else {
                    if (WSAGetLastError() != WSAEWOULDBLOCK) {
                        return Result("socket_error", "`accept()` method error: {}", getOSLastError());
                    }
                }
            }
            if (total > 0) {
                for (auto& i : peerSockets) {
                    PeerSocketInfo* peer = i.second;

                    if (FD_ISSET(peer->socket, &writeSet)) {
                        if (peer->status == Status::Connecting) {
                            if (connectCallback) {
                                connectCallback(Result::ok(), peer->socket);
                            }
                            peer->status = Status::Connected;
                        }
                        else if (peer->writeBuffers.size() > 0) {
                            size_t n = peer->writeBuffers.size();

                            while (n-- > 0) {
                                WriteBufferInfo& writeBuffer = peer->writeBuffers.front();

                                if (writeBuffer.getRemainingBufferSize() > 0) {
                                    int count = send(peer->socket, writeBuffer.getRemainingBuffer(), (int)writeBuffer.getRemainingBufferSize(), 0);

                                    if (count == SOCKET_ERROR) {
                                        close(peer->socket);

                                        if (totalSockets) {
                                            *totalSockets = 1;
                                        }
                                        return Result::ok();
                                    }
                                    writeBuffer.writeDone((size_t)count);
                                }
                                if (writeBuffer.getRemainingBufferSize() == 0) {
                                    if (writeBuffer.writeFinishCallback) {
                                        writeBuffer.writeFinishCallback(Result::ok(), peer->socket, writeBuffer.customData);
                                    }
                                    peer->writeBuffers.pop();
                                }
                            }
                        }
                    }
                    else if (FD_ISSET(peer->socket, &readSet)) {
                        u_long count = 0;
                        int result = ioctlsocket(peer->socket, FIONREAD, &count);

                        if (result == 0 && count > 0) {
                            if (readCallback) {
                                closedSocket = false;
                                readCallback(peer->socket, (size_t)count, peer->customData);

                                if (closedSocket) {
                                    if (totalSockets) {
                                        *totalSockets = 1;
                                    }
                                    return Result::ok();
                                }
                            }
                        }
                        else if (result != 0) {
                            return Result("socket_error", "`ioctlsocket()` method error: {}", getOSLastError());
                        }
                        else if (result == 0 && count == 0) {
                            int r = recv(peer->socket, NULL, 0, 0);

                            if (r <= 0) {
                                close(peer->socket);

                                if (totalSockets) {
                                    *totalSockets = 1;
                                }
                                return Result::ok();
                            }
                        }
                    }
                    else if (FD_ISSET(peer->socket, &errorSet)) {
                        if (peer->status == Status::Connecting) {
                            DWORD errCode = 0;
                            int len = sizeof(errCode);

                            if (getsockopt(peer->socket, SOL_SOCKET, SO_ERROR, (char*)&errCode, &len) == 0) {
                                if (connectCallback) {
                                    connectCallback(Result("socket_error", "`getsockopt()` method error: {}", getOSLastError()), peer->socket);
                                }
                            }
                            peer->status = Status::Error;
                        }
                    }
                }
            }
            if (totalSockets) {
                *totalSockets = (size_t)ret;
            }
#elif __linux__
            int total = epoll_wait(epollFd, events, MAX_SOCKETS, timeout);

            if (total == -1) {
                return Result("socket_error", "`epoll_wait()` method error: {}", getOSLastError());
            }
            if (total == 0) {
                if (totalSockets) {
                    *totalSockets = 0;
                }
                return Result::ok();
            }
            for (int i = 0; i < total; i++) {
                if ((events[i].events & EPOLLERR) ||
                    (events[i].events & EPOLLHUP) ||
                    (events[i].events & EPOLLRDHUP)) {
                    auto it = peerSockets.find(events[i].data.fd);

                    if (it == peerSockets.end()) {
                        close(events[i].data.fd);
                        return Result("socket_error", "socket {} not found", events[i].data.fd);
                    }
                    PeerSocketInfo* peer = it->second;

                    if (peer->status == Status::Connecting) {
                        char buffer[2];
                        int result = recv(peer->socket, buffer, 1, MSG_PEEK | MSG_DONTWAIT);

                        if (result == -1) {
                            if (connectCallback) {
                                connectCallback(Result("socket_error", "`recv()` method error: {}", getOSLastError()), peer->socket);
                            }
                        }
                    }
                    close(peer->socket);

                    if (totalSockets) {
                        *totalSockets = 1;
                    }
                    return Result::ok();
                }
                else if (events[i].data.fd == listenSocket) {
                    PeerSocketInfo* accepted = new PeerSocketInfo();
                    accepted->status = Status::Connected;

                    struct sockaddr_in addr;
                    socklen_t addr_len = sizeof(addr);

                    accepted->socket = accept(listenSocket, (struct sockaddr*)&addr, &addr_len);

                    if (accepted->socket == -1) {
                        if (errno != EAGAIN && errno != EWOULDBLOCK) {
                            return Result("socket_error", "`accept()` method error: {}", getOSLastError());
                        }
                        continue;
                    }
                    int flags = fcntl(accepted->socket, F_GETFL);

                    if (flags == -1) {
                        return Result("socket_error", "`fcntl()` method error: {}", getOSLastError());
                    }
                    int opt = 1;

                    if (setsockopt(accepted->socket, SOL_TCP, TCP_NODELAY, &opt, sizeof(opt)) == -1) {
                        return Result("socket_error", "`setsockopt()` method error: {}", getOSLastError());
                    }
                    if (setsockopt(accepted->socket, IPPROTO_TCP, TCP_QUICKACK, &opt, sizeof(opt)) == -1) {
                        return Result("socket_error", "`setsockopt()` method error: {}", getOSLastError());
                    }
                    if (fcntl(accepted->socket, F_SETFL, flags | O_NONBLOCK) == -1) {
                        return Result("socket_error", "`fcntl()` method error: {}", getOSLastError());
                    }
                    peerSockets[accepted->socket] = accepted;

                    Result result = setEvent(accepted->socket, EPOLL_CTL_ADD, EPOLLIN);

                    if (result) {
                        return result;
                    }
                    if (acceptCallback) {
                        acceptCallback(accepted->socket, addr, &accepted->customData);
                    }
                }
                else if (events[i].events & EPOLLOUT) {
                    auto it = peerSockets.find(events[i].data.fd);

                    if (it == peerSockets.end()) {
                        close(events[i].data.fd);
                        return Result("socket_error", "socket {} not found", events[i].data.fd);
                    }
                    PeerSocketInfo* peer = it->second;

                    if (peer->writeBuffers.size() > 0) {
                        size_t n = peer->writeBuffers.size();

                        while (n-- > 0) {
                            WriteBufferInfo& writeBuffer = peer->writeBuffers.front();

                            if (writeBuffer.getRemainingBufferSize() > 0) {
                                int count = ::send(peer->socket, writeBuffer.getRemainingBuffer(), (int)writeBuffer.getRemainingBufferSize(), MSG_NOSIGNAL);

                                if (count == -1) {
                                    close(peer->socket);

                                    if (totalSockets) {
                                        *totalSockets = 1;
                                    }
                                    return Result::ok();
                                }
                                writeBuffer.writeDone(count);
                            }
                            if (writeBuffer.getRemainingBufferSize() == 0) {
                                if (writeBuffer.writeFinishCallback) {
                                    writeBuffer.writeFinishCallback(Result::ok(), peer->socket, writeBuffer.customData);
                                }
                                peer->writeBuffers.pop();
                            }
                        }
                    }
                    if (peer->writeBuffers.size() == 0) {
                        Result result = setEvent(peer->socket, EPOLL_CTL_MOD, EPOLLIN);

                        if (result) {
                            return result;
                        }
                    }
                }
                else if (events[i].events & EPOLLIN) {
                    if (events[i].data.fd == eventFd) {
                        uint64_t v;
                        int count = recv(events[i].data.fd, &v, sizeof(v), 0);

                        if (count == -1) {
                            return Result("socket_error", "`read()` method error: {}", getOSLastError());
                        }
                    }
                    else {
                        auto it = peerSockets.find(events[i].data.fd);

                        if (it == peerSockets.end()) {
                            close(events[i].data.fd);
                            return Result("socket_error", "socket {} not found", events[i].data.fd);
                        }
                        PeerSocketInfo* peer = it->second;

                        if (peer->status == Status::Connecting) {
                            char buffer[2];
                            int result = recv(peer->socket, buffer, 1, MSG_PEEK | MSG_DONTWAIT);

                            if (result == -1) {
                                if (connectCallback) {
                                    connectCallback(Result("socket_error", "`recv()` method error: {}", getOSLastError()), peer->socket);
                                }
                                close(peer->socket);

                                if (totalSockets) {
                                    *totalSockets = 1;
                                }
                                return Result::ok();
                            }
                            else {
                                if (connectCallback) {
                                    connectCallback(Result::ok(), peer->socket);
                                }
                                peer->status = Status::Connected;
                            }
                        }
                        else {
                            int count;
                            int result = ioctl(events[i].data.fd, FIONREAD, &count);

                            if (result == -1) {
                                return Result("socket_error", "`ioctl()` method error: {}", getOSLastError());
                            }
                            if (count == 0) {
                                close(peer->socket);

                                if (totalSockets) {
                                    *totalSockets = 1;
                                }
                                return Result::ok();
                            }
                            else if (count > 0 && readCallback) {
                                closedSocket = false;
                                readCallback(peer->socket, count, peer->customData);

                                if (closedSocket) {
                                    return Result::ok();
                                }
                            }
                        }
                    }
                }
            }
            if (totalSockets) {
                *totalSockets = (size_t)total;
            }
#endif
            return Result::ok();
        }

        Result write(uint64_t socketId, const char* buffer, size_t len, void* customData, SocketWriteFinishCallback writeFinishCallback) {
            auto it = peerSockets.find((SOCKET)socketId);

            if (it == peerSockets.end()) {
                if (writeFinishCallback) {
                    writeFinishCallback(Result("invalid_socket_id", "invalid socket id {}", socketId), socketId, customData);
                }
                return Result("socket_error", "invalid socketId");
            }
            it->second->writeBuffers.push(WriteBufferInfo(buffer, len, customData, writeFinishCallback));

#ifdef __linux__
            Result result = setEvent(socketId, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);

            if (result) {
                return result;
            }
#endif
            return Result::ok();
        }

        void signal() {
#ifdef __linux__
            lock_guard<mutex> guard(epollFdMutex);

            ssize_t ret = 0;
            uint64_t val = 0;

            do {
                ret = ::write(epollFd, &val, sizeof(val));
            } while (ret < 0 && errno == EAGAIN);
#endif
        }

    private:
        enum class Status {
            Initializing,
            Connecting,
            Connected,
            Error
        };

        class WriteBufferInfo {
        public:
            const char* buffer;
            size_t len;
            size_t pos = 0;
            SocketWriteFinishCallback writeFinishCallback;
            void* customData;

            WriteBufferInfo(const char* _buffer, size_t _len, void* _customData, SocketWriteFinishCallback _writeFinishCallback) :
                buffer(_buffer), len(_len), writeFinishCallback(_writeFinishCallback), customData(_customData) {
            }

            const char* getRemainingBuffer() {
                if (pos >= len) {
                    return nullptr;
                }
                return buffer + pos;
            }

            size_t getRemainingBufferSize() const {
                if (pos >= len) {
                    return 0;
                }
                return len - pos;
            }

            void writeDone(size_t amount) {
                pos += amount;
            }
        };

        struct PeerSocketInfo {
            Status status = Status::Initializing;
            SOCKET socket = 0;
            void* customData = nullptr;
            queue<WriteBufferInfo> writeBuffers;
        };
#ifdef _WIN32
        SOCKET mainSocket;
#elif __linux__
        static const int MAX_SOCKETS = 100 * 1000;

        int epollFd = -1;
        int listenSocket = -1;
        int eventFd = -1;
        epoll_event events[MAX_SOCKETS];
        mutex epollFdMutex;
#endif
        unordered_map<SOCKET, PeerSocketInfo*> peerSockets;
        AcceptCallback acceptCallback = nullptr;
        ConnectCallback connectCallback = nullptr;
        ReadCallback readCallback = nullptr;
        CloseCallback closeCallback = nullptr;
        bool closedSocket = false;

        string getOSLastError() {
#ifdef _WIN32
            static char message[1024];
            LPSTR messageBuffer = nullptr;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, (DWORD)WSAGetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR)&messageBuffer, 0, NULL);
            strcpy(message, messageBuffer);
            LocalFree(messageBuffer);

            string out = message;

            return su::format("wsa_error_code_{} ({})", WSAGetLastError(), out);
#elif __linux__
            return su::format("errno={} ({})", errno, strerror(errno));
#endif
        }

#ifdef _WIN32
        string getErrorMessage(DWORD code) {
            switch (code) {
                case 10049:
                    return "The requested address is not valid in its context";
                case 10060:
                    return "Connection timeout";
                case 10061:
                    return "Connection refused";
            }
            return su::format("winsock error {}", code);
        }
#elif __linux__
        Result setEvent(int socket, int type, int flags) {
            struct epoll_event ev;
            memset(&ev, 0, sizeof(ev));
            ev.data.fd = socket;
            ev.events = flags;

            if (::epoll_ctl(epollFd, type, socket, &ev) == -1) {
                return Result("socket_error", "`epoll_ctl()` method error: {}", getOSLastError());
            }
            return Result::ok();
        }
#endif
    };
}