// Spencer Rawls
// SenderSocket.cpp

#include "stdafx.h"
#include "SenderSocket.h"

// stats_thread
////////////////////////////
void SenderSocket::stats_thread()
{
	statsStart = GetTickCount64();
	while (cont)
	{
		Sleep(2000);
		double timePassed = (GetTickCount64() - statsStart) * .001;
		double megaBytes = (double)bytesAcked / 1e6;
		double speed = megaBytes * 8 / timePassed;
		printf("[%.2f] B %6d (%4.1f MB) N %6d T %d F %d W %d s %.3f Mbps RTT %.3f\n",
			timePassed, senderBase, megaBytes, seqNum, timeouts, fastRetrans,
			receiverWindow, speed, estRTT * .001);
	}
}

// retransmit
////////////////////////////
bool SenderSocket::retransmit(BufferedPacket& pkt)
{
	if (pkt.attempts >= MAX_RETX)
	{
		return false;
	}
	udp_cwrite(pkt.data, pkt.size);
	++pkt.attempts;
	pkt.timeSent = GetTickCount64();
	
	return true;
}

// net_thread
////////////////////////////
void SenderSocket::net_thread()
{
	while (cont || senderBase != seqNum)
	{
		char ack[sizeof(ReceiverHeader)];
		int status = udp_cread(ack, false);
		while (status > 0)
		{
			if (check_ack(ack, status, type_data))
			{
				if (ackCounts[senderBase % senderWindow] == 3)
				{
					ackCounts[senderBase % senderWindow] = -(int)(seqNum - senderBase);
					BufferedPacket& pkt = packetBuffer[senderBase % senderWindow];
					if (!retransmit(pkt))
					{
						printf("Max retx exceeded on sequence number %d, closing socket\n", senderBase);
						Close();
						break;
					}
					++fastRetrans;
				}
			}
			status = udp_cread(ack, false);
		}

		ULONGLONG timeout = min(max(estRTT + 4 * devRTT, MIN_RTO), MAX_RTO);
		UINT maxSendablePkt = senderBase + min(senderWindow, receiverWindow);
		for (int i = senderBase; i < maxSendablePkt; ++i)
		{
			BufferedPacket& pkt = packetBuffer[i % senderWindow];
			if (pkt.status == status_free) {
				break;
			}
			if (pkt.status == status_ready)
			{
				udp_cwrite(pkt.data, pkt.size);
				pkt.attempts = 1;
				pkt.status = status_sent;
				pkt.timeSent = GetTickCount64();
			}
			else if (pkt.status == status_sent && GetTickCount64() >= timeLastPktSent + timeout)
			{
				if (!retransmit(pkt))
				{
					printf("Max retx exceeded on sequence number %d, closing socket\n", i);
					Close();
					break;
				}
				++timeouts;
			}
		}
	}
}

// udp_cwrite
////////////////////////////
int SenderSocket::udp_cwrite(char* buf, UINT length)
{
	int status = sendto(sock, buf, length, 0, (sockaddr*)&remote, sizeof(remote));
	if (status == SOCKET_ERROR) return FAILED_SEND;
	timeLastPktSent = GetTickCount64();
	return STATUS_OK;
}

// udp_cread
////////////////////////////
int SenderSocket::udp_cread(char* buf, bool useTimeouts)
{
	timeval tv;
	ULONGLONG timeout = min(max(estRTT + 4 * devRTT, MIN_RTO), MAX_RTO);
	tv.tv_sec = useTimeouts ? (timeout / 1000) : 0;
	tv.tv_usec = useTimeouts ? (timeout % 1000) * 1000 : 10;

	fd_set fd;
	FD_ZERO(&fd);
	FD_SET(sock, &fd);
	int available = select(0, &fd, NULL, NULL, &tv);
	if (available > 0)
	{
		sockaddr_in response;
		int sizeofSockaddr = sizeof(sockaddr);
		int length = recvfrom(sock, buf, MAX_PKT_SIZE, 0, (sockaddr*)&response, &sizeofSockaddr);
		if (length == SOCKET_ERROR) return -1;
		if (response.sin_port != remote.sin_port || response.sin_addr.s_addr != remote.sin_addr.s_addr) return -2;
		return length;
	}
	return -2;
}

// check_ack
////////////////////////////
bool SenderSocket::check_ack(char* ack, int length, PacketType type)
{
	ReceiverHeader* header = (ReceiverHeader*)ack;
	if (length < sizeof(ReceiverHeader)) return false;
	if (header->ackSeq > seqNum) return false;
	if (header->ackSeq < senderBase) return false;
	Flags& f = header->flags;
	if (f.ACK != 1) return false;
	if (f.magic != MAGIC_PROTOCOL) return false;
	if (type == type_syn && f.SYN != 1) return false;
	if (type == type_fin && f.FIN != 1) return false;
	if (type == type_fin)
	{
		double timePassed = (GetTickCount64() - statsStart) * .001;
		printf("[%.2f] <-- FIN-ACK %d window %x\n",
			timePassed, header->ackSeq, header->recvWnd);
	}
	else
	{
		receiverWindow = header->recvWnd;
	}
	if (type == type_data)
	{
		UINT prevBase = senderBase;
		senderBase = header->ackSeq;
		LONGLONG sampleRTT = estRTT;
		for (UINT i = prevBase; i < senderBase; ++i)
		{
			BufferedPacket& pkt = packetBuffer[i % senderWindow];
			delete[] pkt.data;
			pkt.status = status_free;
			ackCounts[i % senderWindow] = 0;
			bytesAcked += pkt.size - sizeof(SenderDataHeader);
			emptySlots->Release();
			sampleRTT = GetTickCount64() - pkt.timeSent;
		}
		++ackCounts[senderBase % senderWindow];
		estRTT = .875 * estRTT + .125 * sampleRTT;
		devRTT = .75 * devRTT + .25 * abs(sampleRTT - estRTT);
	}
	return true;
}

