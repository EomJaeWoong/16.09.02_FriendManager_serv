#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <list>
#include <map>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "winmm.lib")

#include "Protocol.h"
#include "StreamQueue.h"
#include "NPacket.h"
#include "FriendManager.h"

//---------------------------------------------------------------------------------------
// Listen Socket
//---------------------------------------------------------------------------------------
SOCKET				listen_sock;

//-----------------------------------------------------------------------------------
// ȸ����ȣ ������ ���� NO
//-----------------------------------------------------------------------------------
UINT64				uiAccountNo;
UINT64				uiReqNo;

//-----------------------------------------------------------------------------------
// ȸ�� ���� ����
//-----------------------------------------------------------------------------------
Account				g_lAccount;
Friend				g_mFriend;
FriendReq			g_mFriendReq;
FriendReq_From		g_mFriendReqFrom;
FriendReq_To		g_mFriendReqTo;

DWORD startTime, endTime;

void main()
{
	startTime = GetTickCount();

	if (!InitServer())		return;
	if (!OnServer())		return;

	InitData();

	wprintf(L"Server On.....\n");
	while (1)
	{
		Network();
		ConnectionCheck();
	}

}

/*-------------------------------------------------------------------------------------*/
// Server �ʱ�ȭ
/*-------------------------------------------------------------------------------------*/
BOOL InitServer()
{
	int retval;

	//-----------------------------------------------------------------------------------
	// Winsock �ʱ�ȭ
	//-----------------------------------------------------------------------------------
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		wprintf(L"Winsock() Error\n");
		return FALSE;
	}

	//-----------------------------------------------------------------------------------
	// socket()
	//-----------------------------------------------------------------------------------
	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET)
	{
		wprintf(L"Socket() Error\n");
		return FALSE;
	}

	//-----------------------------------------------------------------------------------
	// bind
	//-----------------------------------------------------------------------------------
	SOCKADDR_IN sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(dfNETWORK_PORT);
	InetPton(AF_INET, L"127.0.0.1", &sockaddr.sin_addr);
	retval = bind(listen_sock, (SOCKADDR*)&sockaddr, sizeof(sockaddr));
	if (retval == SOCKET_ERROR)
	{
		wprintf(L"Bind() Error\n");
		return FALSE;
	}

	//-----------------------------------------------------------------------------------
	// ȸ�� No �ʱ�ȭ
	//-----------------------------------------------------------------------------------
	uiAccountNo = 0;
	uiReqNo		= 0;

	return TRUE;
}

/*-------------------------------------------------------------------------------------*/
// Server On
/*-------------------------------------------------------------------------------------*/
BOOL OnServer()
{
	int retval;

	//-----------------------------------------------------------------------------------
	// Listen
	//-----------------------------------------------------------------------------------
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR)
	{
		wprintf(L"Listen() Error\n");
		return FALSE;
	}

	return TRUE;
}

/*-------------------------------------------------------------------------------------*/
// ���� ��Ʈ��ũ
/*-------------------------------------------------------------------------------------*/
void Network()
{
	int retval;
	AccountIter aIter;

	list<FD_SET *> Readlist;
	list<FD_SET *> Writelist;
	int iSockCount = 0;

	FD_SET *ReadSet = new FD_SET;
	FD_SET *WriteSet = new FD_SET;

	Readlist.push_back(ReadSet);
	Writelist.push_back(WriteSet);

	FD_ZERO(ReadSet);
	FD_ZERO(WriteSet);

	FD_SET(listen_sock, ReadSet);
	iSockCount++;

	for (aIter = g_lAccount.begin(); aIter != g_lAccount.end(); ++aIter)
	{
		if ((*aIter)->sock != INVALID_SOCKET){
			if ((*aIter)->SendQ.GetUseSize() > 0)
				FD_SET((*aIter)->sock, WriteSet);

			FD_SET((*aIter)->sock, ReadSet);

			iSockCount++;
		}

		if (iSockCount > 64)
		{
			ReadSet = new FD_SET;
			WriteSet = new FD_SET;

			Readlist.push_back(ReadSet);
			Writelist.push_back(WriteSet);

			FD_ZERO(&ReadSet);
			FD_ZERO(&WriteSet);

			iSockCount = 0;
		}
	}
	
	/*
	//-----------------------------------------------------------------------------------
	// Account Socket�� ReadSet, WriteSet�� ���
	//-----------------------------------------------------------------------------------
	for (aIter = g_lAccount.begin(); aIter != g_lAccount.end(); ++aIter)
	{
		if ((*aIter)->sock != INVALID_SOCKET){
			if ((*aIter)->SendQ.GetUseSize() > 0)
				FD_SET((*aIter)->sock, &WriteSet);

			FD_SET((*aIter)->sock, &ReadSet);
		}
	}
	*/
	TIMEVAL Time;
	Time.tv_sec = 0;
	Time.tv_usec = 0;

	//-----------------------------------------------------------------------------------
	// Select
	//-----------------------------------------------------------------------------------
	list<FD_SET *>::iterator ReadIter = Readlist.begin();
	list<FD_SET *>::iterator WriteIter = Writelist.begin();

	for (int iCnt = 0; iCnt < Readlist.size(); iCnt++){
		retval = select(0, (*ReadIter), (*WriteIter), NULL, &Time);

		if (retval == 0)		return;

		else if (retval < 0)
		{
			DWORD dwError = GetLastError();
			wprintf(L"Select() Error : %d\n", dwError);
			exit(1);
		}

		else
		{
			//-------------------------------------------------------------------------------
			// Accept ó��
			//-------------------------------------------------------------------------------
			if (FD_ISSET(listen_sock, (*ReadIter)))
				AcceptClient();
			//-------------------------------------------------------------------------------
			// Socket ó��
			//-------------------------------------------------------------------------------
			else
				SocketProc((*ReadIter), (*WriteIter));
		}
	}
}

