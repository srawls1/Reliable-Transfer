// Spencer Rawls
// SenderSocket.h

#pragma once
#include "stdafx.h"
#include "Semaphore.h"
#define STATUS_OK 0
#define ALREADY_CONNECTED 1
#define NOT_CONNECTED 2
#define INVALID_NAME 3
#define FAILED_SEND 4
#define TIMEOUT 5
#define FAILED_RECV 6 

#define MAGIC_PORT 22345
#define MAGIC_PROTOCOL 0x8311AA
#define MAX_PKT_SIZE 1472
#define MAX_SYN 50
#define MAX_RETX 50
#define MIN_RTO 300
#define MAX_RTO 1000


#pragma pack(push, 1)
struct Flags
{
	DWORD reserved : 5;
	DWORD SYN : 1;
	DWORD ACK : 1;
	DWORD FIN : 1;
	DWORD magic : 24;
};

struct SenderDataHeader
{
	Flags flags;
	DWORD seq;
};

struct LinkProperties
{
	float RTT;
	float speed;
	float pLoss[2];
	DWORD bufferSize;
};

struct SenderSynHeader
{
	SenderDataHeader sdh;
	LinkProperties lp;
};

struct ReceiverHeader
{
	Flags flags;
	DWORD recvWnd;
	DWORD ackSeq;
};
#pragma pack(pop)

enum PacketStatus
{
	status_free,
	status_ready,
	status_sent
};

enum PacketType
{
	type_syn,
	type_fin,
	type_data
};

struct BufferedPacket
{
	PacketStatus status;
	char* data;
	UINT size;
	UINT attempts;
	ULONGLONG timeSent;
};

class SenderSocket
{
	SOCKET sock;
	sockaddr_in local;
	sockaddr_in remote;
	ULONGLONG statsStart;
	LONGLONG devRTT;
	ULONGLONG timeLastPktSent;
	UINT seqNum;
	UINT senderBase;
	Semaphore* emptySlots;
	BufferedPacket* packetBuffer;
	UINT* ackCounts;
	std::thread stats;
	std::thread net;
	bool cont;

	UINT bytesAcked;
	UINT timeouts;
	UINT fastRetrans;

	void stats_thread();
	void net_thread();

	bool retransmit(BufferedPacket& pkt);
	void fill_buff(char* pkt, PacketType type, char* src = nullptr, UINT length = 0);
	bool check_ack(char* ack, int length, PacketType type);
	int udp_cread(char* buf, bool useTimeouts);
	int udp_cwrite(char* buf, UINT length);
public:
	UINT senderWindow;
	UINT receiverWindow;
	LONGLONG estRTT;

	SenderSocket();
	~SenderSocket();
	int Open(char* hostname, int port, int window, LinkProperties* lp);
	int Send(char* buffer, UINT length);
	int Close();
};