// fill_buff
////////////////////////////
void SenderSocket::fill_buff(char* pkt, PacketType type, char* src, UINT length)
{
	SenderDataHeader* dataHeader = (SenderDataHeader*)pkt;
	dataHeader->seq = seqNum;
	Flags* flags = &(dataHeader->flags);
	flags->reserved = 0;
	flags->SYN = (type == type_syn) ? 1 : 0;
	flags->ACK = 0;
	flags->FIN = (type == type_fin) ? 1 : 0;
	flags->magic = MAGIC_PROTOCOL;
	char* payload = pkt + sizeof(SenderDataHeader);
	memcpy(payload, src, length);
}

// Constructor
////////////////////////////
SenderSocket::SenderSocket()
{
	senderWindow = 0;
	seqNum = 0;
	estRTT = 1000;
	devRTT = 0;
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET)
	{
		printf("socket initialization error: %d\n", WSAGetLastError());
		exit(-1);
	}

	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons(0);
	if (bind(sock, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR)
	{
		printf("bind error: %d\n", WSAGetLastError());
		exit(-1);
	}
}

// Destructor
////////////////////////////////
SenderSocket::~SenderSocket()
{
	if (senderWindow > 0) Close();
	closesocket(sock);
}

// Open
////////////////////////////////
int SenderSocket::Open(char* hostname, int port, int window, LinkProperties* lp)
{
	if (senderWindow > 0) return ALREADY_CONNECTED;
	in_addr addr;
	int remoteIp = inet_addr(hostname);
	if (remoteIp == INADDR_NONE)
	{
		hostent* host = gethostbyname(hostname);
		if (host == NULL) return INVALID_NAME;
		addr = *(in_addr*)host->h_addr;
	}
	else
	{
		addr.s_addr = remoteIp;
	}

	remote.sin_family = AF_INET;
	remote.sin_port = htons(port);
	remote.sin_addr.s_addr = addr.s_addr;

	packetBuffer = new BufferedPacket[window];
	ackCounts = new UINT[window];
	for (int i = 0; i < window; ++i)
	{
		packetBuffer[i].status = status_free;
		ackCounts[i] = 0;
	}

	senderWindow = window;
	cont = true;
	emptySlots = new Semaphore(window);

	char syn[sizeof(SenderSynHeader)];
	char ack[sizeof(ReceiverHeader)];

	fill_buff(syn, type_syn, (char*)lp, sizeof(LinkProperties));
	int tries, status;
	ULONGLONG startTime = GetTickCount64();
	for (tries = 0; tries < MAX_SYN; ++tries, estRTT *= 2)
	{
		status = udp_cwrite(syn, sizeof(SenderSynHeader));
		if (status == FAILED_SEND) return FAILED_SEND;
		status = udp_cread(ack, true);
		if (status == -2) continue;
		if (status == -1) return FAILED_RECV;
		if (check_ack(ack, status, type_syn)) break;
	}
	if (status < 0) return TIMEOUT;

	ULONGLONG endTime = GetTickCount64();
	estRTT = 3 * (endTime - startTime);

	stats = std::thread(&SenderSocket::stats_thread, this);
	net = std::thread(&SenderSocket::net_thread, this);

	return STATUS_OK;
}

// Send
//////////////////////////////////
int SenderSocket::Send(char* buffer, UINT length)
{
	if (senderWindow == 0) return NOT_CONNECTED;
	char* mes = new char[length + sizeof(SenderDataHeader)];
	fill_buff(mes, type_data, buffer, length);
	emptySlots->Grab();
	packetBuffer[seqNum % senderWindow].data = mes;
	packetBuffer[seqNum % senderWindow].size = length + sizeof(SenderDataHeader);
	packetBuffer[seqNum % senderWindow].status = status_ready;
	++seqNum;
	return STATUS_OK;
}

// Close
//////////////////////////////////
int SenderSocket::Close()
{
	if (senderWindow == 0) return NOT_CONNECTED;
	
	cont = false;

	net.join();
	stats.join();
	senderWindow = 0;
	delete[] packetBuffer;
	delete[] ackCounts;
	delete emptySlots;

	char fin[sizeof(SenderDataHeader)];
	char ack[sizeof(ReceiverHeader)];

	fill_buff(fin, type_fin);
	int tries, status;
	for (tries = 0; tries < MAX_SYN; ++tries, estRTT *= 2)
	{
		status = udp_cwrite(fin, sizeof(SenderDataHeader));
		if (status == FAILED_SEND) return FAILED_SEND;
		status = udp_cread(ack, true);
		if (status == -2) continue;
		if (status == -1) return FAILED_RECV;
		if (check_ack(ack, status, type_fin)) break;
	}
	if (status < 0) return TIMEOUT;
	return STATUS_OK;
}