/*-------------------------------------------------------------------------------------*/
// Client Accept
/*-------------------------------------------------------------------------------------*/
void AcceptClient()
{
	int addrlen = sizeof(SOCKADDR_IN);
	WCHAR wcAddr[16];
	
	stAccount *pAccount = new stAccount;
	pAccount->uiAccountNo = 0;
	memset(pAccount->ID, 0, dfNICK_MAX_LEN * 2);

	pAccount->sock = accept(listen_sock, (SOCKADDR *)&pAccount->sockaddr, &addrlen);
	if (pAccount->sock == INVALID_SOCKET)
	{
		wprintf(L"Accept() Error\n");
		return;
	}
	
	g_lAccount.push_front(pAccount);
	
	InetNtop(AF_INET, &pAccount->sockaddr.sin_addr, wcAddr, sizeof(wcAddr));
	
	wprintf(L"Accept - %s:%d Socket : %d \n", wcAddr, ntohs(pAccount->sockaddr.sin_port), 
		pAccount->sock);
}

/*-------------------------------------------------------------------------------------*/
// Socket Set�� ���� ó��
// - Write, Read�� ���� ���ν��� ó��
/*-------------------------------------------------------------------------------------*/
void SocketProc(FD_SET *ReadSet, FD_SET *WriteSet)
{
	AccountIter aIter;

	for (aIter = g_lAccount.begin(); aIter != g_lAccount.end(); ++aIter)
	{
		if (FD_ISSET((*aIter)->sock, WriteSet))
			WriteProc((*aIter));

		if (FD_ISSET((*aIter)->sock, ReadSet))
			ReadProc((*aIter));
	}
}

/*-------------------------------------------------------------------------------------*/
// Write�� ���� ó��(send)
/*-------------------------------------------------------------------------------------*/
void WriteProc(stAccount *pAccount)
{
	int retval;
	//stAccount *pAccount = findAccount(uiAccountNo);

	//-----------------------------------------------------------------------------------
	// SendQ�� �����Ͱ� �ִ��� ��� ����
	//-----------------------------------------------------------------------------------
	while (pAccount->SendQ.GetUseSize() > 0)
	{
		retval = send(pAccount->sock, pAccount->SendQ.GetReadBufferPtr(),
			pAccount->SendQ.GetNotBrokenGetSize(), 0);
		pAccount->SendQ.RemoveData(retval);

		if (retval == 0)
			break;

		else if (retval < 0)
		{
			DisconnectClient(pAccount);
			return;
		}
	}
}

/*-------------------------------------------------------------------------------------*/
// Read�� ���� ó��(Recv)
/*-------------------------------------------------------------------------------------*/
void ReadProc(stAccount *pAccount)
{
	int retval;
	//stAccount *pAccount = findAccount(uiAccountNo);

	retval = recv(pAccount->sock, pAccount->RecvQ.GetWriteBufferPtr(),
		pAccount->RecvQ.GetNotBrokenPutSize(), 0);
	pAccount->RecvQ.MoveWritePos(retval);

	if (retval == 0)
	{
		DisconnectClient(pAccount);
		return;
	}

	//-----------------------------------------------------------------------------------
	// RecvQ�� �����Ͱ� �����ִ� �� ��� ��Ŷ ó��
	//-----------------------------------------------------------------------------------
	while (pAccount->RecvQ.GetUseSize() > 0)
	{
		if (retval < 0)
		{
			//error
			DisconnectClient(pAccount);
			return;
		}

		else
			PacketProc(uiAccountNo);
	}
}

BOOL PacketProc(UINT64 uiAccountNo)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;
	stAccount *pAccount = findAccount(uiAccountNo);

	//--------------------------------------------------------------------------------------*/
	//RecvQ �뷮�� header���� ������ �˻�
	/*--------------------------------------------------------------------------------------*/
	if (pAccount->RecvQ.GetUseSize() < sizeof(st_PACKET_HEADER))
		return FALSE;

	pAccount->RecvQ.Peek((char *)&header, sizeof(st_PACKET_HEADER));

	/*--------------------------------------------------------------------------------------*/
	//header + payload �뷮�� RecvQ�뷮���� ū�� �˻�
	/*--------------------------------------------------------------------------------------*/
	if (pAccount->RecvQ.GetUseSize() < header.wPayloadSize + sizeof(st_PACKET_HEADER))
		return FALSE;

	pAccount->RecvQ.RemoveData(sizeof(st_PACKET_HEADER));

	/*--------------------------------------------------------------------------------------*/
	//payload�� cPacket�� �̰� ������ �˻�
	/*--------------------------------------------------------------------------------------*/
	if (header.wPayloadSize != 
		pAccount->RecvQ.Get((char *)cPacket.GetBufferPtr(), header.wPayloadSize))
		return FALSE;

	cPacket.MoveWritePos(header.wPayloadSize);

	wprintf(L"PacketRecv [UserNo:%d][Type:%d]\n", uiAccountNo, header.wMsgType);

	/*--------------------------------------------------------------------------------------*/
	// CheckSum �˻�(������ ���� �����)
	/*--------------------------------------------------------------------------------------*/
	/*
	if (header.byCheckSum != MakeCheckSum(header.wMsgType, &cPacket))
		wprintf(L"CheckSum Error : %d, %d\n", header.byCheckSum, MakeCheckSum(header.wMsgType, &cPacket));
	*/

	/*--------------------------------------------------------------------------------------*/
	// Message Ÿ�Կ� ���� Packet ó��
	/*--------------------------------------------------------------------------------------*/
	switch (header.wMsgType)
	{
	case df_REQ_ACCOUNT_ADD:
		return packetProc_ReqAccountAdd(pAccount, &cPacket);
		break;

	case df_REQ_LOGIN:
		return packetProc_ReqLogin(pAccount, &cPacket);
		break;

	case df_REQ_ACCOUNT_LIST:
		return packetProc_ReqAccountList(pAccount, &cPacket);
		break;

	case df_REQ_FRIEND_LIST:
		return packetProc_ReqFriendList(pAccount, &cPacket);
		break;

	case df_REQ_FRIEND_REQUEST_LIST:
		return packetProc_ReqFriendReqList(pAccount, &cPacket);
		break;

	case df_REQ_FRIEND_REPLY_LIST:
		return packetProc_ReqFriendReplyList(pAccount, &cPacket);
		break;

	case df_REQ_FRIEND_REMOVE:
		return packetProc_ReqFriendRemove(pAccount, &cPacket);
		break;

	case df_REQ_FRIEND_REQUEST:
		return packetProc_ReqFriendReq(pAccount, &cPacket);
		break;

	case df_REQ_FRIEND_CANCEL:
		return packetProc_ReqFriendCancel(pAccount, &cPacket);
		break;

	case df_REQ_FRIEND_DENY:
		return packetProc_ReqFriendDeny(pAccount, &cPacket);
		break;

	case df_REQ_FRIEND_AGREE:
		return packetProc_ReqFriendAgree(pAccount, &cPacket);
		break;

	case df_REQ_STRESS_ECHO:
		return packetProc_ReqStressEcho(pAccount, &cPacket);
		break;

	default:
		break;
	}

	return TRUE;
}

