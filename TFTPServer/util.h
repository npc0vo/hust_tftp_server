#ifndef MAIN_HEADER
#define MAIN_HEADER
#include <io.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <exception>
#include <vector>
#include <direct.h>
#include "pch.h"
#include <sys/stat.h>
#include "TFTP_Packet.h"
#include "TFTPServerDlg.h"
#define WM_ADDLOG (WM_USER + 1000)
#define WM_UPDLOG (WM_USER + 2)
#define DEBUG

#include <winsock2.h>


#define TFTP_ERROR_0 "Not defined, see error message (if any)"
#define TFTP_ERROR_1 "File not found"
#define TFTP_ERROR_2 "Access violation"
#define TFTP_ERROR_3 "Disk full or allocation exceeded"
#define TFTP_ERROR_4 "Illegal TFTP operation"
#define TFTP_ERROR_5 "Unknown transfer ID"
#define TFTP_ERROR_6 "File already exists"
#define TFTP_ERROR_7 "No such user"	
#define LOG_PATH "tftpserver.log"
#define _CRT_SECURE_NO_WARNINGS
#include <stdarg.h>


char TFTP_SERVER_ROOT[MAX_FILENAME];
int SERVER_PORT = 69;
CTFTPServerDlg* mydlg;
struct ThreadParams {
    int port;
    int done;
    CTFTPServerDlg* dlg;
};

struct handleParams {
    struct ThreadParams threadParams;
    TFTP_Request request;
};

int Done = 0;


/**
 * 将格式化的字符串写入到日志文件中。
 *
 * @param filename 日志文件的路径，可以使用Windows风格的路径分隔符。
 * @param format 格式化字符串。
 * @param ... 可变参数列表。
 * @return 成功返回非负值，失败返回负值。
 */
int mylog(const char* filename, const char* format, ...) {
    FILE* file;
    va_list args;
    int result;

    // 打开文件，如果文件不存在则创建，如果文件已存在则追加内容。
    file = fopen(filename, "a"); // "a" 模式表示追加模式。
    if (file == NULL) {
        perror("Error opening file");
        return -1; // 文件打开失败。
    }

    // 初始化可变参数列表。
    va_start(args, format);

    // 使用vfprintf将格式化字符串写入文件。
    result = vfprintf(file, format, args);

    // 清理可变参数列表。
    va_end(args);

    // 关闭文件。
    fclose(file);

    return result;
}




void send_error(SOCKET sock, struct sockaddr_in* client_addr, int addr_len, int error_code, const char* error_msg)
{
    char error_packet[516];
    *(unsigned short*)error_packet = htons(CMD_ERROR);												// 操作码（2字节）
    *(unsigned short*)(error_packet + 2) = htons(error_code);										// 错误码（2字节）
    strcpy(error_packet + 4, error_msg);															// 错误消息
    sendto(sock, error_packet, strlen(error_msg) + 5, 0, (sockaddr*)client_addr, addr_len); // 发送错误包
}

int send_ack(int sock, TFTP_Packet* packet, int size) {

    if (send(sock, (char*)packet, size, 0) != size) {
        return -1;
    }

    return size;
}

