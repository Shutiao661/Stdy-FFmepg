#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fstream>

// 封装一个“保证发完”的函数，防止 send 没发完就返回
bool sendAll(int sock, const char* data, size_t length)
{
    size_t total_sent = 0;
    while (total_sent < length)
    {
        int bytes = send(sock, data + total_sent, length - total_sent, 0);
        if (bytes <= 0) return false; // 发送失败
        total_sent += bytes;
    }
    return true;
}
int main()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == -1)
    {
        std::cerr << "连接失败\n"; return -1;
    }
    std::cout << "连接成功，准备发送文件...\n";
    // ate：打开后直接跳到文件末尾，方便获取大小
    std::ifstream file("data/demo.mp4", std::ios::binary | std::ios::ate);
    if (!file)
    {
        std::cerr << "找不到文件\n"; return -1;
    }
    // 1. 获取文件大小
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);// 回到文件开头，准备读数据
    std::cout << "文件大小: " << file_size << " 字节\n";
    // 2. 发送包头 (4 字节的文件大小，转换为网络字节序)
    uint32_t net_file_size = htonl(static_cast<uint32_t>(file_size));
    if (!sendAll(sock, reinterpret_cast<const char*>(&net_file_size), sizeof(net_file_size)))
    {
        std::cerr << "发送包头失败\n"; return -1;
    }
    // 3. 发送文件数据
    const int CHUNK_SIZE = 4096;
    char buffer[CHUNK_SIZE];
    while (file.read(buffer, CHUNK_SIZE) || file.gcount() > 0)
    {
        int bytes_to_send = file.gcount();
        if (!sendAll(sock, buffer, bytes_to_send))
        {
            std::cerr << "发送文件块失败\n"; break;
        }
    }
    std::cout << "发送完毕。\n";
    file.close();
    close(sock);
    return 0;
}