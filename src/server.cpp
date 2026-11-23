#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <thread>     // 引入多线程库
#include <algorithm>
#include "SafeQueue.h" // 引入刚才写的安全队列

bool recvAll(int sock, char* data, size_t length) {
    size_t total_received = 0;
    while (total_received < length) {
        int bytes = recv(sock, data + total_received, length - total_received, 0);
        if (bytes <= 0) return false; 
        total_received += bytes;
    }
    return true;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8888);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, 5);
    
    std::cout << "服务端已启动（多线程版），持续监听 8888 端口...\n";

    int file_count = 0;

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        std::cout << "等待新的客户端连接...\n";
        int client_socket = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) continue;
        
        file_count++;
        std::cout << "客户端已连接！准备接收第 " << file_count << " 个文件。\n";

        // 每次接收一个新文件，都创建一个新的队列
        SafeQueue file_queue;
        std::string filename = "data/received_" + std::to_string(file_count) + ".mp4";

        // 启动消费者线程（写盘线程）
        // 用 lambda 表达式开启一个后台线程，专门负责写文件
        std::thread disk_writer([&file_queue, filename]() {
            std::ofstream outfile(filename, std::ios::binary);
            std::vector<char> chunk;
            
            // 只要队列没关闭，就一直尝试从队列取数据
            while (file_queue.pop(chunk)) {
                outfile.write(chunk.data(), chunk.size());
            }
            outfile.close();
            std::cout << "[后台写盘线程] 文件 " << filename << " 写入完成。\n";
        });

        // --- 主线程（生产者）：负责从网络收数据 ---
        uint32_t net_file_size = 0;
        if (!recvAll(client_socket, reinterpret_cast<char*>(&net_file_size), sizeof(net_file_size))) {
            std::cerr << "接收包头失败\n"; 
            close(client_socket);
            file_queue.shutdown(); // 通知写盘线程下班
            disk_writer.join();
            continue;
        }

        uint32_t file_size = ntohl(net_file_size);
        std::cout << "解析到包头，文件总大小为: " << file_size << " 字节\n";

        const int BUFFER_SIZE = 4096;
        char buffer[BUFFER_SIZE];
        uint32_t total_received = 0;

        while (total_received < file_size) {
            uint32_t bytes_to_read = std::min(static_cast<uint32_t>(BUFFER_SIZE), file_size - total_received);
            int bytes_received = recv(client_socket, buffer, bytes_to_read, 0);
            if (bytes_received <= 0) break;
            
            // 把收到的数据转成 vector，塞进队列
            // 注意：这里必须拷贝一份数据放进 vector，因为下一次 recv 会覆盖 buffer
            std::vector<char> chunk(buffer, buffer + bytes_received);
            file_queue.push(std::move(chunk)); // 塞进队列，通知后台写盘

            total_received += bytes_received;
        }

        std::cout << "[网络接收线程] 数据接收完毕，实际接收: " << total_received << " 字节\n";
        
        // 收完了，关闭队列，告诉后台线程“没有新数据了，写完剩下的就下班吧”
        file_queue.shutdown();
        
        // 等待后台线程把队列里剩余的数据全部写完，回收线程资源
        disk_writer.join();

        close(client_socket);
    }

    close(server_fd);
    return 0;
}