void handle_rrq(int sock, TFTP_Request* request) {
    TFTP_Packet snd_packet;
    char fullpath[256];
    char buffer[BUFFER_SIZE];
    char* r_path = request->packet.filename;	// request file
    char* mode = r_path + strlen(r_path) + 1;
    char* blocksize_str = mode + strlen(mode) + 1;
    int blocksize = atoi(blocksize_str);
    int recv_len = 0;
    int bytes_transferred = 0;
    char* filename = request->packet.filename;
    // 处理读请求（从服务器下载文件）
    int addr_len = sizeof(request->client);

    mylog(LOG_PATH, "Handling RRQ: Download %s in %s mode\n", filename, mode);
    // build fullpath
    memset(fullpath, 0, sizeof(fullpath));
    strcpy(fullpath, TFTP_SERVER_ROOT);
    if (r_path[0] != '/') {
        strcat(fullpath, "/");
    }
    strcat(fullpath, r_path);
    //转为unix风格的路径
    for (size_t i = 0; i < strlen(fullpath); ++i) {
        if (fullpath[i] == '\\') {
            fullpath[i] = '/';
        }
    }
    mylog(LOG_PATH, "rrq: \"%s\", blocksize=%d\n", fullpath, 512);
    // 打开要读取的文件
    FILE* file = fopen(fullpath, mode[0] == 'o' ? "rb" : "r");
    if (!file)
    {
        // 如果文件不存在，发送文件未找到的错误包

        send_error(sock, &(request->client), addr_len, 6, TFTP_error_messages[6]);
        mylog(LOG_PATH, "Error: File not found.");
        return;
    }

    char data_packet[BUFFER_SIZE];	 // 数据包缓冲区
    unsigned short block_number = 1; // 初始化数据块编号
    int bytes_read;

    clock_t start_time = clock(); // 开始计时

    // 循环逐块发送文件内容
    do
    {
        memset(data_packet, 0, BUFFER_SIZE);						// 清空数据包缓冲区
        *(unsigned short*)data_packet = htons(CMD_DATA);			// 操作码：数据包
        *(unsigned short*)(data_packet + 2) = htons(block_number); // 数据块编号

        bytes_read = fread(data_packet + 4, 1, DATA_SIZE, file); // 读取文件数据

        int retries = 0;
        bool send_packet = true;
        time_t last_ack_time; // 初始化计时器

        while (retries < MAX_RETRIES)
        {
            if (send_packet)
            {
                sendto(sock, data_packet, bytes_read + 4, 0, (struct sockaddr*)&(request->client), addr_len); // 发送数据包
                last_ack_time = time(NULL);
            }

            // 设置超时时间
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(sock, &read_fds);
            struct timeval timeout;
            timeout.tv_sec = TIMEOUT_SEC;
            timeout.tv_usec = 0;

            // 等待ACK确认包
            int select_result = select(0, &read_fds, NULL, NULL, &timeout);
            if (select_result > 0)
            {
                recv_len = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&(request->client), &addr_len);;
                if (recv_len >= 4)
                {
                    unsigned short received_opcode = ntohs(*(unsigned short*)buffer);
                    unsigned short received_block_number = ntohs(*(unsigned short*)(buffer + 2));

                    if (received_opcode == CMD_ACK && received_block_number == block_number)
                    {
                        // 收到有效的ACK包，退出重传循环
                        retries = 0;
                        last_ack_time = time(NULL); // 重置计时器
                        break;
                    }
                    else if (received_block_number == block_number - 1)
                    {
                        // 收到无效的ACK包，继续等待正确的ACK包
                        mylog(LOG_PATH, "Received invalid ACK. Expected block number: %d, Received block number: %d\n", block_number, received_block_number);
                        if (difftime(time(NULL), last_ack_time) >= TIMEOUT_SEC)
                        {
                            // 如果计时器超时，重传数据包
                            mylog(LOG_PATH, "Retransmitting block number triggered by bad packets: %d, Retry count: %d\n", block_number, retries + 1);
                            send_packet = true;
                            last_ack_time = time(NULL); // 重置计时器
                            retries++;
                        }
                    }
                }
            }
            else
            {
                // 检查计时器是否超时
                if (difftime(time(NULL), last_ack_time) >= TIMEOUT_SEC)
                {
                    // 记录重传的块号和重传的次数
                    retries++;
                    mylog(LOG_PATH, "Retransmitting block number triggered by timeout: %d, Retry count: %d\n", block_number, retries + 1);
                    send_packet = true;
                    last_ack_time = time(NULL); // 重置计时器
                }
                else
                {
                    send_packet = false;
                }
            }
        }

        if (retries == MAX_RETRIES)
        {
            // 如果达到最大重传次数，记录错误并退出
            mylog(LOG_PATH, "Error: Maximum retries reached, transfer aborted.\n");
            if (bytes_read < DATA_SIZE)
            {
                mylog(LOG_PATH, "This is the last packet to send. Maybe client's lst ack lost?\n");
            }
            send_error(sock, (sockaddr_in*)&(request->client), addr_len, 0, TFTP_ERROR_0);
            fclose(file);
            return;
        }

        bytes_transferred += bytes_read; // 累加已传输的字节数
        block_number++;					 // 发送下一块数据
    } while (bytes_read == DATA_SIZE); // 如果数据块大小不足512字节，表示文件已传输完毕

    fclose(file); // 关闭文件

    // 计算并显示传输吞吐量
    double time_elapsed = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    double down_bps = bytes_transferred / time_elapsed;
    double down_mbs = down_bps / (8 * 1024 * 1024);
    mylog(LOG_PATH, "File transfer completed. Throughput: %.2f Bps %.2f MBS\n", down_bps, down_mbs);
}

