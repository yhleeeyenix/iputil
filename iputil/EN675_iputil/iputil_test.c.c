#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_arp.h>

#define IP_UTIL_LISTEN_PORT 59163
#define IP_UTIL_SEND_PORT 59164
#define MAGIC 0x73747278
#define DHCP_USE_ON 2
#define DHCP_USE_OFF 1
#define INTERFACE_NAME "eth0" // 원하는 네트워크 인터페이스 이름을 지정합니다.

#define BUF_SIZE 512
#define MULTICAST_GROUP "239.255.255.250"

#define FLAG_AUTO_IP 0x1
#define FLAG_DHCP 0x2

static unsigned long  s_xid  = 0;
static unsigned long  s_curXid;

enum {		// opcode
	MSG_IP_SEARCH = 1,
	MSG_CAM_ACK,
	MSG_IP_ACK,
	MSG_IP_SET,
	MSG_CAM_SET,
	MSG_IPUTIL_SEARCH,
	MSG_IPUTIL_RUN,
	MSG_IP_REBOOT,
	MSG_IP_UPDATE,
	MSG_PTZ_UPDATE,	// 10
	MSG_IP_MACUPDATE,
	MSG_CAM_AUTOIP,
	MSG_IP_DHCP
};

enum {  // nettype
    E_NET_STATIC = 1,
    E_NET_DHCP,
    E_NET_PPPoE
};

// NETCONF_MESSAGE 구조체 정의
#pragma pack(push, 1)
typedef struct {
    uint8_t opcode;
    uint8_t nettype;
    uint16_t secs;
    uint32_t xid;
    uint32_t magic;
    uint32_t ciaddr;
    uint8_t chaddr[16];
    uint32_t yiaddr;
    uint32_t miaddr;
    uint32_t giaddr;
    uint32_t d1iaddr;
    uint32_t d2iaddr;
    uint32_t d3iaddr;
    uint32_t siaddr;
    uint32_t sport;
    uint32_t http;
    uint8_t vend[128];
    uint32_t stream_port;
    uint8_t fw_ver[20];
    uint8_t vidstd;
    uint32_t flag;
    uint8_t chaddr2[6];
    uint32_t runningtime;
} NETCONF_MESSAGE;
#pragma pack(pop)



// 현재 IP 함수
int get_current_ip(char *ip_addr) {
    int sock;
    struct ifreq ifr;
    struct sockaddr_in *sin;

    // 네트워크 인터페이스와의 통신을 위해 소켓 생성(IPv4 프로토콜, 스트림을 의미, TCP 소켓(연결지향형 소켓)을 할당함)
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 0;
    }

    // 네트워크 인터페이스 이름 설정
    strncpy(ifr.ifr_name, INTERFACE_NAME, IFNAMSIZ-1);

    // 인터페이스의 IP 주소 가져오기(SIOCGIFADDR: 지정된 인터페이스의 IP 주소를 가져오도록 하는 명령)
    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
        perror("ioctl SIOCGIFADDR");
        close(sock);
        return 0;
    }

    // IP 주소를 문자열로 변환하여 반환
    sin = (struct sockaddr_in *) &ifr.ifr_addr;
    strcpy(ip_addr, inet_ntoa(sin->sin_addr));

    // 소켓 닫기
    close(sock);

    return 1;
}

// 현재 Netmask 함수
int get_current_netmask(char *subnet_mask){
    int sock;
    struct ifreq ifr;
    struct sockaddr_in *sin;

    // SOCK_DGRAM: 데이터 그램을 의미, UDP 소켓(비연결지향형 소켓)을 할당함.
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 네트워크 인터페이스 이름 설정
    strncpy(ifr.ifr_name, INTERFACE_NAME, IFNAMSIZ-1);
        if (ioctl(sock, SIOCGIFNETMASK, &ifr) == -1) {
        perror("ioctl SIOCGIFNETMASK");
        close(sock);
        exit(EXIT_FAILURE);
    }

    sin = (struct sockaddr_in *) &ifr.ifr_addr;
    strcpy(subnet_mask, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr));

    close(sock);
    return 1;
}