/*-------------------------------------------------------------------------------------*/
// Client -> Server�� �� Packet ó��


//---------------------------------------------------------------------------------------
// ȸ������ ��û
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqAccountAdd(stAccount *pAccount, CNPacket *cPacket)
{
	AccountIter aIter;
	stAccount *stTargetAccount;
	WCHAR wID[dfNICK_MAX_LEN];

	cPacket->Get(wID, dfNICK_MAX_LEN * 2);
	for (aIter = g_lAccount.begin(); aIter != g_lAccount.end(); ++aIter)
	{
		//�ߺ� �г���
		if (0 == wcscmp((*aIter)->ID, wID))
			return FALSE;
			//TODO : �ź� �� ����
	}

	stTargetAccount = new stAccount;
	stTargetAccount->uiAccountNo = ++uiAccountNo;
	stTargetAccount->sock = INVALID_SOCKET;
	memcpy(stTargetAccount->ID, wID, dfNICK_MAX_LEN * 2);

	g_lAccount.push_front(stTargetAccount);

	return sendProc_ResAccountAdd(pAccount, stTargetAccount->uiAccountNo);
}

//---------------------------------------------------------------------------------------
// ȸ�� �α���
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqLogin(stAccount *pAccount, CNPacket *cPacket)
{
	AccountIter aIter;
	UINT64 uiAccountNo;

	*cPacket >> uiAccountNo;

	for (aIter = g_lAccount.begin(); aIter != g_lAccount.end(); ++aIter)
	{
		if ((*aIter)->uiAccountNo == uiAccountNo)	break;
	}

	//-----------------------------------------------------------------------------------
	// ȸ���� ������ 0
	//-----------------------------------------------------------------------------------
	if (aIter == g_lAccount.end())	uiAccountNo = 0;
	else
	{
		pAccount->uiAccountNo = uiAccountNo;
		wcscpy_s(pAccount->ID, (*aIter)->ID);

		g_lAccount.erase(aIter);
	}

	return sendProc_ResLogin(pAccount, uiAccountNo, pAccount->ID);
}

//---------------------------------------------------------------------------------------
// ȸ������Ʈ ��û
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqAccountList(stAccount *pAccount, CNPacket *cPacket)
{
	return sendProc_ResAccountList(pAccount);
}

//---------------------------------------------------------------------------------------
// ģ����� ��û
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqFriendList(stAccount *pAccount, CNPacket *cPacket)
{
	return sendProc_ResFriendList(pAccount);
}

//---------------------------------------------------------------------------------------
// ģ����û ���� ��� ��û
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqFriendReqList(stAccount *pAccount, CNPacket *cPacket)
{
	return sendProc_ResFriendReqList(pAccount);
}

//---------------------------------------------------------------------------------------
// ģ����û ������ ��� ��û
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqFriendReplyList(stAccount *pAccount, CNPacket *cPacket)
{
	return sendProc_ResFriendReplyList(pAccount);
}

//---------------------------------------------------------------------------------------
// ģ������ ����
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqFriendRemove(stAccount *pAccount, CNPacket *cPacket)
{
	pair<FriendIter, FriendIter> range;
	FriendIter fIterFrom, fIterTo;
	BOOL bFrom = FALSE, bTo = FALSE;
	stAccount *stTargetAccount;
	UINT64 uiFriendAccountNo;
	BYTE byResult = df_RESULT_FRIEND_REMOVE_OK;

	*cPacket >> uiFriendAccountNo;

	stTargetAccount = findAccount(uiFriendAccountNo);

	range = g_mFriend.equal_range(pAccount->uiAccountNo);
	for (fIterFrom = range.first; fIterFrom != range.second; ++fIterFrom)
	{
		if (fIterFrom->second->uiToNo == uiFriendAccountNo)
			break;
	}

	range = g_mFriend.equal_range(uiFriendAccountNo);
	for (fIterTo = range.first; fIterTo != range.second; ++fIterTo)
	{
		if (fIterTo->second->uiToNo == pAccount->uiAccountNo)
			break;
	}

	//ģ�����谡 ���� ���� ȸ���� ���� ��
	if (NULL == stTargetAccount)
		byResult = df_RESULT_FRIEND_REMOVE_FAIL;

	//ģ���� �ƴ� ��
	if (fIterFrom == g_mFriend.end() || fIterTo == g_mFriend.end())
		byResult = df_RESULT_FRIEND_REMOVE_NOTFRIEND;

	if (byResult = df_RESULT_FRIEND_REMOVE_OK)
		DeleteFriend(pAccount->uiAccountNo, uiFriendAccountNo);

	return sendProc_ResFriendRemove(pAccount, uiFriendAccountNo, byResult);
}