void handle_wrq(int sock, TFTP_Request* request) {
    TFTP_Packet ack_packet, rcv_packet;
    char fullpath[256];
    char* r_path = request->packet.filename;	// request file
    char* mode = r_path + strlen(r_path) + 1;
    char* blocksize_str = mode + strlen(mode) + 1;
    int blocksize = atoi(blocksize_str);
    clock_t start_time = clock(); // 开始计时

    if (blocksize <= 0 || blocksize > DATA_SIZE) {
        blocksize = DATA_SIZE;
    }

    if (strlen(r_path) + strlen(TFTP_SERVER_ROOT) > sizeof(fullpath) - 1) {
        mylog(LOG_PATH, "Request path too long. %d\n", strlen(r_path) + strlen(TFTP_SERVER_ROOT));
        return;
    }

    // build fullpath
    memset(fullpath, 0, sizeof(fullpath));
    strcpy(fullpath, TFTP_SERVER_ROOT);
    if (r_path[0] != '/') {
        strcat(fullpath, "/");
    }
    strcat(fullpath, r_path);

    //转为unix风格的路径
    for (size_t i = 0; i < strlen(fullpath); ++i) {
        if (fullpath[i] == '\\') {
            fullpath[i] = '/';
        }
    }
    mylog(LOG_PATH, "wrq: \"%s\", blocksize=%d\n", fullpath, blocksize);
    mylog(LOG_PATH, "Handling WRQ: Upload %s in %s mode\n", fullpath, mode);


    FILE* fp = fopen(fullpath, "r");
    if (fp != NULL) {
        // send error packet
        fclose(fp);
        send_error(sock, (sockaddr_in*)&(request->client), sizeof(sockaddr_in), 1, "File not found");
        mylog(LOG_PATH, "File \"%s\" already exists.\n", fullpath);
        return;
    }

    fp = fopen(fullpath, mode[0] == 'o' ? "wb" : "w");

    if (fp == NULL) {
        mylog(LOG_PATH, "File \"%s\" create error.\n", fullpath);
        return;
    }

    ack_packet.cmd = htons(CMD_ACK);
    ack_packet.block = htons(0);

    char buffer[BUFFER_SIZE];
    int s_size = 0;
    int r_size = 0;
    unsigned short int block = 0;
    int time_wait_data;
    int bytes_transferred = 0;
    int recv_len = 0;
    int retries = 0;
    socklen_t addr_len = sizeof(sockaddr_in);
    clock_t start_time2 = clock();
    // 发送ACK确认包，ACK包中包含上一个接收的块编号
    // 循环接收客户端上传的数据
    send_ack(sock, &ack_packet, sizeof(ack_packet));
    while (1)
    {
        // 设置超时时间
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        struct timeval timeout;
        timeout.tv_sec = TIMEOUT_SEC;
        timeout.tv_usec = 0;

        // 等待数据包
        int select_result = select(0, &read_fds, NULL, NULL, &timeout);
        if (select_result > 0)
        {
            recv_len = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&(request->client), &addr_len);

            unsigned short opcode = ntohs(*(unsigned short*)buffer);

            if (recv_len >= 4 && opcode == CMD_DATA)
            {
                unsigned short received_block_number = ntohs(*(unsigned short*)(buffer + 2));
                if (received_block_number == block + 1)
                {
                    // 收到新的数据块
                    block++;							   // 块编号递增
                    bytes_transferred += recv_len - 4;		   // 累加已接收的字节数
                    fwrite(buffer + 4, 1, recv_len - 4, fp); // 将数据写入文件
                    retries = 0;							   // 重置重传计数器
                    // 重置计时器
                    timeout.tv_sec = TIMEOUT_SEC;
                    timeout.tv_usec = 0;
                    ack_packet.block = htons(block);
                    send_ack(sock, &ack_packet, sizeof(ack_packet));
                    // 当收到小于512的DATA包时,退出
                    if (recv_len < DATA_SIZE + 4)
                    {
                        break;
                    }
                }
                else if (received_block_number == block)
                {
                    // 收到重复的数据块，重新发送ACK确认包
                    ack_packet.block = htons(block);

                    send_ack(sock, &ack_packet, sizeof(ack_packet));
                    // 记录收到重复的数据块
                    mylog(LOG_PATH, "Received duplicate block number: %d \n", received_block_number);
                }
            }
            else if (opcode == CMD_WRQ)
            {
                mylog(LOG_PATH, "Received duplicated WRQ during WRQ process\n");
            }
        }
        else
        {
            // 超时未收到数据包，增加重传计数
            retries++;
            if (retries >= MAX_RETRIES)
            {
                // 达到最大重传次数，记录错误并退出
                mylog(LOG_PATH, "Error: Timeout waiting for WRQ data packet, transfer aborted.\n");
                send_error(sock, (sockaddr_in*)&(request->client), sizeof(sockaddr_in), 0, "WRQ Time Out");
                fclose(fp);
                return;
            }
            else
            {
                // 记录等待重传的块号和重传的次数
                mylog(LOG_PATH, "Awaiting retransmitting block number: %d, Retry count: %d\n", block + 1, retries);
                ack_packet.block = htons(block);
                send_ack(sock, &ack_packet, sizeof(ack_packet)); // 重传ACK确认包
            }
        }
    }
    // 如果接收到的数据块小于512字节，则传输结束

    // 发送对最后一个包的ACK
    send_ack(sock, &ack_packet, sizeof(ack_packet)); // 重传ACK确认包;
    fclose(fp); // 关闭文件
    double time_elapsed = (double)(clock() - start_time2) / CLOCKS_PER_SEC;
    double upd_bps = bytes_transferred / time_elapsed;
    double upd_mbs = upd_bps / (8 * 1024 * 1024);
    mylog(LOG_PATH, "File transfer completed. Throughput: %.2f Bps %.2f MBS\n", upd_bps, upd_mbs);
    return;
}