// 현재 gateway 함수
int get_current_gateway(char *gateway){
    FILE *fp;
    char line[256];
    fp = popen("ip route show default", "r");
    if (fp == NULL) {
        perror("popen");
        exit(EXIT_FAILURE);
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "default", 7) == 0) {
            sscanf(line, "default via %s", gateway);
            break;
        }
    }
    pclose(fp);
}

// DNS 함수
void get_dns_servers(char *dns1, char *dns2) {
    FILE *fp;
    char line[256];
    int count = 0;

    fp = fopen("/etc/resolv.conf", "r");
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "nameserver", 10) == 0) {
            char *token = strtok(line, " ");
            token = strtok(NULL, " \n");

            if (token != NULL) {
                if (count == 0) {
                    strcpy(dns1, token);
                } else if (count == 1) {
                    strcpy(dns2, token);
                    break;
                }
                count++;
            }
        }
    }

    fclose(fp);
}

// MAC주소 함수
void get_mac_address(unsigned char *mac_address) {
    int sock;
    struct ifreq ifr;

    // 소켓 생성
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 인터페이스 이름 설정
    strncpy(ifr.ifr_name, INTERFACE_NAME, IFNAMSIZ-1);
    ifr.ifr_name[IFNAMSIZ-1] = '\0'; // 널 종료

    // MAC 주소 가져오기
    if (ioctl(sock, SIOCGIFHWADDR, &ifr) == -1) {
        perror("ioctl SIOCGIFHWADDR");
        close(sock);
        exit(EXIT_FAILURE);
    }

    close(sock);

    // MAC 주소 복사
    memcpy(mac_address, ifr.ifr_hwaddr.sa_data, 6);
}

int is_dhcp_client_running() {
    // DHCP 클라이언트 확인
    int dhcp_status = system("ps aux | grep -E 'dhclient|dhcpcd|udhcpc' | grep -v grep > /dev/null");
    if (dhcp_status == 0) {
        return E_NET_DHCP; // DHCP 클라이언트 실행 중
    }

    // PPPoE 클라이언트 확인
    int pppoe_status = system("ps aux | grep -E 'pppd|pppoe' | grep -v grep > /dev/null");
    if (pppoe_status == 0) {
        return E_NET_PPPoE; // PPPoE 클라이언트 실행 중
    }

    // Static IP로 가정
    return E_NET_STATIC; // DHCP 또는 PPPoE 클라이언트 실행 안 함
}



// HTTP 서비스 포트 얻기
int get_http_port() {
    FILE *fp;
    char line[256];
    int port = 80; // 기본 HTTP 포트

    // fp = popen("ss -tnlp | grep ':80 ' | awk '{print $4}'", "r");
    // if (fp == NULL) {
    //     perror("popen");
    //     return port;
    // }

    // while (fgets(line, sizeof(line), fp) != NULL) {
    //     if (strstr(line, ":80")) {
    //         sscanf(line, ":%d", &port);
    //         break;
    //     }
    // }

    // pclose(fp);
    return port;
}

// running time
unsigned long get_system_uptime() {
    FILE *fp;
    double uptime_seconds;

    fp = fopen("/proc/uptime", "r");
    if (fp == NULL) {
        perror("fopen");
        return 0;
    }

    if (fscanf(fp, "%lf", &uptime_seconds) != 1) {
        perror("fscanf");
        fclose(fp);
        return 0;
    }

    fclose(fp);
    return (unsigned long)uptime_seconds;
}


