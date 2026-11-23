#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sys/epoll.h>
#include <fcntl.h>
#include <vector>
#include <map>
#include <fstream>
#include <algorithm>
#include <arpa/inet.h>

// 1. 把 socket 设为非阻塞（ET模式必须）
void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 2. 核心：为每个客户端建立一个独立的上下文，记录它的接收状态
struct ClientContext {
    int fd;
    uint32_t expected_size = 0; // 包头声明的文件大小
    uint32_t received_size = 0; // 已经接收的字节数
    bool header_received = false; // 是否已经读完 4 字节包头
    std::vector<char> header_buf; // 用来拼接包头的缓冲
    std::ofstream outfile;        // 该客户端专属的写文件流
    int file_id;                  // 第几个文件
};

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8888);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, 128);
    setNonBlocking(server_fd); 

    int epfd = epoll_create1(0); 
    struct epoll_event ev, events[1024];
    
    // 监听 server_fd，并采用边缘触发 (ET) 模式
    ev.events = EPOLLIN | EPOLLET; 
    ev.data.fd = server_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

    std::cout << "Epoll 服务端已启动 (ET 模式 + 状态机)，监听 8888 端口...\n";

    std::map<int, ClientContext> clients; // 管理所有客户端的状态
    int global_file_id = 0;

    while (true) {
        int nfds = epoll_wait(epfd, events, 1024, -1);
        
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            // 有新的客户端连接
                        // 有新的客户端连接
            if (fd == server_fd) {
                //  核心修复：因为是 ET 模式，必须一次性把所有排队的连接都 accept 完！
                while (true) {
                    sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
                    
                    if (client_fd > 0) {
                        setNonBlocking(client_fd);
                        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP; 
                        ev.data.fd = client_fd;
                        epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);

                        clients[client_fd].fd = client_fd;
                        clients[client_fd].header_buf.clear();
                        clients[client_fd].header_received = false;
                        std::cout << "[Server] 新客户端接入，fd = " << client_fd << "\n";
                    } 
                    else if (client_fd < 0) {
                        // 如果没有新连接了（被抽干了），会报 EAGAIN，此时退出循环
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        // 其他真正的错误
                        break; 
                    }
                }
            } 
            // 客户端发数据来了
            else if (events[i].events & EPOLLIN) {
                ClientContext& ctx = clients[fd]; // 取出该客户端的专属状态
                
                // ET 模式下，必须循环读取直到读完
                while (true) {
                    char buf[4096];
                    int bytes = recv(fd, buf, sizeof(buf), 0);
                    
                    if (bytes > 0) {
                        int offset = 0;
                        // 第一步：拼接包头
                        if (!ctx.header_received) {
                            int need = 4 - ctx.header_buf.size();
                            int to_copy = std::min(need, bytes);
                            ctx.header_buf.insert(ctx.header_buf.end(), buf, buf + to_copy);
                            offset += to_copy;

                            if (ctx.header_buf.size() == 4) {
                                // 解析出文件大小
                                uint32_t net_size;
                                memcpy(&net_size, ctx.header_buf.data(), 4);
                                ctx.expected_size = ntohl(net_size);
                                ctx.header_received = true;
                                
                                // 打开文件准备写盘
                                ctx.file_id = ++global_file_id;
                                std::string filename = "data/epoll_received_" + std::to_string(ctx.file_id) + ".mp4";
                                ctx.outfile.open(filename, std::ios::binary);
                                std::cout << "[Server] fd " << fd << " 解析到包头，文件大小: " << ctx.expected_size << " 字节，开始接收...\n";
                            }
                        }

                        // 第二步：写包体数据
                        if (ctx.header_received && offset < bytes) {
                            int body_bytes = bytes - offset;
                            ctx.outfile.write(buf + offset, body_bytes);
                            ctx.received_size += body_bytes;
                        }

                        // 判断是不是收完了
                        if (ctx.header_received && ctx.received_size >= ctx.expected_size) {
                            std::cout << "[Server] fd " << fd << " 文件接收完毕！已保存到本地。\n";
                            ctx.outfile.close();
                            // 收完了，脱离 epoll 监听并关闭连接
                            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                            close(fd);
                            clients.erase(fd);
                            break; // 退出 while 循环
                        }

                    } else if (bytes == 0) {
                        // 对方断开
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                        close(fd);
                        clients.erase(fd);
                        break;
                    } else {
                        // bytes < 0
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // 这是 ET 模式必须处理的：数据读空了，退出 while，下次再来
                            break;
                        } else {
                            // 真正的错误
                            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                            close(fd);
                            clients.erase(fd);
                            break;
                        }
                    }
                }
            }
            // 客户端异常断开事件
            else if (events[i].events & EPOLLRDHUP) {
                int client_fd = events[i].data.fd;
                std::cout << "[Server] 客户端 fd " << client_fd << " 异常断开\n";
                epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
                close(client_fd);
                clients.erase(client_fd);
            }
        }
    }
    close(server_fd);
    close(epfd);
    return 0;
}