//---------------------------------------------------------------------------------------
// ģ����û
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqFriendReq(stAccount *pAccount, CNPacket *cPacket)
{
	pair<FriendReqFromIter, FriendReqFromIter> range;
	FriendReqFromIter rfIter;
	FriendReqIter rIter;
	stAccount *stTargetAccount;
	UINT64 uiFriendAccountNo;
	BYTE byResult = df_RESULT_FRIEND_REQUEST_OK;

	*cPacket >> uiFriendAccountNo;

	range = g_mFriendReqFrom.equal_range(uiFriendAccountNo);
	stTargetAccount = findAccount(uiFriendAccountNo);

	//ã�� �� ���� ��
	if (NULL == stTargetAccount)
		byResult = df_RESULT_FRIEND_REQUEST_NOTFOUND;
	//�̹� ��û �Ǿ����� ��
	for (rfIter = range.first; rfIter != range.second; ++rfIter)
	{
		rIter = g_mFriendReq.find(rfIter->second);
		if ((rIter != g_mFriendReq.end()) && 
			(rIter->second->uiToNo == stTargetAccount->uiAccountNo))
		{
			byResult = df_RESULT_FRIEND_REQUEST_AREADY;
		}
	}

	if (byResult == df_RESULT_FRIEND_REQUEST_OK)
		AddFriendReq(pAccount->uiAccountNo, uiFriendAccountNo);

	return sendProc_ResFriendReq(pAccount, uiFriendAccountNo, byResult);
}

//---------------------------------------------------------------------------------------
// ģ����û ���
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqFriendCancel(stAccount *pAccount, CNPacket *cPacket)
{
	pair<FriendReqToIter, FriendReqToIter> range;
	FriendReqToIter rtIter;
	FriendReqIter rIter;
	stAccount *stTargetAccount;
	UINT64 uiFriendAccountNo;
	BYTE byResult = df_RESULT_FRIEND_CANCEL_OK;

	*cPacket >> uiFriendAccountNo;

	range = g_mFriendReqTo.equal_range(uiFriendAccountNo);
	stTargetAccount = findAccount(uiFriendAccountNo);

	//ã�� �� ���� ��
	if (NULL == stTargetAccount)
		byResult = df_RESULT_FRIEND_CANCEL_FAIL;

	//��û�� �ٸ���
	for (rtIter = range.first; rtIter != range.second; ++rtIter)
	{
		rIter = g_mFriendReq.find(rtIter->second);
		if ((rIter == g_mFriendReq.end()) || 
			rIter->second->uiToNo != uiFriendAccountNo)
			byResult = df_RESULT_FRIEND_CANCEL_NOTFRIEND;
	}

	if (byResult == df_RESULT_FRIEND_CANCEL_OK)
		DeleteFriendReq(pAccount->uiAccountNo, uiFriendAccountNo);

	return sendProc_ResFriendCancel(pAccount, uiFriendAccountNo, byResult);
}

//---------------------------------------------------------------------------------------
// ģ����û �ź�
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqFriendDeny(stAccount *pAccount, CNPacket *cPacket)
{
	pair<FriendReqFromIter, FriendReqFromIter> range;
	FriendReqFromIter rfIter;
	FriendReqIter rIter;
	stAccount *stTargetAccount;
	UINT64 uiFriendAccountNo;
	BYTE byResult = df_RESULT_FRIEND_DENY_OK;

	*cPacket >> uiFriendAccountNo;

	range = g_mFriendReqFrom.equal_range(uiFriendAccountNo);
	stTargetAccount = findAccount(uiFriendAccountNo);

	//ã�� �� ���� ��
	if (NULL == stTargetAccount)
		byResult = df_RESULT_FRIEND_DENY_FAIL;

	//��û�� �ٸ���
	for (rfIter = range.first; rfIter != range.second; ++rfIter)
	{
		rIter = g_mFriendReq.find(rfIter->second);
		if ((rIter == g_mFriendReq.end()) ||
			rIter->second->uiFromNo != uiFriendAccountNo)
			byResult = df_RESULT_FRIEND_DENY_NOTFRIEND;
	}

	if (byResult == df_RESULT_FRIEND_CANCEL_OK)
		DeleteFriendReq(uiFriendAccountNo, pAccount->uiAccountNo);

	return sendProc_ResFriendDeny(pAccount, uiFriendAccountNo, byResult);
}

//---------------------------------------------------------------------------------------
// ģ����û ����
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqFriendAgree(stAccount *pAccount, CNPacket *cPacket)
{
	FriendReqToIter ftIter;
	FriendReqIter frIter;
	AccountIter aIter;
	UINT64 uiFriendAccountNo;
	BYTE byResult = df_RESULT_FRIEND_AGREE_OK;
	stAccount *stTargetAccount;

	*cPacket >> uiFriendAccountNo;

	stTargetAccount = findAccount(uiFriendAccountNo);

	ftIter = g_mFriendReqTo.find(pAccount->uiAccountNo);
	frIter = g_mFriendReq.find(ftIter->second);

	if (aIter == g_lAccount.end())
		byResult = df_RESULT_FRIEND_AGREE_FAIL;

	if (frIter->second->uiFromNo != uiFriendAccountNo)
		byResult = df_RESULT_FRIEND_AGREE_NOTFRIEND;

	if (byResult == df_RESULT_FRIEND_AGREE_OK)
		AddFriend(uiFriendAccountNo, pAccount->uiAccountNo);

	return sendProc_ResFriendAgree(pAccount, uiFriendAccountNo, byResult);
}
/*-------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------------*/
// Server -> Client�� Packet ����� ����

