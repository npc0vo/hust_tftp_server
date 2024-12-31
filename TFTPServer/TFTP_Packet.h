#ifndef TFTP_PACKET
#define TFTP_PACKET
#define _CRT_SECURE_NO_WARNINGS


/* Opcodes */
#define CMD_RRQ    1
#define CMD_WRQ    2
#define CMD_DATA   3
#define CMD_ACK	   4
#define CMD_ERROR  5
#define CMD_LIST 6
#define CMD_HEAD 7

#define TFTP_PACKET_MAX_SIZE 1024
#define TFTP_PACKET_DATA_SIZE 512
#define DATA_SIZE   	 512
#define MAX_FILENAME	 255
#define INITIAL_BLOCK 0
#define MAX_TIMEOUTS	 5
#define TIMEOUT_SECS	3
#define BUFFER_SIZE 516
// Max request datagram size
#define MAX_REQUEST_SIZE 1024
// TFTPX_DATA_SIZE
#define DATA_SIZE 512
//
#define LIST_BUF_SIZE (DATA_SIZE * 8)


// Max packet retransmission.
#define PKT_MAX_RXMT 3
#define MAX_RETRIES 3
#define TIMEOUT_SEC 3
// usecond
#define PKT_SND_TIMEOUT 12*1000*1000
#define PKT_RCV_TIMEOUT 3*1000*1000
// usecond
#define PKT_TIME_INTERVAL 5*1000

typedef struct TFTP_Packet {
	unsigned short int cmd;
	union {
		unsigned short int code;
		unsigned short int block;
		// For a RRQ and WRQ TFTP packet
		char filename[2];
	};
	char data[DATA_SIZE];
};


typedef struct TFTP_Request {
	int size;
	sockaddr_in client;
	TFTP_Packet packet;
};



typedef struct {
	unsigned short int opcode;
	char filename[MAX_FILENAME];
	char zero_0;
	char mode[MAX_FILENAME];
	char zero_1;
} TFTP_Packet2;

typedef struct {
	unsigned short int opcode;
	unsigned short int block;
	char data[DATA_SIZE];
} TFTP_Data;

typedef struct {
	unsigned short int opcode;
	unsigned short int block;
} TFTP_Ack;

static const char* TFTP_error_messages[] = {
	"Undefined error",                 // Error code 0
	"File not found",                  // 1
	"Access violation",                // 2
	"Disk full or allocation error",   // 3
	"Illegal TFTP operation",          // 4
	"Unknown transfer ID",             // 5
	"File already exists",             // 6
	"No such user"                     // 7
};

//mode:"netascii", "octet", or "mail"
#define TFTP_DEFAULT_TRANSFER_MODE "octet"






#endif