UINT handleClient(LPVOID pParam)
{
    TFTP_Request* request = reinterpret_cast<TFTP_Request*>(pParam);


    mylog(LOG_PATH, "size:%d\n", request->size);
    sockaddr_in server;
    socklen_t addr_len = sizeof(sockaddr_in);

    if (request->size <= 0) {
        mylog(LOG_PATH, "Bad request.\n");
        return NULL;
    }

    mylog(LOG_PATH, "work_thread started.\n");

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        mylog(LOG_PATH, "初始化失败: %d\n", WSAGetLastError());
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        mylog(LOG_PATH, "创建套接字失败: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }


    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = 0;

    if (bind(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        mylog(LOG_PATH, "work_thread bind failed.\n");
        return NULL;
    }

    if (connect(sock, (struct sockaddr*)&(request->client), addr_len) < 0) {
        mylog(LOG_PATH, "Can't connect to client.\n");
        return NULL;
    }

    CString LOGMessage;

    // Choose handler
    switch (request->packet.cmd) {
    case CMD_RRQ:
        mylog(LOG_PATH, "handle_rrq called.\n");

        handle_rrq(sock, request);
        break;
    case CMD_WRQ:
        mylog(LOG_PATH, "handle_wrq called.\n");

        handle_wrq(sock, request);
        break;
    default:
        mylog(LOG_PATH, "Illegal TFTP operation.\n");

        break;
    }


    free(request);
    closesocket(sock);
    return NULL;
}

UINT startServer(LPVOID pParam)
{
    ThreadParams* params = reinterpret_cast<ThreadParams*>(pParam);
    CTFTPServerDlg* dlg = params->dlg;
    mydlg = dlg;
    mylog(LOG_PATH, "server已经启动\n");
    mylog(LOG_PATH, "port: %d", SERVER_PORT);

    //CString Message;
    //Message.Format("Server Start,listening on %d", SERVER_PORT);
    //dlg->PostMessage(WM_ADDLOG, 0, (LPARAM)&Message);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        mylog(LOG_PATH, "初始化失败: %d\n", WSAGetLastError());
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket == INVALID_SOCKET) {
        mylog(LOG_PATH, "创建套接字失败: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    //设置为非阻塞模式
    u_long mode = 1;
    ioctlsocket(serverSocket, FIONBIO, &mode);

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(params->port);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        mylog(LOG_PATH, "绑定失败: %d\n", WSAGetLastError());
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    mylog(LOG_PATH, "服务器正在监听...\n");
    TFTP_Request* request;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    while (!Done) {
        request = (TFTP_Request*)malloc(sizeof(TFTP_Request));
        memset(request, 0, sizeof(TFTP_Request));
        request->size = recvfrom(serverSocket, (char*)&(request->packet), MAX_REQUEST_SIZE, 0,
            (struct sockaddr*)&(request->client),
            &addr_len);

        if (request->size != SOCKET_ERROR) {
            request->packet.cmd = ntohs(request->packet.cmd);
            mylog(LOG_PATH, "Receving clients' requests...\n");



            CWinThread* pHandleThread = AfxBeginThread(handleClient, request);
        }
        else {
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                // 没有数据可读，检查终止标志
                Sleep(100); // 短暂休眠以避免忙等待
                continue;

            }
            else {
                Sleep(100);
                continue;
            }
        }
    }


    mylog(LOG_PATH, "服务器停止\n");


    closesocket(serverSocket);
    WSACleanup();
    return 0;
}



#endif
