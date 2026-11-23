#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sys/epoll.h> // 引入 epoll 库
#include <fcntl.h>     // 引入 fcntl，用于设置非阻塞
#include <vector>

// 把 socket 设置为非阻塞（ET 模式的硬性要求）
void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    // 1. 创建监听 socket (和以前一样)
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8888);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, 128);
    setNonBlocking(server_fd); // 监听 socket 也设非阻塞

    // 2. 创建 epoll 实例（管家上班）
    // 参数 128 表示 epoll 能同时监听的最大事件数（只是个建议值）
    int epfd = epoll_create1(0); 
    
    // 3. 把 server_fd 注册到 epoll 中，关注它的“连接到来（EPOLLIN）”事件
    struct epoll_event ev, events[1024];
    ev.events = EPOLLIN; // 默认是水平触发 (LT)
    ev.data.fd = server_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

    std::cout << "Epoll 服务端已启动，监听 8888 端口...\n";

    // 4. 死循环，无限等待事件到来
    while (true) {
        // 阻塞等待，直到有事件发生。返回发生事件的个数 nfds
        int nfds = epoll_wait(epfd, events, 1024, -1);
        
        // 遍历所有发生的事件
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            // 如果是 server_fd 来事件了，说明有新的客户端连接
            if (fd == server_fd) {
                sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
                
                if (client_fd > 0) {
                    std::cout << "新客户端接入，fd = " << client_fd << "\n";
                    setNonBlocking(client_fd); // 客户端 socket 也设非阻塞
                    // 把新客户端注册到 epoll 中
                    ev.events = EPOLLIN; 
                    ev.data.fd = client_fd;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);
                }
            } 
            // 否则，就是某个客户端发数据来了
            else {
                char buf[1024] = {0};
                // 接收数据
                int bytes = recv(fd, buf, sizeof(buf) - 1, 0);
                
                if (bytes > 0) {
                    std::cout << "收到 fd " << fd << " 的消息: " << buf << "\n";
                    // 如果是 echo 服务器，这里应该原样发回去：send(fd, buf, bytes, 0);
                } else if (bytes == 0) {
                    // 客户端断开连接
                    std::cout << "客户端 fd " << fd << " 断开\n";
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                } else {
                    std::cerr << "接收错误\n";
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                }
            }
        }
    }
    close(server_fd);
    close(epfd);
    return 0;
}