//---------------------------------------------------------------------------------------
// ȸ������ ���
//---------------------------------------------------------------------------------------
BOOL sendProc_ResAccountAdd(stAccount *pAccount, UINT64 uiAccountNo)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;

	makePacket_ResAccountAdd(&header, &cPacket, uiAccountNo);

	SendUnicast(pAccount, &header, &cPacket);

	return TRUE;
}

//---------------------------------------------------------------------------------------
// ȸ���α��� ���
//---------------------------------------------------------------------------------------
BOOL sendProc_ResLogin(stAccount *pAccount, UINT64 uiAccountNo, WCHAR* pID)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;

	makePacket_ResLogin(&header, &cPacket, uiAccountNo, pID);

	SendUnicast(pAccount, &header, &cPacket);

	return TRUE;
}

//---------------------------------------------------------------------------------------
// ȸ������Ʈ ���
//---------------------------------------------------------------------------------------
BOOL sendProc_ResAccountList(stAccount *pAccount)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;

	makePacket_ResAccountList(&header, &cPacket);

	SendUnicast(pAccount, &header, &cPacket);

	return TRUE;
}

//---------------------------------------------------------------------------------------
// ģ����û��� ���
//---------------------------------------------------------------------------------------
BOOL sendProc_ResFriendList(stAccount *pAccount)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;

	makePacket_ResFriendList(&header, &cPacket, pAccount->uiAccountNo);

	SendUnicast(pAccount, &header, &cPacket);

	return TRUE;
}

//---------------------------------------------------------------------------------------
// ģ����û ������� ���
//---------------------------------------------------------------------------------------
BOOL sendProc_ResFriendReqList(stAccount *pAccount)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;

	makePacket_ResFriendReqList(&header, &cPacket, pAccount->uiAccountNo);

	SendUnicast(pAccount, &header, &cPacket);

	return TRUE;
}

//---------------------------------------------------------------------------------------
// ģ����û ������� ���
//---------------------------------------------------------------------------------------
BOOL sendProc_ResFriendReplyList(stAccount *pAccount)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;

	makePacket_ResFriendReplyList(&header, &cPacket, pAccount->uiAccountNo);

	SendUnicast(pAccount, &header, &cPacket);

	return TRUE;
}

//---------------------------------------------------------------------------------------
// ģ������ ���� ���
//---------------------------------------------------------------------------------------
BOOL sendProc_ResFriendRemove(stAccount *pAccount, UINT64 uiFriendAccountNo,
	BYTE byResult)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;

	makePacket_ResFriendRemove(&header, &cPacket, uiFriendAccountNo, byResult);

	SendUnicast(pAccount, &header, &cPacket);

	return TRUE;
}

//---------------------------------------------------------------------------------------
// ģ����û ���
//---------------------------------------------------------------------------------------
BOOL sendProc_ResFriendReq(stAccount *pAccount, UINT64 uiFriendAccountNo, BYTE byResult)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;

	makePacket_ResFriendReq(&header, &cPacket, uiFriendAccountNo, byResult);

	SendUnicast(pAccount, &header, &cPacket);

	return TRUE;
}

//---------------------------------------------------------------------------------------
// ģ����û ��� ���
//---------------------------------------------------------------------------------------
BOOL sendProc_ResFriendCancel(stAccount *pAccount, UINT64 uiFriendAccountNo,
	BYTE byResult)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;

	makePacket_ResFriendCancel(&header, &cPacket, uiFriendAccountNo, byResult);

	SendUnicast(pAccount, &header, &cPacket);

	return TRUE;
}

//---------------------------------------------------------------------------------------
// ģ����û �ź� ���
//---------------------------------------------------------------------------------------
BOOL sendProc_ResFriendDeny(stAccount *pAccount, UINT64 uiFriendAccountNo, BYTE byResult)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;

	makePacket_ResFriendDeny(&header, &cPacket, uiFriendAccountNo, byResult);

	SendUnicast(pAccount, &header, &cPacket);

	return TRUE;
}

//---------------------------------------------------------------------------------------
// ģ����û ���� ���
//---------------------------------------------------------------------------------------
BOOL sendProc_ResFriendAgree(stAccount *pAccount, UINT64 uiFriendAccountNo,
	BYTE byResult)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;

	makePacket_ResFriendAgree(&header, &cPacket, uiFriendAccountNo, byResult);

	SendUnicast(pAccount, &header, &cPacket);

	return TRUE;
}
/*-------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------------*/
// Packet ���� �Լ�

//---------------------------------------------------------------------------------------
// ȸ������ ��� Packet ����
//---------------------------------------------------------------------------------------
void makePacket_ResAccountAdd(st_PACKET_HEADER *header, CNPacket *cPacket, 
	UINT64 uiAccountNo)
{
	*cPacket << uiAccountNo;

	header->byCode = dfPACKET_CODE;
	header->wMsgType = df_RES_ACCOUNT_ADD;
	header->wPayloadSize = cPacket->GetDataSize();
}

//---------------------------------------------------------------------------------------
// ȸ���α��� ��� Packet ����
//---------------------------------------------------------------------------------------
void makePacket_ResLogin(st_PACKET_HEADER *header, CNPacket *cPacket,
	UINT64 uiAccountNo, WCHAR *pID)
{
	*cPacket << uiAccountNo;

	if (uiAccountNo != 0)	cPacket->Put(pID, dfNICK_MAX_LEN);

	header->byCode = dfPACKET_CODE;
	header->wMsgType = df_RES_LOGIN;
	header->wPayloadSize = cPacket->GetDataSize();
}

