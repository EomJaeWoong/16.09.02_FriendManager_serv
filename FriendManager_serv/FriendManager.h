#ifndef __FRIENDMANAGER__H__
#define __FRIENDMANAGER__H__

using namespace std;

/*-------------------------------------------------------------------------------------*/
// 회원 정보
/*-------------------------------------------------------------------------------------*/
struct stAccount
{
	UINT64 uiAccountNo;

	WCHAR ID[dfNICK_MAX_LEN];

	CAyaStreamSQ SendQ;
	CAyaStreamSQ RecvQ;

	SOCKET sock;
	SOCKADDR_IN sockaddr;
};

#define Account map<UINT64, stAccount *>
#define AccountIter Account::iterator

/*-------------------------------------------------------------------------------------*/
// 친구 정보
/*-------------------------------------------------------------------------------------*/
struct stFRIEND
{
	UINT64 uiNo;

	UINT64 uiFromNo;
	UINT64 uiToNo;

	time_t Time;
};

#define Friend multimap<UINT64, stFRIEND *>
#define FriendIter Friend::iterator

/*-------------------------------------------------------------------------------------*/
// 친구 요청 정보
/*-------------------------------------------------------------------------------------*/
struct stFRIEND_REQ
{
	UINT64 uiNo;

	UINT64 uiFromNo;
	UINT64 uiToNo;

	time_t Time;
};

#define FriendReq			map<UINT64, stFRIEND_REQ *>
#define FriendReqIter		FriendReq::iterator
#define FriendReq_From		multimap<UINT64, UINT64>
#define FriendReqFromIter	FriendReq_From::iterator
#define FriendReq_To		multimap<UINT64, UINT64>
#define FriendReqToIter		FriendReq_To::iterator

BOOL InitServer();
BOOL OnServer();
void Network();

void InitData();
void ConnectionCheck();

void AcceptClient();
void SocketProc(FD_SET ReadSet, FD_SET WriteSet);
void WriteProc(UINT64 uiAccountNo);
void ReadProc(UINT64 uiAccountNo);

BOOL PacketProc(UINT64 uiAccountNo);

/*-------------------------------------------------------------------------------------*/
// Client -> Server로 온 Packet 처리
/*-------------------------------------------------------------------------------------*/
BOOL packetProc_ReqAccountAdd(stAccount *pAccount, CNPacket *cPacket);
BOOL packetProc_ReqLogin(stAccount *pAccount, CNPacket *cPacket);
BOOL packetProc_ReqAccountList(stAccount *pAccount, CNPacket *cPacket);
BOOL packetProc_ReqFriendList(stAccount *pAccount, CNPacket *cPacket);
BOOL packetProc_ReqFriendReqList(stAccount *pAccount, CNPacket *cPacket);
BOOL packetProc_ReqFriendReplyList(stAccount *pAccount, CNPacket *cPacket);
BOOL packetProc_ReqFriendRemove(stAccount *pAccount, CNPacket *cPacket);
BOOL packetProc_ReqFriendReq(stAccount *pAccount, CNPacket *cPacket);
BOOL packetProc_ReqFriendCancel(stAccount *pAccount, CNPacket *cPacket);
BOOL packetProc_ReqFriendDeny(stAccount *pAccount, CNPacket *cPacket);
BOOL packetProc_ReqFriendAgree(stAccount *pAccount, CNPacket *cPacket);
/*-------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------------*/
// Server -> Client로 Packet 만들어 보냄
/*-------------------------------------------------------------------------------------*/
BOOL sendProc_ResAccountAdd(stAccount *pAccount, UINT64 uiAccountNo);
BOOL sendProc_ResLogin(stAccount *pAccount, UINT64 uiAccountNo);
BOOL sendProc_ResAccountList(stAccount *pAccount);
BOOL sendProc_ResFriendList(stAccount *pAccount);
BOOL sendProc_ResFriendReqList(stAccount *pAccount);
BOOL sendProc_ResFriendReplyList(stAccount *pAccount);
BOOL sendProc_ResFriendRemove(stAccount *pAccount, UINT64 uiFriendAccountNo, BYTE byResult);
BOOL sendProc_ResFriendReq(stAccount *pAccount, UINT64 uiFriendAccountNo, BYTE byResult);
BOOL sendProc_ResFriendCancel(stAccount *pAccount, UINT64 uiFriendAccountNo, BYTE byResult);
BOOL sendProc_ResFriendDeny(stAccount *pAccount, UINT64 uiFriendAccountNo, BYTE byResult);
BOOL sendProc_ResFriendAgree(stAccount *pAccount, UINT64 uiFriendAccountNo, BYTE byResult);
/*-------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------------*/
// Packet 제작 함수
/*-------------------------------------------------------------------------------------*/
void makePacket_ResAccountAdd(st_PACKET_HEADER *header, CNPacket *cPacket, UINT64 uiAccountNo);
void makePacket_ResLogin(st_PACKET_HEADER *header, CNPacket *cPacket, UINT64 uiAccountNo, WCHAR *pID);
void makePacket_ResAccountList(st_PACKET_HEADER *header, CNPacket *cPacket);
void makePacket_ResFriendList(st_PACKET_HEADER *header, CNPacket *cPacket, UINT64 uiAccountNo);
void makePacket_ResFriendReqList(st_PACKET_HEADER *header, CNPacket *cPacket, UINT64 uiAccountNo);
void makePacket_ResFriendReplyList(st_PACKET_HEADER *header, CNPacket *cPacket, UINT64 uiAccountNo);
void makePacket_ResFriendRemove(st_PACKET_HEADER *header, CNPacket *cPacket, UINT64 uiFriendAccountNo, BYTE byResult);
void makePacket_ResFriendReq(st_PACKET_HEADER *header, CNPacket *cPacket, UINT64 uiFriendAccountNo, BYTE byResult);
void makePacket_ResFriendCancel(st_PACKET_HEADER *header, CNPacket *cPacket, UINT64 uiFriendAccountNo, BYTE byResult);
void makePacket_ResFriendDeny(st_PACKET_HEADER *header, CNPacket *cPacket, UINT64 uiFriendAccountNo, BYTE byResult);
void makePacket_ResFriendAgree(st_PACKET_HEADER *header, CNPacket *cPacket, UINT64 uiFriendAccountNo, BYTE byResult);
/*-------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------------*/
// Packet 전송 함수
/*-------------------------------------------------------------------------------------*/
void SendUnicast(stAccount *pClient, st_PACKET_HEADER *header, CNPacket *cPacket);
void SendBroadcast(st_PACKET_HEADER *header, CNPacket *cPacket);

/*-------------------------------------------------------------------------------------*/
// 회원 찾기
/*-------------------------------------------------------------------------------------*/
stAccount *findAccount(UINT64 uiAccountNo);

/*-------------------------------------------------------------------------------------*/
// 친구요청 목록에 추가
/*-------------------------------------------------------------------------------------*/
void AddFriendReq(UINT64 uiFrom, UINT64 uiTo);

#endif