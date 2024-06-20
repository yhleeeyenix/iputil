#ifndef _IP_CAM_DEF_
#define _IP_CAM_DEF_

#define VERSION                 "0.0.21"
/************************************/
/* Defaults _you_ may want to tweak */
/************************************/

typedef	unsigned char	u_int8_t;
typedef unsigned short	u_int16_t;
typedef unsigned int u_int32_t;

#define LISTEN_PORT             59164
#define SEND_PORT               59163
#define UPDATE_PORT				59165
#define MAGIC                   0x73747278      // magic of strx configuration

/* miscellaneous defines */
#define TRUE                    1
#define FALSE                   0
#define MAX_BUF_SIZE            20 /* max xxx.xxx.xxx.xxx-xxx\n */
#define MAX_IP_ADDR             254

/* netconf_Message's flag, codemate */
//-------------------------------------------
/* netconf_Message's flag, codemate */
#define MSG_FLAG_MAGIC_FLAG     0xAB000000
//-------------------------------------------
#define MSG_FLAG_REQUIRE_REBOOT 0x00800000

//-------------------------------------------
#define MSG_FLAG_MAGIC_MASK(x)  (0xFF000000 & x)
#define MSG_FLAG_INFO_MASK(x)   (0x00FFFFFF & x)
#define MSG_FLAG_VERIFY(x)      (MSG_FLAG_MAGIC_MASK(x)?1:0)
#define MSG_FLAG_SET(x)         (MSG_FLAG_MAGIC_FLAG | MSG_FLAG_INFO_MASK(x))
#define MSG_FLAG_GET(x)         (MSG_FLAG_VERIFY(x)?MSG_FLAG_INFO_MASK(x):0)
//-------------------------------------------

 
#pragma pack(push, IPCAMPACK, 1)
struct netMsg {
	u_int8_t opcode;	// opcode
	u_int8_t nettype;	// network type
	u_int16_t secs;		// retransmission seconds
	u_int32_t xid;		// transaction id, random number
	u_int32_t magic;	// magic code
	u_int32_t ciaddr;	// client's current ip address
	u_int8_t chaddr[16];	// client hardware address -> �ٲ�°�
	u_int32_t yiaddr;	// client's yielded ip address (your ip)
	u_int32_t miaddr;	// net mask ip address
	u_int32_t giaddr;	// gateway ip address
	u_int32_t d1iaddr;	// dns first ip address
	u_int32_t d2iaddr;	// dns second ip address
	u_int32_t d3iaddr;	// dns third ip address, ������, NULL
	u_int32_t siaddr;	// server (CMS) ip address
	u_int32_t sport;	// server (CMS) port to notify the state of ready
	u_int32_t http;	// server (HTTP) port to serve http service
	u_int8_t vend[128];	// vendor specific informations
	u_int32_t stream; // stream server port // codemate
	u_int8_t fw_ver[20]; // firmware version // codemate
	u_int8_t vidstd;    // video standard // codemate
	u_int32_t flag;      // 32bit Flag Bitmask Mode �߰�(2014.09.15) 
	u_int8_t chaddr2[6]; /// 20140807 ������ ���� �־���� �Ѵ�(�ݵ��) OPCode �߰���
	u_int32_t runningtime;
}; //__attribute__((packed));
#pragma pack(pop, IPCAMPACK)

enum {  // opcode
	MSG_IP_SEARCH    = 1,	// ������ ī�޶� �˻� ���
	MSG_CAM_ACK      ,    // ������ ī�޶󿡼� ���� ����
	MSG_IP_ACK       ,    // ������ ī�޶�� ������ ����
	MSG_IP_SET	     ,    // ������ ���� ���
  	MSG_CAM_SET      ,
  	MSG_IPUTIL_SEARCH,
  	MSG_IPUTIL_RUN   ,
  	MSG_IP_REBOOT    ,
  	MSG_IP_UPDATE    ,
  	MSG_PTZ_UPDATE   ,
	MSG_IP_MACUPDATE ,
	MSG_CAM_AUTOIP   , // Zero Config
	MSG_IP_DHCP      ,
	MSG_OPCODE_MAX
};


// (Harace) ====================================
#define WM_UPDATE_DATA              WM_USER + 100
enum tag_UpdateStatus {
    UPDATE_START   = 0,
	UPDATE_WRITE	  ,
    UPDATE_FINISH     ,
    UPDATE_FAIL       ,
    UPDATE_MAX
}UPDATE_STATUS;

#define WM_SEARCH_DATA              WM_USER + 200
// (Harace) ====================================


// Add IP Changed!1
enum {		// nettype
  	NET_STATIC = 1,
  	NET_DHCP	  ,
  	NET_PPPoE
};

enum { // Camera Status Flg 
	FLAG_STATUS_AUTOIP = 0,
	FLAG_STATUS_DHCP      ,
	FLAG_STATUS_MAX
};

typedef struct tag_AsdInfo{
    HANDLE hParent;
    BOOL   bAssending;    
}ASD_INFO;

//==================
	const COLORREF BACK_COLOR RGB(45, 45, 48);
	const COLORREF TEXT_COLOR RGB(255, 255, 255);
	const COLORREF COLOR_CUSTOMIZE_ORANGE = RGB(255,177,30);
	const COLORREF COLOR_CUSTOMIZE_RED    = RGB(255,0,30)  ;
#endif // _IP_CAM_DEF_