//---------------------------------------------------------------------------------------
// ȸ������Ʈ ��� Packet ����
//---------------------------------------------------------------------------------------
void makePacket_ResAccountList(st_PACKET_HEADER *header, CNPacket *cPacket)
{
	AccountIter aIter;
	int size = g_lAccount.size();

	//-----------------------------------------------------------------------------------
	// ȸ�� �� ���
	//-----------------------------------------------------------------------------------
	for (aIter = g_lAccount.begin(); aIter != g_lAccount.end(); ++aIter)
	{
		if (0 == wcscmp((*aIter)->ID, L""))
			size--;
	}

	*cPacket << (UINT)size;
	
	//-----------------------------------------------------------------------------------
	// ȸ�� ���� ����
	//-----------------------------------------------------------------------------------
	for (aIter = g_lAccount.begin(); aIter != g_lAccount.end(); ++aIter)
	{
		if (0 != wcscmp((*aIter)->ID, L""))
		{
			*cPacket << (*aIter)->uiAccountNo;
			cPacket->Put((*aIter)->ID, dfNICK_MAX_LEN);
		}
	}

	header->byCode = dfPACKET_CODE;
	header->wMsgType = df_RES_ACCOUNT_LIST;
	header->wPayloadSize = cPacket->GetDataSize();
}

//---------------------------------------------------------------------------------------
// ģ����û��� ��� Packet ����
//---------------------------------------------------------------------------------------
void makePacket_ResFriendList(st_PACKET_HEADER *header, CNPacket *cPacket,
	UINT64 uiAccountNo)
{
	FriendIter fIter, fIterTo;
	stAccount *stTargetAccount;
	UINT iCount = 0;

	for (fIter = g_mFriend.begin(); fIter != g_mFriend.end(); ++fIter)
	{
		if (fIter->second->uiFromNo == uiAccountNo)
		{
			fIterTo = g_mFriend.find(fIter->second->uiToNo);
			if (fIterTo != g_mFriend.end() && fIterTo->second->uiToNo == uiAccountNo)
				iCount++;
		}
	}

	*cPacket << iCount;

	for (fIter = g_mFriend.begin(); fIter != g_mFriend.end(); ++fIter)
	{
		if (fIter->second->uiFromNo == uiAccountNo)
		{
			fIterTo = g_mFriend.find(fIter->second->uiToNo);
			if (fIterTo != g_mFriend.end() && fIterTo->second->uiToNo == uiAccountNo)
			{
				*cPacket << fIter->second->uiToNo;
				stTargetAccount = findAccount(fIter->second->uiToNo);
				cPacket->Put(stTargetAccount->ID, dfNICK_MAX_LEN);
			}
		}
	}

	header->byCode = dfPACKET_CODE;
	header->wMsgType = df_RES_FRIEND_LIST;
	header->wPayloadSize = cPacket->GetDataSize();
}

//---------------------------------------------------------------------------------------
// ģ����û ������� ��� Packet ����
//---------------------------------------------------------------------------------------
void makePacket_ResFriendReqList(st_PACKET_HEADER *header, CNPacket *cPacket,
	UINT64 uiAccountNo)
{
	pair<FriendReqFromIter, FriendReqFromIter> range;
	FriendReqFromIter ffIter;
	FriendReqIter rIter;
	stAccount *stTargetAccount;

	*cPacket << (UINT)g_mFriendReqFrom.count(uiAccountNo);
	range = g_mFriendReqFrom.equal_range(uiAccountNo);

	for (ffIter = range.first; ffIter != range.second; ffIter++)
	{
		rIter = g_mFriendReq.find(ffIter->second);
		*cPacket << rIter->second->uiToNo;

		stTargetAccount = findAccount(rIter->second->uiToNo);
		cPacket->Put(stTargetAccount->ID, dfNICK_MAX_LEN);
	}

	header->byCode = dfPACKET_CODE;
	header->wMsgType = df_RES_FRIEND_REQUEST_LIST;
	header->wPayloadSize = cPacket->GetDataSize();
}

//---------------------------------------------------------------------------------------
// ģ����û ������� ��� Packet ����
//---------------------------------------------------------------------------------------
void makePacket_ResFriendReplyList(st_PACKET_HEADER *header, CNPacket *cPacket, 
	UINT64 uiAccountNo)
{
	pair<FriendReqToIter, FriendReqToIter> range;
	FriendReqToIter ftIter;
	FriendReqIter rIter;
	stAccount *stTargetAccount;

	*cPacket << (UINT)g_mFriendReqTo.count(uiAccountNo);
	range = g_mFriendReqTo.equal_range(uiAccountNo);

	for (ftIter = range.first; ftIter != range.second; ftIter++)
	{
		rIter = g_mFriendReq.find(ftIter->second);
		*cPacket << rIter->second->uiFromNo;

		stTargetAccount = findAccount(rIter->second->uiFromNo);
		cPacket->Put(stTargetAccount->ID, dfNICK_MAX_LEN);
	}

	header->byCode = dfPACKET_CODE;
	header->wMsgType = df_RES_FRIEND_REPLY_LIST;
	header->wPayloadSize = cPacket->GetDataSize();
}

//---------------------------------------------------------------------------------------
// ģ������ ���� ��� Packet ����
//---------------------------------------------------------------------------------------
void makePacket_ResFriendRemove(st_PACKET_HEADER *header, CNPacket *cPacket, 
	UINT64 uiFriendAccountNo, BYTE byResult)
{
	*cPacket << uiFriendAccountNo;
	*cPacket << byResult;

	header->byCode = dfPACKET_CODE;
	header->wMsgType = df_RES_FRIEND_REMOVE;
	header->wPayloadSize = cPacket->GetDataSize();
}

