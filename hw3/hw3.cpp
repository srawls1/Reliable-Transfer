// Spencer Rawls
// hw3.cpp

#include "stdafx.h"
#include "SenderSocket.h"
#include "Checksum.h"

int main(int argc, char* argv[])
{
	if (argc != 8)
	{
		printf("Usage: hw2.exe <hostName> <bufferSize> <senderWindow> <RTT> <lossProb1> <lossProb2> <bottleneckSpeed>\n");
		return 1;
	}

	char* hostname = argv[1];
	UINT power = atoi(argv[2]);
	UINT window = atoi(argv[3]);
	LinkProperties lp;
	lp.bufferSize = window;
	lp.RTT = atof(argv[4]);
	lp.speed = 1e6 * atof(argv[7]);
	lp.pLoss[0] = atof(argv[5]);
	lp.pLoss[1] = atof(argv[6]);

	WSADATA wsadata;
	WORD wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsadata) != 0)
	{
		printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		return 5;
	}

	printf("Main:\tsender W = %d, RTT %.3f sec, loss %g / %g, link %g Mbps\n",
		window, lp.RTT, lp.pLoss[0], lp.pLoss[1], lp.speed / 1e6);
	printf("Main:\tinitializing DWORD array with 2^%d elements... ", power);

	ULONGLONG startTime = GetTickCount64();
	UINT64 bufferSize = (UINT64)1 << power;
	DWORD* dWordBuffer = new DWORD[bufferSize];
	for (UINT64 i = 0; i < bufferSize; ++i)
	{
		dWordBuffer[i] = i;
	}
	ULONGLONG endTime = GetTickCount64();
	printf("done in %d ms\n", endTime - startTime);

	SenderSocket ss;
	startTime = GetTickCount64();
	int status = ss.Open(hostname, MAGIC_PORT, window, &lp);
	if (status != STATUS_OK)
	{
		printf("Main:\tconnect failed with status %d\n", status);
		delete[] dWordBuffer;
		return 2;
	}
	else
	{
		endTime = GetTickCount64();
		printf("Main:\tconnected to %s in %.3f sec, pkt %d bytes\n", hostname, (endTime - startTime)*.001, 12);
	}

	startTime = GetTickCount64();

	char* charBuffer = (char*)dWordBuffer;
	UINT64 charBufferSize = bufferSize << 2;

	UINT64 offset = 0;
	UINT seq = 0;
	while (offset < charBufferSize)
	{
		int bytes = min(charBufferSize - offset, MAX_PKT_SIZE - sizeof(SenderDataHeader));
		status = ss.Send(charBuffer + offset, bytes);
		offset += bytes;
		++seq;
	}

	status = ss.Close();
	if (status != STATUS_OK)
	{
		printf("Main:\tclose failed with status %d\n", status);
		delete[] dWordBuffer;
		return 4;
	}
	else
	{
		Checksum c;
		endTime = GetTickCount64();
		double timePassed = (endTime - startTime) * .001;
		printf("Main:\ttransfer finished in %.3f sec, %.2f Kbps, checksum %x\n",
			timePassed, charBufferSize / timePassed * .008, c.CRC32(charBuffer, charBufferSize));
		float idealRate = 8.0f * (MAX_PKT_SIZE - sizeof(SenderDataHeader)) / ss.estRTT * window;
		printf("Main:\testRTT %.3f, ideal rate %.2f Kbps\n", ss.estRTT * .001f, idealRate);
	}


	delete[] dWordBuffer;
	return 0;
}

