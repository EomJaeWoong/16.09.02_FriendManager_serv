#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
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
// 회원번호 증가를 위한 NO
//-----------------------------------------------------------------------------------
UINT64				uiAccountNo;
UINT64				uiFriendNo;
UINT64				uiReqNo;

//-----------------------------------------------------------------------------------
// 회원 관리 변수
//-----------------------------------------------------------------------------------
Account				g_mAccount;
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
// Server 초기화
/*-------------------------------------------------------------------------------------*/
BOOL InitServer()
{
	int retval;

	//-----------------------------------------------------------------------------------
	// Winsock 초기화
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
	// 회원 No 초기화
	//-----------------------------------------------------------------------------------
	uiAccountNo = 0;
	uiReqNo		= 0;
	uiFriendNo  = 0;

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
// 메인 네트워크
/*-------------------------------------------------------------------------------------*/
void Network()
{
	int retval;
	AccountIter aIter;
	FD_SET ReadSet, WriteSet;

	FD_ZERO(&ReadSet);
	FD_ZERO(&WriteSet);

	FD_SET(listen_sock, &ReadSet);

	//-----------------------------------------------------------------------------------
	// Account Socket을 ReadSet, WriteSet에 등록
	//-----------------------------------------------------------------------------------
	for (aIter = g_mAccount.begin(); aIter != g_mAccount.end(); ++aIter)
	{
		if (aIter->second->sock != INVALID_SOCKET){
			if (aIter->second->SendQ.GetUseSize() > 0)
				FD_SET(aIter->second->sock, &WriteSet);

			FD_SET(aIter->second->sock, &ReadSet);
		}
	}

	TIMEVAL Time;
	Time.tv_sec = 0;
	Time.tv_usec = 0;

	//-----------------------------------------------------------------------------------
	// Select
	//-----------------------------------------------------------------------------------
	retval = select(0, &ReadSet, &WriteSet, NULL, &Time);

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
		// Accept 처리
		//-------------------------------------------------------------------------------
		if (FD_ISSET(listen_sock, &ReadSet))
			AcceptClient();
		//-------------------------------------------------------------------------------
		// Socket 처리
		//-------------------------------------------------------------------------------
		else
			SocketProc(ReadSet, WriteSet);
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
	
	g_mAccount.insert(pair<UINT64, stAccount *>(pAccount->uiAccountNo, pAccount));
	
	InetNtop(AF_INET, &pAccount->sockaddr.sin_addr, wcAddr, sizeof(wcAddr));
	
	wprintf(L"Accept - %s:%d Socket : %d \n", wcAddr, ntohs(pAccount->sockaddr.sin_port), 
		pAccount->sock);
}

/*-------------------------------------------------------------------------------------*/
// Socket Set에 대한 처리
// - Write, Read에 대한 프로시저 처리
/*-------------------------------------------------------------------------------------*/
void SocketProc(FD_SET ReadSet, FD_SET WriteSet)
{
	AccountIter iter;

	for (iter = g_mAccount.begin(); iter != g_mAccount.end(); ++iter)
	{
		if (FD_ISSET(iter->second->sock, &WriteSet))
			WriteProc(iter->first);

		if (FD_ISSET(iter->second->sock, &ReadSet))
			ReadProc(iter->first);
	}
}

/*-------------------------------------------------------------------------------------*/
// Write에 관한 처리(send)
/*-------------------------------------------------------------------------------------*/
void WriteProc(UINT64 uiAccountNo)
{
	int retval;
	stAccount *pAccount = findAccount(uiAccountNo);

	//-----------------------------------------------------------------------------------
	// SendQ에 데이터가 있는한 계속 보냄
	//-----------------------------------------------------------------------------------
	while (pAccount->SendQ.GetUseSize() > 0)
	{
		retval = send(pAccount->sock, pAccount->SendQ.GetReadBufferPtr(),
			pAccount->SendQ.GetNotBrokenGetSize(), 0);
		pAccount->SendQ.RemoveData(retval);

		if (retval == 0)
			break;

		else if (retval < 0){
			DWORD dwError = GetLastError();
			wprintf(L"Send() Error [AccountNo : %d] : %d", pAccount->uiAccountNo, dwError);
			//TODO : Disconnect Client
			return;
		}
	}
}

/*-------------------------------------------------------------------------------------*/
// Read에 관란 처리(Recv)
/*-------------------------------------------------------------------------------------*/
void ReadProc(UINT64 uiAccountNo)
{
	int retval;
	stAccount *pAccount = findAccount(uiAccountNo);

	retval = recv(pAccount->sock, pAccount->RecvQ.GetWriteBufferPtr(),
		pAccount->RecvQ.GetNotBrokenPutSize(), 0);
	pAccount->RecvQ.MoveWritePos(retval);

	//-----------------------------------------------------------------------------------
	// RecvQ에 데이터가 남아있는 한 계속 패킷 처리
	//-----------------------------------------------------------------------------------
	while (pAccount->RecvQ.GetUseSize() > 0)
	{
		if (retval == 0)
			return;

		else if (retval < 0)
		{
			DWORD dwError = GetLastError();
			if (dwError == WSAECONNRESET)
			{
				DisconnectClient(uiAccountNo);
			}
			wprintf(L"Recv() Error [AccountNo : %d] : %d\n", 
				pAccount->uiAccountNo, dwError);
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
	//RecvQ 용량이 header보다 작은지 검사
	/*--------------------------------------------------------------------------------------*/
	if (pAccount->RecvQ.GetUseSize() < sizeof(st_PACKET_HEADER))
		return FALSE;

	pAccount->RecvQ.Peek((char *)&header, sizeof(st_PACKET_HEADER));

	/*--------------------------------------------------------------------------------------*/
	//header + payload 용량이 RecvQ용량보다 큰지 검사
	/*--------------------------------------------------------------------------------------*/
	if (pAccount->RecvQ.GetUseSize() < header.wPayloadSize + sizeof(st_PACKET_HEADER))
		return FALSE;

	pAccount->RecvQ.RemoveData(sizeof(st_PACKET_HEADER));

	/*--------------------------------------------------------------------------------------*/
	//payload를 cPacket에 뽑고 같은지 검사
	/*--------------------------------------------------------------------------------------*/
	if (header.wPayloadSize != 
		pAccount->RecvQ.Get((char *)cPacket.GetBufferPtr(), header.wPayloadSize))
		return FALSE;

	cPacket.MoveWritePos(header.wPayloadSize);

	wprintf(L"PacketRecv [UserNo:%d][Type:%d]\n", uiAccountNo, header.wMsgType);

	/*--------------------------------------------------------------------------------------*/
	// CheckSum 검사(만들지 말지 고민중)
	/*--------------------------------------------------------------------------------------*/
	/*
	if (header.byCheckSum != MakeCheckSum(header.wMsgType, &cPacket))
		wprintf(L"CheckSum Error : %d, %d\n", header.byCheckSum, MakeCheckSum(header.wMsgType, &cPacket));
	*/

	/*--------------------------------------------------------------------------------------*/
	// Message 타입에 따른 Packet 처리
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
// Client -> Server로 온 Packet 처리


//---------------------------------------------------------------------------------------
// 회원가입 요청
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqAccountAdd(stAccount *pAccount, CNPacket *cPacket)
{
	AccountIter aIter;

	cPacket->Get(pAccount->ID, dfNICK_MAX_LEN * 2);
	for (aIter = g_mAccount.begin(); aIter != g_mAccount.end(); ++aIter)
	{
		//중복 닉네임
		if ((aIter->second != pAccount) && 
			(0 != wcscmp(aIter->second->ID, pAccount->ID)))
			return FALSE;
			//TODO : 거부 후 삭제
	}

	return sendProc_ResAccountAdd(pAccount, pAccount->uiAccountNo);
}

//---------------------------------------------------------------------------------------
// 회원 로그인
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqLogin(stAccount *pAccount, CNPacket *cPacket)
{
	AccountIter aIter;
	UINT64 uiAccountNo;

	*cPacket >> uiAccountNo;

	aIter = g_mAccount.find(uiAccountNo);

	//-----------------------------------------------------------------------------------
	// 회원이 없으면 0
	//-----------------------------------------------------------------------------------
	if (aIter == g_mAccount.end())	uiAccountNo = 0;
	
	return sendProc_ResLogin(pAccount, uiAccountNo);
}

//---------------------------------------------------------------------------------------
// 회원리스트 요청
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqAccountList(stAccount *pAccount, CNPacket *cPacket)
{
	return sendProc_ResAccountList(pAccount);
}

//---------------------------------------------------------------------------------------
// 친구목록 요청
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqFriendList(stAccount *pAccount, CNPacket *cPacket)
{
	return sendProc_ResFriendList(pAccount);
}

//---------------------------------------------------------------------------------------
// 친구요청 보낸 목록 요청
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqFriendReqList(stAccount *pAccount, CNPacket *cPacket)
{
	return sendProc_ResFriendReqList(pAccount);
}

//---------------------------------------------------------------------------------------
// 친구요청 받은거 목록 요청
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqFriendReplyList(stAccount *pAccount, CNPacket *cPacket)
{
	return sendProc_ResFriendReplyList(pAccount);
}

//---------------------------------------------------------------------------------------
// 친구관계 끊기
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqFriendRemove(stAccount *pAccount, CNPacket *cPacket)
{
	FriendIter fIterA, fIterB;
	AccountIter aIter;

	UINT64 uiFriendAccountNo;
	BYTE byResult = df_RESULT_FRIEND_REMOVE_OK;

	*cPacket >> uiFriendAccountNo;

	fIterA = g_mFriend.find(pAccount->uiAccountNo);
	fIterB = g_mFriend.find(uiFriendAccountNo);
	aIter = g_mAccount.find(uiFriendAccountNo);

	//친구관계가 없을 때나 회원이 없을 때
	if ((fIterA == g_mFriend.end()) || (fIterB == g_mFriend.end()) ||
		(aIter == g_mAccount.end()))
		byResult = df_RESULT_FRIEND_REMOVE_FAIL;

	//친구가 아닐 때
	if ((fIterA->second->uiFromNo != fIterB->second->uiToNo) ||
		(fIterA->second->uiToNo != fIterB->second->uiFromNo))
		byResult = df_RESULT_FRIEND_REMOVE_NOTFRIEND;

	return sendProc_ResFriendRemove(pAccount, uiFriendAccountNo, byResult);
}

//---------------------------------------------------------------------------------------
// 친구요청
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqFriendReq(stAccount *pAccount, CNPacket *cPacket)
{
	AccountIter aIter;

	UINT64 uiFriendAccountNo;
	BYTE byResult = df_RESULT_FRIEND_REQUEST_OK;

	*cPacket >> uiFriendAccountNo;

	aIter = g_mAccount.find(uiFriendAccountNo);

	if (aIter == g_mAccount.end())
		byResult = df_RESULT_FRIEND_REQUEST_NOTFOUND;
	//이미 요청 되어있을 때

	if (byResult == df_RESULT_FRIEND_REQUEST_OK)
		AddFriendReq(pAccount->uiAccountNo, uiFriendAccountNo);

	return sendProc_ResFriendReq(pAccount, uiFriendAccountNo, byResult);
}

//---------------------------------------------------------------------------------------
// 친구요청 취소
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqFriendCancel(stAccount *pAccount, CNPacket *cPacket)
{
	FriendReqFromIter ffIter;
	FriendReqIter frIter;
	AccountIter aIter;

	UINT64 uiFriendAccountNo;
	BYTE byResult = df_RESULT_FRIEND_CANCEL_OK;

	*cPacket >> uiFriendAccountNo;

	aIter = g_mAccount.find(uiFriendAccountNo);
	ffIter = g_mFriendReqFrom.find(pAccount->uiAccountNo);
	frIter = g_mFriendReq.find(ffIter->second);

	if (aIter == g_mAccount.end())
		byResult = df_RESULT_FRIEND_CANCEL_FAIL;

	if (frIter->second->uiToNo != uiFriendAccountNo)
		byResult = df_RESULT_FRIEND_CANCEL_NOTFRIEND;

	return sendProc_ResFriendCancel(pAccount, uiFriendAccountNo, byResult);
}

//---------------------------------------------------------------------------------------
// 친구요청 거부
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqFriendDeny(stAccount *pAccount, CNPacket *cPacket)
{
	FriendReqToIter ftIter;
	FriendReqIter frIter;
	AccountIter aIter;

	UINT64 uiFriendAccountNo;
	BYTE byResult = df_RESULT_FRIEND_DENY_OK;

	*cPacket >> uiFriendAccountNo;

	aIter = g_mAccount.find(uiFriendAccountNo);
	ftIter = g_mFriendReqTo.find(pAccount->uiAccountNo);
	frIter = g_mFriendReq.find(ftIter->second);

	if (aIter == g_mAccount.end())
		byResult = df_RESULT_FRIEND_CANCEL_FAIL;

	if (frIter->second->uiFromNo != uiFriendAccountNo)
		byResult = df_RESULT_FRIEND_CANCEL_NOTFRIEND;

	return sendProc_ResFriendDeny(pAccount, uiFriendAccountNo, byResult);
}

//---------------------------------------------------------------------------------------
// 친구요청 수락
//---------------------------------------------------------------------------------------
BOOL packetProc_ReqFriendAgree(stAccount *pAccount, CNPacket *cPacket)
{
	FriendReqToIter ftIter;
	FriendReqIter frIter;
	AccountIter aIter;

	UINT64 uiFriendAccountNo;
	BYTE byResult = df_RESULT_FRIEND_AGREE_OK;

	*cPacket >> uiFriendAccountNo;

	aIter = g_mAccount.find(uiFriendAccountNo);
	ftIter = g_mFriendReqTo.find(pAccount->uiAccountNo);
	frIter = g_mFriendReq.find(ftIter->second);

	if (aIter == g_mAccount.end())
		byResult = df_RESULT_FRIEND_AGREE_FAIL;

	if (frIter->second->uiFromNo != uiFriendAccountNo)
		byResult = df_RESULT_FRIEND_AGREE_NOTFRIEND;

	return sendProc_ResFriendAgree(pAccount, uiFriendAccountNo, byResult);
}
/*-------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------------*/
// Server -> Client로 Packet 만들어 보냄

//---------------------------------------------------------------------------------------
// 회원가입 결과
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
// 회원로그인 결과
//---------------------------------------------------------------------------------------
BOOL sendProc_ResLogin(stAccount *pAccount, UINT64 uiAccountNo)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;

	makePacket_ResLogin(&header, &cPacket, uiAccountNo, pAccount->ID);

	SendUnicast(pAccount, &header, &cPacket);

	return TRUE;
}

//---------------------------------------------------------------------------------------
// 회원리스트 결과
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
// 친구요청목록 결과
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
// 친구요청 보낸목록 결과
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
// 친구요청 받은목록 결과
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
// 친구관계 끊기 결과
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
// 친구요청 결과
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
// 친구요청 취소 결과
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
// 친구요청 거부 결과
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
// 친구요청 수락 결과
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
// Packet 제작 함수

//---------------------------------------------------------------------------------------
// 회원가입 결과 Packet 제작
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
// 회원로그인 결과 Packet 제작
//---------------------------------------------------------------------------------------
void makePacket_ResLogin(st_PACKET_HEADER *header, CNPacket *cPacket,
	UINT64 uiAccountNo, WCHAR *pID)
{
	*cPacket << uiAccountNo;

	if (uiAccountNo != 0)	cPacket->Put(pID, dfNICK_MAX_LEN * 2);

	header->byCode = dfPACKET_CODE;
	header->wMsgType = df_RES_LOGIN;
	header->wPayloadSize = cPacket->GetDataSize();
}

//---------------------------------------------------------------------------------------
// 회원리스트 결과 Packet 제작
//---------------------------------------------------------------------------------------
void makePacket_ResAccountList(st_PACKET_HEADER *header, CNPacket *cPacket)
{
	AccountIter aIter;
	int size = g_mAccount.size();

	for (aIter = g_mAccount.begin(); aIter != g_mAccount.end(); ++aIter)
	{
		if (0 == wcscmp(aIter->second->ID, L""))
			size--;
	}

	*cPacket << (UINT)size;
	
	for (aIter = g_mAccount.begin(); aIter != g_mAccount.end(); ++aIter)
	{
		*cPacket << aIter->second->uiAccountNo;
		cPacket->Put(aIter->second->ID, dfNICK_MAX_LEN);
	}

	header->byCode = dfPACKET_CODE;
	header->wMsgType = df_RES_ACCOUNT_LIST;
	header->wPayloadSize = cPacket->GetDataSize();
}

//---------------------------------------------------------------------------------------
// 친구요청목록 결과 Packet 제작
//---------------------------------------------------------------------------------------
void makePacket_ResFriendList(st_PACKET_HEADER *header, CNPacket *cPacket,
	UINT64 uiAccountNo)
{
	FriendIter fIter;
	AccountIter aIter;

	*cPacket << (UINT)g_mFriend.count(uiAccountNo);

	for (fIter = g_mFriend.begin(); fIter != g_mFriend.end(); ++fIter)
	{
		if (fIter->first == uiAccountNo)
		{
			*cPacket << fIter->second->uiToNo;

			aIter = g_mAccount.find(fIter->second->uiToNo);
			*cPacket << aIter->second->ID;
		}
	}

	header->byCode = dfPACKET_CODE;
	header->wMsgType = df_RES_FRIEND_LIST;
	header->wPayloadSize = cPacket->GetDataSize();
}

//---------------------------------------------------------------------------------------
// 친구요청 보낸목록 결과 Packet 제작
//---------------------------------------------------------------------------------------
void makePacket_ResFriendReqList(st_PACKET_HEADER *header, CNPacket *cPacket,
	UINT64 uiAccountNo)
{
	FriendReqFromIter ffIter;
	AccountIter aIter;
	
	*cPacket << (UINT)g_mFriendReqFrom.count(uiAccountNo);

	for (ffIter = g_mFriendReqFrom.begin(); ffIter != g_mFriendReqFrom.end(); ffIter++)
	{
		*cPacket << ffIter->second;

		aIter = g_mAccount.find(ffIter->second);
		*cPacket << aIter->second->ID;
	}

	header->byCode = dfPACKET_CODE;
	header->wMsgType = df_RES_FRIEND_REQUEST_LIST;
	header->wPayloadSize = cPacket->GetDataSize();
}

//---------------------------------------------------------------------------------------
// 친구요청 받은목록 결과 Packet 제작
//---------------------------------------------------------------------------------------
void makePacket_ResFriendReplyList(st_PACKET_HEADER *header, CNPacket *cPacket, 
	UINT64 uiAccountNo)
{
	FriendReqToIter ftIter;
	AccountIter aIter;

	*cPacket << (UINT)g_mFriendReqTo.count(uiAccountNo);

	for (ftIter = g_mFriendReqTo.begin(); ftIter != g_mFriendReqTo.end(); ftIter++)
	{
		*cPacket << ftIter->second;

		aIter = g_mAccount.find(ftIter->second);
		*cPacket << aIter->second->ID;
	}

	header->byCode = dfPACKET_CODE;
	header->wMsgType = df_RES_FRIEND_REPLY_LIST;
	header->wPayloadSize = cPacket->GetDataSize();
}

//---------------------------------------------------------------------------------------
// 친구관계 끊기 결과 Packet 제작
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
// 친구요청 결과 Packet 제작
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
// 친구요청 취소 결과 Packet 제작
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
// 친구요청 거부 결과 Packet 제작
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
// 친구요청 수락 결과 Packet 제작
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
// Stress test용 Packet처리

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
// Stress Test용 Packet 제작
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
// 1 : n Send
/*-------------------------------------------------------------------------------------*/
void SendBroadcast(st_PACKET_HEADER *header, CNPacket *cPacket)
{
	AccountIter iter;

	for (iter = g_mAccount.begin(); iter != g_mAccount.end(); ++iter)
	{
		SendUnicast(iter->second, header, cPacket);
	}
}

/*-------------------------------------------------------------------------------------*/
// 연결 끊기
/*-------------------------------------------------------------------------------------*/
void DisconnectClient(UINT64 uiAccountNo)
{
	AccountIter aIter;
	SockIter sIter;

	if (uiAccountNo == df_NO_LOGIN)
	{
		for (sIter = g_mConnectSock.begin(); sIter != g_mConnectSock.end(); ++sIter)
		{
			if ()
		}
	}
	aIter = g_mAccount.find(uiAccountNo);

	if (aIter != g_mAccount.end())
	{
		aIter->second->sock = INVALID_SOCKET;
		aIter->second->RecvQ.ClearBuffer();
		aIter->second->SendQ.ClearBuffer();
		//memset(&aIter->second->sockaddr, 0, sizeof(SOCKADDR_IN));
	}


	wprintf(L"Disconnect - %s:%d [UserNo:%d][Socket:%d]\n", aIter->second->ID,
		aIter->second->sockaddr.sin_port, uiAccountNo);
}

/*-------------------------------------------------------------------------------------*/
// 회원 찾기
/*-------------------------------------------------------------------------------------*/
stAccount *findAccount(UINT64 uiAccountNo)
{
	AccountIter aIter = g_mAccount.find(uiAccountNo);

	return aIter->second;
}

/*-------------------------------------------------------------------------------------*/
// 친구요청 목록에 추가
/*-------------------------------------------------------------------------------------*/
void AddFriendReq(UINT64 uiFrom, UINT64 uiTo)
{
	stFRIEND_REQ *pFriendReq = new stFRIEND_REQ;
	pFriendReq->uiNo	 = ++uiReqNo;
	pFriendReq->uiFromNo = uiFrom;
	pFriendReq->uiToNo	 = uiTo;
	pFriendReq->Time = time(NULL);

	g_mFriendReq.insert(pair<UINT64, stFRIEND_REQ *>(uiFrom, pFriendReq));

	g_mFriendReqFrom.insert(pair<UINT64, UINT64>(uiFrom, uiReqNo));
	g_mFriendReqTo.insert(pair<UINT64, UINT64>(uiTo, uiReqNo));
}

/*-------------------------------------------------------------------------------------*/
// 친구 목록에 추가
/*-------------------------------------------------------------------------------------*/
void AddFriend(UINT64 uiFrom, UINT64 uiTo)
{
	stFRIEND *pFriendA = new stFRIEND;
	stFRIEND *pFriendB = new stFRIEND;

	pFriendA->uiNo = ++uiFriendNo;
	pFriendA->uiFromNo = uiFrom;
	pFriendA->uiToNo = uiTo;

	pFriendA->uiNo = uiFriendNo;
	pFriendA->uiFromNo = uiTo;
	pFriendA->uiToNo = uiFrom;

	g_mFriend.insert(pair<UINT64, stFRIEND *>(uiFriendNo, pFriendA));
	g_mFriend.insert(pair<UINT64, stFRIEND *>(uiFriendNo, pFriendB));
}

/*-------------------------------------------------------------------------------------*/
// 데이터 초기화
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
		wcscat_s(wName, L"테스트계정");
		wsprintf(wNumber, L"%d", uiAccountNo);
		wcscat_s(wName, wNumber);
		wcscpy_s(pAccount->ID, sizeof(wName), wName);

		g_mAccount.insert(pair<UINT64, stAccount *>(uiAccountNo, pAccount));
	}
}

/*-------------------------------------------------------------------------------------*/
// 접속자 수 체크
/*-------------------------------------------------------------------------------------*/
void ConnectionCheck()
{
	endTime = GetTickCount();

	if ((endTime - startTime) / 1000 > 0)
	{
		wprintf(L"Connection : %d\n", g_mConnectSock.size());
		startTime = GetTickCount();
	}
}