//---------------------------------------------------------------------------------------
// ģ����û ��� Packet ����
//---------------------------------------------------------------------------------------
void makePacket_ResFriendReq(st_PACKET_HEADER *header, CNPacket *cPacket, 
	UINT64 uiFriendAccountNo, BYTE byResult)
{
	*cPacket << uiFriendAccountNo;
	*cPacket << byResult;

	header->byCode = dfPACKET_CODE;
	header->wMsgType = df_RES_FRIEND_REQUEST;
	header->wPayloadSize = cPacket->GetDataSize();
}

//---------------------------------------------------------------------------------------
// ģ����û ��� ��� Packet ����
//---------------------------------------------------------------------------------------
void makePacket_ResFriendCancel(st_PACKET_HEADER *header, CNPacket *cPacket,
	UINT64 uiFriendAccountNo, BYTE byResult)
{
	*cPacket << uiFriendAccountNo;
	*cPacket << byResult;

	header->byCode = dfPACKET_CODE;
	header->wMsgType = df_RES_FRIEND_CANCEL;
	header->wPayloadSize = cPacket->GetDataSize();
}

//---------------------------------------------------------------------------------------
// ģ����û �ź� ��� Packet ����
//---------------------------------------------------------------------------------------
void makePacket_ResFriendDeny(st_PACKET_HEADER *header, CNPacket *cPacket,
	UINT64 uiFriendAccountNo, BYTE byResult)
{
	*cPacket << uiFriendAccountNo;
	*cPacket << byResult;

	header->byCode = dfPACKET_CODE;
	header->wMsgType = df_RES_FRIEND_DENY;
	header->wPayloadSize = cPacket->GetDataSize();
}

//---------------------------------------------------------------------------------------
// ģ����û ���� ��� Packet ����
//---------------------------------------------------------------------------------------
void makePacket_ResFriendAgree(st_PACKET_HEADER *header, CNPacket *cPacket,
	UINT64 uiFriendAccountNo, BYTE byResult)
{
	*cPacket << uiFriendAccountNo;
	*cPacket << byResult;

	header->byCode = dfPACKET_CODE;
	header->wMsgType = df_RES_FRIEND_AGREE;
	header->wPayloadSize = cPacket->GetDataSize();
}
/*-------------------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------------------*/
// Stress test�� Packetó��

//---------------------------------------------------------------------------------------
// Client -> Server
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqStressEcho(stAccount *pAccount, CNPacket *cPacket)
{
	short wSize;
	WCHAR *wText;

	*cPacket >> wSize;

	wText = new WCHAR[wSize];
	cPacket->Get(wText, wSize);

	return sendProc_ResStressEcho(pAccount, wSize, wText);
}

//---------------------------------------------------------------------------------------
// Server -> Client
//---------------------------------------------------------------------------------------
BOOL sendProc_ResStressEcho(stAccount *pAccount, WORD wSize, WCHAR *wText)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;

	makePacket_ResStressEcho(&header, &cPacket, wSize, wText);

	SendUnicast(pAccount, &header, &cPacket);

	return TRUE;
}

//---------------------------------------------------------------------------------------
// Stress Test�� Packet ����
//---------------------------------------------------------------------------------------
void makePacket_ResStressEcho(st_PACKET_HEADER *header, CNPacket *cPacket, 
	WORD wSize, WCHAR *wText)
{
	*cPacket << wSize;
	*cPacket << wText;

	header->byCode = dfPACKET_CODE;
	header->wMsgType = df_RES_STRESS_ECHO;
	header->wPayloadSize = cPacket->GetDataSize();
}
/*-------------------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------------------*/
// 1 : 1 Send
/*-------------------------------------------------------------------------------------*/
void SendUnicast(stAccount *pClient, st_PACKET_HEADER *header, CNPacket *cPacket)
{
	pClient->SendQ.Put((char *)header, sizeof(st_PACKET_HEADER));
	pClient->SendQ.Put((char *)cPacket->GetBufferPtr(), cPacket->GetDataSize());
}

/*-------------------------------------------------------------------------------------*/
// ���� ����
/*-------------------------------------------------------------------------------------*/
void DisconnectClient(stAccount *stTargetAccount)
{
	WCHAR wcAddr[16];

	InetNtop(AF_INET, &stTargetAccount->sockaddr.sin_addr, wcAddr, sizeof(wcAddr));
	wprintf(L"Disconnect - %s:%d [UserNo:%d][Socket:%d]\n", wcAddr,
		ntohs(stTargetAccount->sockaddr.sin_port), stTargetAccount->uiAccountNo,
		stTargetAccount->sock);

	shutdown(stTargetAccount->sock, SD_BOTH);

	stTargetAccount->sock = INVALID_SOCKET;
	stTargetAccount->RecvQ.ClearBuffer();
	stTargetAccount->SendQ.ClearBuffer();
	memset(&stTargetAccount->sockaddr, 0, sizeof(SOCKADDR_IN));

	if (stTargetAccount == df_NO_LOGIN)
	{
		AccountIter aIter;

		for (aIter = g_lAccount.begin(); aIter != g_lAccount.end(); ++aIter)
		{
			if ((*aIter) == stTargetAccount)
				break;
		}

		delete stTargetAccount;
		g_lAccount.erase(aIter);
	}
}

/*-------------------------------------------------------------------------------------*/
// ȸ�� ã��
/*-------------------------------------------------------------------------------------*/
stAccount *findAccount(UINT64 uiAccountNo)
{
	AccountIter aIter;

	for (aIter = g_lAccount.begin(); aIter != g_lAccount.end(); ++aIter)
	{
		if ((*aIter)->uiAccountNo == uiAccountNo)	break;
	}

	if (aIter == g_lAccount.end())	return NULL;
	else							return (*aIter);
}

