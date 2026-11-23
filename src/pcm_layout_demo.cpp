#include <iostream>
#include <vector>
#include <cstdint>
#include <iomanip>

// ==========================================
// 步骤 1：定义 PCM 的元信息（Metadata）
// ==========================================
enum class Layout { Interleaved, Planar };
enum class Endian { LittleEndian, BigEndian };

struct PcmFormat {
    int sampleRate = 48000;      // 采样率：每秒钟多少个采样点
    int bitDepth   = 16;         // 位深：每个采样点占多少 bit
    int channels   = 2;          // 声道数：1=单声道，2=立体声
    Layout layout  = Layout::Interleaved; // 内存布局：交错 vs 平面
    Endian endianness = Endian::LittleEndian; // 字节序：小端 vs 大端
};

// ==========================================
// 步骤 2：多声道 PCM 数据生成与内存布局
// ==========================================
// 模拟采集到的一段立体声数据（假设这是左、右声道的原始样本）
// 注意：真实世界中，左声道和右声道的数据往往是在同一个时间点同时采到的
void createPcmData(const PcmFormat& fmt, int numFrames, std::vector<uint8_t>& outBuffer) {
    // numFrames 指的是“采样帧数”（每个帧包含所有声道在同一时刻的采样）
    // 比如立体声，1 帧 = 1 个 L + 1 个 R 样本
    // 所以总样本数 = numFrames * channels
    int totalSamples = numFrames * fmt.channels;
    int bytesPerSample = fmt.bitDepth / 8;
    
    // 预分配缓冲区大小 = 采样点总数 * 每个采样点的字节数
    outBuffer.resize(totalSamples * bytesPerSample);

    // 我们假装生成一段左声道递增，右声道递减的数据
    // 注意：我们只处理 16bit 有符号整数的情况来演示
    for (int frame = 0; frame < numFrames; ++frame) {
        int16_t leftSample  = static_cast<int16_t>(frame * 100);        // L 声道：0, 100, 200...
        int16_t rightSample = static_cast<int16_t>(65535 - frame * 100); // R 声道：65535, 65435...

        if (fmt.layout == Layout::Interleaved) {
            // 【交错布局】内存排版：L0 R0 L1 R1 L2 R2 ...
            // 索引计算：第 frame 帧的左声道在 (frame * channels) 的位置
            //           第 frame 帧的右声道在 (frame * channels + 1) 的位置
            int idxL = frame * fmt.channels;
            int idxR = frame * fmt.channels + 1;

            // 将 int16_t 按字节写入 vector
            // 这里假设是小端：低字节在前，高字节在后
            outBuffer[idxL * bytesPerSample]     = (leftSample & 0xFF);        // L 低字节
            outBuffer[idxL * bytesPerSample + 1] = (leftSample >> 8) & 0xFF;   // L 高字节
            
            outBuffer[idxR * bytesPerSample]     = (rightSample & 0xFF);       // R 低字节
            outBuffer[idxR * bytesPerSample + 1] = (rightSample >> 8) & 0xFF;  // R 高字节
        } 
        else {
            // 【平面布局】内存排版：L0 L1 L2 ... R0 R1 R2 ...
            // 索引计算：左声道的起始偏移是 0，右声道的起始偏移是 numFrames
            int idxL = frame;                // 左声道连续存放
            int idxR = numFrames + frame;    // 右声道跟在左声道后面

            outBuffer[idxL * bytesPerSample]     = (leftSample & 0xFF);
            outBuffer[idxL * bytesPerSample + 1] = (leftSample >> 8) & 0xFF;
            
            outBuffer[idxR * bytesPerSample]     = (rightSample & 0xFF);
            outBuffer[idxR * bytesPerSample + 1] = (rightSample >> 8) & 0xFF;
        }
    }
}

// ==========================================
// 步骤 3：从内存中读取 PCM 数据
// ==========================================
// 演示从交错布局的内存中还原出原始的 L 和 R 数据
void readInterleavedPcm(const std::vector<uint8_t>& buffer, int numFrames, int channels) {
    std::cout << "\n[读取交错 PCM] 前 4 帧数据：\n";
    for (int frame = 0; frame < numFrames && frame < 4; ++frame) {
        for (int ch = 0; ch < channels; ++ch) {
            int idx = frame * channels + ch;
            int byteOffset = idx * 2; // 16bit = 2 字节

            // 小端模式重组：低字节 + 高字节 << 8
            int16_t sample = (buffer[byteOffset] | (buffer[byteOffset + 1] << 8));
            
            std::cout << "  Frame " << frame << " | " 
                      << (ch == 0 ? "左(L)" : "右(R)") << " = " << sample << "\n";
        }
    }
}

// ==========================================
// 步骤 4：主函数 & 测试
// ==========================================
int main() {
    PcmFormat fmt;
    fmt.layout = Layout::Interleaved; // 测试交错
    int numFrames = 4; // 只生成 4 帧

    std::vector<uint8_t> pcmBuffer;
    createPcmData(fmt, numFrames, pcmBuffer);

    // 打印内存里的原始字节（十六进制），验证到底是怎么排列的
    std::cout << "[交错布局内存大小] " << pcmBuffer.size() << " 字节 (4 帧 * 2 声道 * 2 字节)\n";
    std::cout << "[内存十六进制打印] ";
    for (uint8_t byte : pcmBuffer) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
    }
    std::cout << std::dec << "\n";

    // 读取并打印
    readInterleavedPcm(pcmBuffer, numFrames, fmt.channels);

    // 测试平面布局
    std::cout << "\n--- 切换成平面布局 ---\n";
    fmt.layout = Layout::Planar;
    createPcmData(fmt, numFrames, pcmBuffer);
    std::cout << "[平面布局内存大小] " << pcmBuffer.size() << " 字节\n";
    std::cout << "[内存十六进制打印] ";
    for (uint8_t byte : pcmBuffer) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
    }
    std::cout << std::dec << "\n";

    return 0;
}