// 기본 네트워크 정보를 설정하는 함수
void make_packet(NETCONF_MESSAGE *packet, char opcode) {
    char ip_address[INET_ADDRSTRLEN]; // 충분한 크기로 배열 선언
    get_current_ip(ip_address);

    char subnet_mask[INET_ADDRSTRLEN];
    get_current_netmask(subnet_mask);

    char gateway[INET_ADDRSTRLEN];
    get_current_gateway(gateway);

    unsigned char mac[6];
    get_mac_address(mac);

    char dns1[INET_ADDRSTRLEN];
    char dns2[INET_ADDRSTRLEN];
    get_dns_servers(dns1, dns2);

    int http_port = get_http_port();

    char strNodeName[64];

    packet->opcode = opcode;
    packet->nettype = is_dhcp_client_running();
    packet->secs = htons(0);
    packet->xid =  get_system_uptime();
    packet->magic = htonl(MAGIC);
    packet->ciaddr = inet_addr(ip_address);
    memcpy(packet->chaddr, mac, 6);
    packet->miaddr = inet_addr(subnet_mask);
    packet->giaddr = inet_addr(gateway);
    packet->d1iaddr = inet_addr(dns1);
    packet->d2iaddr = inet_addr(dns2);
    // packet->sport = htonl(IP_UTIL_SEND_PORT);
    packet->http = htonl(http_port);
    packet->stream_port = htonl(554);
    strcpy((char*)packet->fw_ver, "1.0.0");

    // packet->flag = 0x1  & gtUser.autoipoffon;
    packet->flag = 0x1;

	if(packet->nettype== DHCP_USE_OFF)
		packet->flag &= 0xFFFD;	// dhcp Disable
	else
		packet->flag |= ((0x1)<< 1);

    // flag 초기화 및 설정
    // packet->flag = 0;

    // if (packet->nettype == E_NET_STATIC) {
    //     packet->flag |= FLAG_AUTO_IP;  // Auto IP 사용 (예시로 설정)
    // } else if (packet->nettype == E_NET_DHCP) {
    //     packet->flag |= FLAG_DHCP;     // DHCP 사용
    // }

    packet->runningtime = get_system_uptime();

    memset (strNodeName, 0, sizeof(strNodeName));
	strcpy (strNodeName, "cp100");

	sprintf((char *)(packet->vend), "%s; %s; %s; %s; %s",
			strNodeName, "svrsysname", "svrrelease", "svrversion", "svrmachine");
}

// 패킷을 출력하는 함수
void print_packet_hex(const char *packet, int length) {
    for (int i = 0; i < length; i += 16) {
        printf("%04x   ", i);
        for (int j = 0; j < 16; j++) {
            if (i + j < length) {
                printf("%02x ", (unsigned char)packet[i + j]);
            } else {
                printf("   ");
            }
        }
        printf("  ");
        for (int j = 0; j < 16; j++) {
            if (i + j < length) {
                char ch = packet[i + j];
                if (ch >= 32 && ch <= 126) {
                    printf("%c", ch);
                } else {
                    printf(".");
                }
            }
        }
        printf("\n");
    }
}