/*-------------------------------------------------------------------------------------*/
// ģ����û ��Ͽ� �߰�
/*-------------------------------------------------------------------------------------*/
void AddFriendReq(UINT64 uiFrom, UINT64 uiTo)
{
	stFRIEND_REQ *pFriendReq = new stFRIEND_REQ;
	pFriendReq->uiNo	 = ++uiReqNo;
	pFriendReq->uiFromNo = uiFrom;
	pFriendReq->uiToNo	 = uiTo;
	pFriendReq->Time = time(NULL);

	g_mFriendReq.insert(pair<UINT64, stFRIEND_REQ *>(uiReqNo, pFriendReq));

	g_mFriendReqFrom.insert(pair<UINT64, UINT64>(uiFrom, uiReqNo));
	g_mFriendReqTo.insert(pair<UINT64, UINT64>(uiTo, uiReqNo));
}

/*-------------------------------------------------------------------------------------*/
// ģ����û ��Ͽ��� ����
/*-------------------------------------------------------------------------------------*/
void DeleteFriendReq(UINT64 uiFrom, UINT64 uiTo)
{
	FriendReqFromIter rfIter;
	FriendReqToIter rtIter;
	FriendReqIter rIter;

	for (rIter = g_mFriendReq.begin(); rIter != g_mFriendReq.end(); ++rIter)
	{
		if (rIter->second->uiFromNo == uiFrom && rIter->second->uiToNo == uiTo)
			break;
	}

	for (rfIter = g_mFriendReqFrom.begin(); rfIter != g_mFriendReqFrom.end(); ++rfIter)
	{
		if ((rfIter->first == uiFrom) && (rfIter->second == rIter->second->uiNo))
		{
			g_mFriendReqFrom.erase(rfIter);
			break;
		}
	}

	for (rtIter = g_mFriendReqTo.begin(); rtIter != g_mFriendReqTo.end(); ++rtIter)
	{
		if ((rtIter->first == uiTo) && (rtIter->second == rIter->second->uiNo))
		{
			g_mFriendReqTo.erase(rtIter);
			break;
		}
	}

	delete rIter->second;
	g_mFriendReq.erase(rIter);
}

/*-------------------------------------------------------------------------------------*/
// ģ�� ��Ͽ� �߰�
/*-------------------------------------------------------------------------------------*/
void AddFriend(UINT64 uiFrom, UINT64 uiTo)
{
	stFRIEND *pFriendA = new stFRIEND;
	stFRIEND *pFriendB = new stFRIEND;

	pFriendA->uiFromNo = uiFrom;
	pFriendA->uiToNo = uiTo;
	
	g_mFriend.insert(pair<UINT64, stFRIEND *>(uiFrom, pFriendA));

	pFriendB->uiFromNo = uiTo;
	pFriendB->uiToNo = uiFrom;
	
	g_mFriend.insert(pair<UINT64, stFRIEND *>(uiTo, pFriendB));
}

/*-------------------------------------------------------------------------------------*/
// ģ�� ��Ͽ��� ����
/*-------------------------------------------------------------------------------------*/
void DeleteFriend(UINT64 uiFrom, UINT64 uiTo)
{
	FriendIter fIterFrom, fIterTo;

	for (fIterFrom = g_mFriend.begin(); fIterFrom != g_mFriend.end(); ++fIterFrom)
	{
		if ((fIterFrom->second->uiFromNo == uiFrom) && (fIterFrom->second->uiToNo == uiTo))
			break;
	}

	for (fIterTo = g_mFriend.begin(); fIterTo != g_mFriend.end(); ++fIterTo)
	{
		if ((fIterTo->second->uiFromNo == uiTo) && (fIterTo->second->uiToNo == uiFrom))
			break;
	}

	if (fIterFrom != g_mFriend.end() && fIterTo != g_mFriend.end())
	{
		delete fIterFrom->second;
		delete fIterTo->second;
		g_mFriend.erase(fIterFrom);
		g_mFriend.erase(fIterTo);
	}
}

/*-------------------------------------------------------------------------------------*/
// ������ �ʱ�ȭ
/*-------------------------------------------------------------------------------------*/
void InitData()
{
	WCHAR wName[dfNICK_MAX_LEN];
	WCHAR wNumber[2];

	for (int iCnt = 0; iCnt < 10; iCnt++)
	{
		stAccount * pAccount = new stAccount;
		pAccount->uiAccountNo = ++uiAccountNo;
		pAccount->sock = INVALID_SOCKET;
		memset(pAccount->ID, 0, dfNICK_MAX_LEN * 2);
		
		memset(wName, 0, dfNICK_MAX_LEN * 2);
		wcscat_s(wName, L"�׽�Ʈ����");
		wsprintf(wNumber, L"%d", uiAccountNo);
		wcscat_s(wName, wNumber);
		wcscpy_s(pAccount->ID, sizeof(wName), wName);

		g_lAccount.push_back(pAccount);
	}
}

/*-------------------------------------------------------------------------------------*/
// ������ �� üũ
/*-------------------------------------------------------------------------------------*/
void ConnectionCheck()
{
	AccountIter aIter;
	endTime = GetTickCount();
	int iSize = g_lAccount.size();

	for (aIter = g_lAccount.begin(); aIter != g_lAccount.end(); ++aIter)
	{
		if ((*aIter)->sock == INVALID_SOCKET)
			--iSize;
	}

	if ((endTime - startTime) / 1000 > 0)
	{
		wprintf(L"Connection : %d\n", iSize);
		startTime = GetTickCount();
	}
}