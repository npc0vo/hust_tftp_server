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
 * ����ʽ�����ַ���д�뵽��־�ļ��С�
 *
 * @param filename ��־�ļ���·��������ʹ��Windows����·���ָ�����
 * @param format ��ʽ���ַ�����
 * @param ... �ɱ�����б�
 * @return �ɹ����طǸ�ֵ��ʧ�ܷ��ظ�ֵ��
 */
int mylog(const char* filename, const char* format, ...) {
    FILE* file;
    va_list args;
    int result;

    // ���ļ�������ļ��������򴴽�������ļ��Ѵ�����׷�����ݡ�
    file = fopen(filename, "a"); // "a" ģʽ��ʾ׷��ģʽ��
    if (file == NULL) {
        perror("Error opening file");
        return -1; // �ļ���ʧ�ܡ�
    }

    // ��ʼ���ɱ�����б�
    va_start(args, format);

    // ʹ��vfprintf����ʽ���ַ���д���ļ���
    result = vfprintf(file, format, args);

    // ����ɱ�����б�
    va_end(args);

    // �ر��ļ���
    fclose(file);

    return result;
}




void send_error(SOCKET sock, struct sockaddr_in* client_addr, int addr_len, int error_code, const char* error_msg)
{
    char error_packet[516];
    *(unsigned short*)error_packet = htons(CMD_ERROR);												// �����루2�ֽڣ�
    *(unsigned short*)(error_packet + 2) = htons(error_code);										// �����루2�ֽڣ�
    strcpy(error_packet + 4, error_msg);															// ������Ϣ
    sendto(sock, error_packet, strlen(error_msg) + 5, 0, (sockaddr*)client_addr, addr_len); // ���ʹ����
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
    // ��������󣨴ӷ����������ļ���
    int addr_len = sizeof(request->client);

    mylog(LOG_PATH, "Handling RRQ: Download %s in %s mode\n", filename, mode);
    // build fullpath
    memset(fullpath, 0, sizeof(fullpath));
    strcpy(fullpath, TFTP_SERVER_ROOT);
    if (r_path[0] != '/') {
        strcat(fullpath, "/");
    }
    strcat(fullpath, r_path);
    //תΪunix����·��
    for (size_t i = 0; i < strlen(fullpath); ++i) {
        if (fullpath[i] == '\\') {
            fullpath[i] = '/';
        }
    }
    mylog(LOG_PATH, "rrq: \"%s\", blocksize=%d\n", fullpath, 512);
    // ��Ҫ��ȡ���ļ�
    FILE* file = fopen(fullpath, mode[0] == 'o' ? "rb" : "r");
    if (!file)
    {
        // ����ļ������ڣ������ļ�δ�ҵ��Ĵ����

        send_error(sock, &(request->client), addr_len, 6, TFTP_error_messages[6]);
        mylog(LOG_PATH, "Error: File not found.");
        return;
    }

    char data_packet[BUFFER_SIZE];	 // ���ݰ�������
    unsigned short block_number = 1; // ��ʼ�����ݿ���
    int bytes_read;

    clock_t start_time = clock(); // ��ʼ��ʱ

    // ѭ����鷢���ļ�����
    do
    {
        memset(data_packet, 0, BUFFER_SIZE);						// ������ݰ�������
        *(unsigned short*)data_packet = htons(CMD_DATA);			// �����룺���ݰ�
        *(unsigned short*)(data_packet + 2) = htons(block_number); // ���ݿ���

        bytes_read = fread(data_packet + 4, 1, DATA_SIZE, file); // ��ȡ�ļ�����

        int retries = 0;
        bool send_packet = true;
        time_t last_ack_time; // ��ʼ����ʱ��

        while (retries < MAX_RETRIES)
        {
            if (send_packet)
            {
                sendto(sock, data_packet, bytes_read + 4, 0, (struct sockaddr*)&(request->client), addr_len); // �������ݰ�
                last_ack_time = time(NULL);
            }

            // ���ó�ʱʱ��
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(sock, &read_fds);
            struct timeval timeout;
            timeout.tv_sec = TIMEOUT_SEC;
            timeout.tv_usec = 0;

            // �ȴ�ACKȷ�ϰ�
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
                        // �յ���Ч��ACK�����˳��ش�ѭ��
                        retries = 0;
                        last_ack_time = time(NULL); // ���ü�ʱ��
                        break;
                    }
                    else if (received_block_number == block_number - 1)
                    {
                        // �յ���Ч��ACK���������ȴ���ȷ��ACK��
                        mylog(LOG_PATH, "Received invalid ACK. Expected block number: %d, Received block number: %d\n", block_number, received_block_number);
                        if (difftime(time(NULL), last_ack_time) >= TIMEOUT_SEC)
                        {
                            // �����ʱ����ʱ���ش����ݰ�
                            mylog(LOG_PATH, "Retransmitting block number triggered by bad packets: %d, Retry count: %d\n", block_number, retries + 1);
                            send_packet = true;
                            last_ack_time = time(NULL); // ���ü�ʱ��
                            retries++;
                        }
                    }
                }
            }
            else
            {
                // ����ʱ���Ƿ�ʱ
                if (difftime(time(NULL), last_ack_time) >= TIMEOUT_SEC)
                {
                    // ��¼�ش��Ŀ�ź��ش��Ĵ���
                    retries++;
                    mylog(LOG_PATH, "Retransmitting block number triggered by timeout: %d, Retry count: %d\n", block_number, retries + 1);
                    send_packet = true;
                    last_ack_time = time(NULL); // ���ü�ʱ��
                }
                else
                {
                    send_packet = false;
                }
            }
        }

        if (retries == MAX_RETRIES)
        {
            // ����ﵽ����ش���������¼�����˳�
            mylog(LOG_PATH, "Error: Maximum retries reached, transfer aborted.\n");
            if (bytes_read < DATA_SIZE)
            {
                mylog(LOG_PATH, "This is the last packet to send. Maybe client's lst ack lost?\n");
            }
            send_error(sock, (sockaddr_in*)&(request->client), addr_len, 0, TFTP_ERROR_0);
            fclose(file);
            return;
        }

        bytes_transferred += bytes_read; // �ۼ��Ѵ�����ֽ���
        block_number++;					 // ������һ������
    } while (bytes_read == DATA_SIZE); // ������ݿ��С����512�ֽڣ���ʾ�ļ��Ѵ������

    fclose(file); // �ر��ļ�

    // ���㲢��ʾ����������
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
    clock_t start_time = clock(); // ��ʼ��ʱ

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

    //תΪunix����·��
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
    // ����ACKȷ�ϰ���ACK���а�����һ�����յĿ���
    // ѭ�����տͻ����ϴ�������
    send_ack(sock, &ack_packet, sizeof(ack_packet));
    while (1)
    {
        // ���ó�ʱʱ��
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        struct timeval timeout;
        timeout.tv_sec = TIMEOUT_SEC;
        timeout.tv_usec = 0;

        // �ȴ����ݰ�
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
                    // �յ��µ����ݿ�
                    block++;							   // ���ŵ���
                    bytes_transferred += recv_len - 4;		   // �ۼ��ѽ��յ��ֽ���
                    fwrite(buffer + 4, 1, recv_len - 4, fp); // ������д���ļ�
                    retries = 0;							   // �����ش�������
                    // ���ü�ʱ��
                    timeout.tv_sec = TIMEOUT_SEC;
                    timeout.tv_usec = 0;
                    ack_packet.block = htons(block);
                    send_ack(sock, &ack_packet, sizeof(ack_packet));
                    // ���յ�С��512��DATA��ʱ,�˳�
                    if (recv_len < DATA_SIZE + 4)
                    {
                        break;
                    }
                }
                else if (received_block_number == block)
                {
                    // �յ��ظ������ݿ飬���·���ACKȷ�ϰ�
                    ack_packet.block = htons(block);

                    send_ack(sock, &ack_packet, sizeof(ack_packet));
                    // ��¼�յ��ظ������ݿ�
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
            // ��ʱδ�յ����ݰ��������ش�����
            retries++;
            if (retries >= MAX_RETRIES)
            {
                // �ﵽ����ش���������¼�����˳�
                mylog(LOG_PATH, "Error: Timeout waiting for WRQ data packet, transfer aborted.\n");
                send_error(sock, (sockaddr_in*)&(request->client), sizeof(sockaddr_in), 0, "WRQ Time Out");
                fclose(fp);
                return;
            }
            else
            {
                // ��¼�ȴ��ش��Ŀ�ź��ش��Ĵ���
                mylog(LOG_PATH, "Awaiting retransmitting block number: %d, Retry count: %d\n", block + 1, retries);
                ack_packet.block = htons(block);
                send_ack(sock, &ack_packet, sizeof(ack_packet)); // �ش�ACKȷ�ϰ�
            }
        }
    }
    // ������յ������ݿ�С��512�ֽڣ��������

    // ���Ͷ����һ������ACK
    send_ack(sock, &ack_packet, sizeof(ack_packet)); // �ش�ACKȷ�ϰ�;
    fclose(fp); // �ر��ļ�
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
        mylog(LOG_PATH, "��ʼ��ʧ��: %d\n", WSAGetLastError());
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        mylog(LOG_PATH, "�����׽���ʧ��: %d\n", WSAGetLastError());
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
    mylog(LOG_PATH, "server�Ѿ�����\n");
    mylog(LOG_PATH, "port: %d", SERVER_PORT);

    //CString Message;
    //Message.Format("Server Start,listening on %d", SERVER_PORT);
    //dlg->PostMessage(WM_ADDLOG, 0, (LPARAM)&Message);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        mylog(LOG_PATH, "��ʼ��ʧ��: %d\n", WSAGetLastError());
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket == INVALID_SOCKET) {
        mylog(LOG_PATH, "�����׽���ʧ��: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    //����Ϊ������ģʽ
    u_long mode = 1;
    ioctlsocket(serverSocket, FIONBIO, &mode);

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(params->port);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        mylog(LOG_PATH, "��ʧ��: %d\n", WSAGetLastError());
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    mylog(LOG_PATH, "���������ڼ���...\n");
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
                // û�����ݿɶ��������ֹ��־
                Sleep(100); // ���������Ա���æ�ȴ�
                continue;

            }
            else {
                Sleep(100);
                continue;
            }
        }
    }


    mylog(LOG_PATH, "������ֹͣ\n");


    closesocket(serverSocket);
    WSACleanup();
    return 0;
}



#endif