// 패킷을 보내는 함수
void send_packet(NETCONF_MESSAGE *pbuf, struct sockaddr_in *client_addr) {
    int send_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 서버 주소 설정
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(IP_UTIL_SEND_PORT);

    if (bind(send_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(send_sock);
        exit(EXIT_FAILURE);
    }

    // 클라이언트 주소 설정
    client_addr->sin_port = htons(IP_UTIL_SEND_PORT);

    int ret = sendto(send_sock, pbuf, sizeof(NETCONF_MESSAGE), 0, (struct sockaddr*)client_addr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        perror("sendto");
        close(send_sock);
        exit(EXIT_FAILURE);
    }

    printf("Sent response packet:\n");
    // print_packet_hex((char*)pbuf, sizeof(NETCONF_MESSAGE));

    close(send_sock);
}


// 인터페이스의 IP 주소를 설정하는 함수
void set_ip_address(const char *new_ip_address) {
    int sockfd;
    struct ifreq ifr;
    struct sockaddr_in *addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    strncpy(ifr.ifr_name, INTERFACE_NAME, IFNAMSIZ-1);
    ifr.ifr_name[IFNAMSIZ-1] = '\0';

    addr = (struct sockaddr_in *)&ifr.ifr_addr;
    addr->sin_family = AF_INET;
    inet_pton(AF_INET, new_ip_address, &addr->sin_addr);

    // SIOCSIFADDR: 지정된 네트워크 인터페이스의 IP 주소를 설정
    if (ioctl(sockfd, SIOCSIFADDR, &ifr) == -1) {
        perror("ioctl SIOCSIFADDR");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    close(sockfd);
}

// 인터페이스의 서브넷 마스크를 설정하는 함수
void set_netmask(const char *netmask) {
    int sockfd;
    struct ifreq ifr;
    struct sockaddr_in *addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    strncpy(ifr.ifr_name, INTERFACE_NAME, IFNAMSIZ-1);
    ifr.ifr_name[IFNAMSIZ-1] = '\0';

    addr = (struct sockaddr_in *)&ifr.ifr_netmask;
    addr->sin_family = AF_INET;
    inet_pton(AF_INET, netmask, &addr->sin_addr);

    if (ioctl(sockfd, SIOCSIFNETMASK, &ifr) == -1) {
        perror("ioctl SIOCSIFNETMASK");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    close(sockfd);
}

// 인터페이스의 게이트웨이를 설정하는 함수
void set_gateway(const char *gateway) {
    char cmd[100];
    snprintf(cmd, sizeof(cmd), "ip route add default via %s dev %s", gateway, INTERFACE_NAME);
    system(cmd);
}

// 인터페이스의 DNS 서버를 설정하는 함수
void set_dns_server(const char *dns1, const char *dns2) {
    FILE *fp = fopen("/etc/resolv.conf", "w");
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    fprintf(fp, "nameserver %s\n", dns1);
    fprintf(fp, "nameserver %s\n", dns2);
    fclose(fp);
}

static int setup_network(NETCONF_MESSAGE *packet, void *ext) {
    int i;
    int bMacSame = 1;
    unsigned char current_mac[6];

    // 현재 MAC 주소 가져오기
    get_mac_address(current_mac);

    // MAC 주소 비교
    for (i = 0; i < 6; i++) {
        if (packet->chaddr[i] != current_mac[i]) {
            bMacSame = 0;
            printf("MAC 주소가 다르기 때문에 네트워크 설정 업데이트 안함\n");
            break;
        }
    }
    if (packet->nettype == E_NET_STATIC) {
        if (bMacSame) {
            // IP 주소 설정
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &packet->yiaddr, ip, INET_ADDRSTRLEN);
            set_ip_address(ip);
            printf("변경할 IP 주소: %s\n", ip);

            // 서브넷 마스크 설정
            char netmask[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &packet->miaddr, netmask, INET_ADDRSTRLEN);
            set_netmask(netmask);
            printf("변경할 서브넷 마스크: %s\n", netmask);

            // 게이트웨이 설정
            char gateway[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &packet->giaddr, gateway, INET_ADDRSTRLEN);
            set_gateway(gateway);
            printf("변경할 게이트웨이: %s\n", gateway);

            // DNS 서버 설정
            char dns1[INET_ADDRSTRLEN];
            char dns2[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &packet->d1iaddr, dns1, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &packet->d2iaddr, dns2, INET_ADDRSTRLEN);
            set_dns_server(dns1, dns2);
            printf("변경할 DNS 서버 1: %s\n", dns1);
            printf("변경할 DNS 서버 2: %s\n", dns2);
        }

    } else if (packet->nettype == E_NET_DHCP) {
        printf("ERROR: DHCP not implemented!\n");
    } else if (packet->nettype == E_NET_PPPoE) {
        printf("ERROR: PPPoE not implemented!\n");
    }

    return 0;
}

// 시스템 재부팅
static int setup_Reboot()
{
	printf("System Reboot!\r\n\r\n");
    sleep(1);
	system("reboot");

	return 0;
}

// 인터페이스의 Mac 주소를 설정하는 함수
int set_mac_address(const char *interface_name, const uint8_t *mac_address) {
    int sock;
    struct ifreq ifr;
    char gateway[INET_ADDRSTRLEN];

    // 인터페이스 비활성화
    char cmd[100];
    get_current_gateway(gateway);

    snprintf(cmd, sizeof(cmd), "ifconfig %s down", interface_name);
    system(cmd);

    // 소켓 생성
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("socket");
        return -1;
    }

    // 인터페이스 이름 설정
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ-1);
    ifr.ifr_name[IFNAMSIZ-1] = '\0';

    // MAC 주소 설정
    memcpy(ifr.ifr_hwaddr.sa_data, mac_address, 6);
    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;

    if (ioctl(sock, SIOCSIFHWADDR, &ifr) == -1) {
        perror("ioctl");
        close(sock);
        return -1;
    }

    close(sock);

    // 인터페이스 활성화
    snprintf(cmd, sizeof(cmd), "ifconfig %s up", interface_name);
    system(cmd);
    set_gateway(gateway);

    return 0;
}

static int setup_macaddr(NETCONF_MESSAGE *packet, void *ext) {
    int i;
    int bChanged = 0;
    char ip_address[INET_ADDRSTRLEN];
    get_current_ip(ip_address);

    if (packet->nettype == E_NET_STATIC) {
        printf("setup_macaddr packet->nettype %d\r\n", packet->nettype);
        // IP 주소 비교
        if (packet->ciaddr == inet_addr(ip_address)) {
            printf("packet->ciaddr == inet_addr(ip_address)\n");
            for (i = 0; i < 6; i++) {
                // 기존의 Mac 주소와 변경하려는 Mac 주소 비교
                if (packet->chaddr[i] != packet->chaddr2[i]) {
                    bChanged = 1;
                    break;
                }
            }

            if (bChanged) {
                // printf("1. %d\n", bChanged);
                for (i = 0; i < 6; i++) {
                    if (packet->chaddr[i] != packet->chaddr2[i]) {
                        bChanged = 1;
                        break;
                    }
                }

                if (bChanged) {
                    // printf("2. %d\n", bChanged);
                    uint8_t *p = packet->chaddr2;
                    if (set_mac_address(INTERFACE_NAME, p) == 0) {
                        printf("MAC address successfully changed to: %02x:%02x:%02x:%02x:%02x:%02x\n",
                                p[0], p[1], p[2], p[3], p[4], p[5]);
                    } else {
                        printf("Failed to change MAC address\n");
                    }
                }
            }
            else{
                printf("bChanged의 값: %d\n",bChanged);
            }
        }
        else{
            printf("%d, %d\n", packet->ciaddr, inet_addr(ip_address));
        }
    }
    // free(ip_address);
    return 0;
}


int main() {
    int recv_sock;
    struct sockaddr_in server_addr, client_addr;
    struct ip_mreq mreq;
    socklen_t addrlen = sizeof(client_addr);
    char buffer[BUF_SIZE];
    NETCONF_MESSAGE snet_conf;
    unsigned long searchXid = 0;

    // 소켓 생성(UDP)
    recv_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (recv_sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 주소와 포트 설정
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(IP_UTIL_LISTEN_PORT);

    // 소켓에 주소 바인딩
    if (bind(recv_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(recv_sock);
        exit(EXIT_FAILURE);
    }

    // 멀티캐스트 그룹에 가입
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(recv_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("setsockopt");
        close(recv_sock);
        exit(EXIT_FAILURE);
    }

    printf("Listening for messages on port %d and joined multicast group %s...\n", IP_UTIL_LISTEN_PORT, MULTICAST_GROUP);

    while (1) {
        // 메시지 수신
        int n = recvfrom(recv_sock, buffer, BUF_SIZE, 0, (struct sockaddr*)&client_addr, &addrlen);
        if (n < 0) {
            perror("recvfrom");
            close(recv_sock);
            exit(EXIT_FAILURE);
        }

        printf("Received message from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // 수신한 패킷 출력
        // print_packet_hex(buffer, n);

        // 요청 메시지 확인
        NETCONF_MESSAGE *rnet_conf = (NETCONF_MESSAGE*)buffer;
        if (ntohl(rnet_conf->magic) != MAGIC) {
            printf("Invalid magic number: %x\n", ntohl(rnet_conf->magic));
            continue; // 잘못된 메시지 무시
        }

        printf("Valid request received, preparing response...\n");

        // 응답 메시지 생성 및 전송
        if (rnet_conf->opcode == MSG_IP_SEARCH) {
            if (searchXid == rnet_conf->xid)
				continue;

            searchXid = rnet_conf->xid;
            make_packet(&snet_conf, MSG_CAM_ACK);
            send_packet(&snet_conf, &client_addr);
            s_curXid = snet_conf.xid;
            printf("iputil : MSG_IP_SEARCH E s_curXid %d\r\n",s_curXid);

        }

        // 장치 설정 요청을 처리 -> s_curXid가 패킷의 xid와 일치하면 네트워크 설정을 업데이트합니다
        else if (rnet_conf->opcode == MSG_IP_SET) {
            if (s_curXid == rnet_conf->xid) {
                setup_network(rnet_conf, 0);
                printf("장치 설정 요청\n");
                s_curXid = 0;
            }
            else{
                printf("not same s_curXid xid\n");
                printf("%02x, %02x\n", s_curXid, rnet_conf->xid);
            }
        }

        // ACK 패킷을 처리(네트워크 통신에서 수신한 데이터가 성공적으로 전달되었음을 확인하기 위해 사용되는 패킷)
        else if (rnet_conf->opcode == MSG_IP_ACK) {
            printf ("iputil : MSG_IP_ACK\r\n");
        }

        // 장치 재부팅 요청을 처리
        else if (rnet_conf->opcode == MSG_IP_REBOOT) {
            if (s_curXid == rnet_conf->xid) {
                setup_Reboot();
                printf("장치 재부팅 요청\n");
                s_curXid = 0;
            }
            else{
                printf("boot fail\n");
                printf("%02x, %02x\n", s_curXid, rnet_conf->xid);
            }            
        }

        // 장치 업데이트 요청을 처리
        else if (rnet_conf->opcode == MSG_IP_UPDATE) {
            printf("iputil : MSG_IP_UPDATE\r\n");
            if (s_curXid == rnet_conf->xid){
                printf("장치 업데이트 요청\n");
            }
            else{
                printf("장치 업데이트 조건 실패\n");
            }
        
			// iprintf ("iputil : MSG_IP_UPDATE\r\n");
			// if (s_curXid == rnet_conf->xid)
			// {
			// 	while(1)
			// 	{
			// 		if (update_sock < 0)
			// 		{
			// 			update_sock = socket(AF_INET, SOCK_DGRAM, 0);

			// 			if(update_sock == -1)
			// 			{
			// 				iprintf("Iputil : UDP recv_socket error! retry...\r\n");
			// 				vTaskDelay(20);
			// 				continue;
			// 			}
			// 		}

			// 		memset(&update_addr, 0, sizeof(update_addr));
			// 		update_addr.sin_family = AF_INET;
			// 		update_addr.sin_addr.s_addr = to.sin_addr.s_addr; //htonl(INADDR_ANY);	//IPADDR_BROADCAST;//INADDR_ANY;
			// 		update_addr.sin_port = htons(IP_UTIL_UPDATE_PORT);
			// 		iprintf("bind() ++\r\n");

			// 		connect(update_sock, (struct sockaddr*)&update_addr, sizeof(update_addr));

			// 		break;
			// 	}
			// 	update_len = sizeof(update_to);
			// 	while(1)
			// 	{
			// 		buffer = fwupdate_process();
			// 		slen = sendto(update_sock, &buffer, sizeof(buffer), 0, NULL, 0);
			// 		vTaskDelay(100);
			// 		if(buffer == 100)
			// 			break;
			// 	}
				//vTaskDelay(10);
				//s_curXid = 0;
			// }
		}

        // MAC 주소 업데이트 요청을 처리
        else if (rnet_conf->opcode == MSG_IP_MACUPDATE) {
            if (s_curXid == rnet_conf->xid) {
                setup_macaddr(rnet_conf, NULL);
                printf("MAC 주소 업데이트 요청\n");
                s_curXid = 0;
            }
            else{
                printf("MCK change failed\n");
                printf("%02x, %02x\n", s_curXid, rnet_conf->xid);
            }            
        }

        // 자동 IP 설정 요청을 처리
        else if (rnet_conf->opcode == MSG_CAM_AUTOIP) {
            if (s_curXid == rnet_conf->xid) {
                // setup_AutoIp(&rnet_conf, 0);
                printf("자동 IP 설정 요청\n");
                s_curXid = 0;
            }
            else{
                printf("자동 IP 설정 요청 실패\n");
            }            
        }

        // DHCP 설정 요청을 처리
        else if (rnet_conf->opcode == MSG_IP_DHCP) {
            if (s_curXid == rnet_conf->xid) {
                // setup_Dhcp(&rnet_conf, 0);
                printf("DHCP 설정 요청\n");
                s_curXid = 0;
            }
            else{
                printf("noDHCP 설정 요청 실패\n");
            }            
        }

        printf("Response sent.\n");
    }

    close(recv_sock);
    return 0;
}







