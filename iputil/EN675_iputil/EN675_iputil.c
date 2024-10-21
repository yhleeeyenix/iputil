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
#include <signal.h>
#include <stdbool.h>


#define IP_UTIL_LISTEN_PORT 59163
#define IP_UTIL_SEND_PORT 59164
#define MAGIC 0x73747278
#define DHCP_USE_ON 2
#define DHCP_USE_OFF 1
#define INTERFACE_NAME "eth0"
#define MAX_LINE_LENGTH 1024

#define BUF_SIZE 512
#define MULTICAST_GROUP "239.255.255.250"

#define FLAG_AUTO_IP 0x1
#define FLAG_DHCP 0x2

#define ID 1234

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

// NETCONF_MESSAGE structure definition
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

// Function to get IP address
int get_current_ip(char *ip_addr) {
    int sock;
    struct ifreq ifr;
    struct sockaddr_in *sin;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 0;
    }

    strncpy(ifr.ifr_name, INTERFACE_NAME, IFNAMSIZ-1);

    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
        perror("ioctl SIOCGIFADDR");
        close(sock);
        return 0;
    }

    sin = (struct sockaddr_in *) &ifr.ifr_addr;
    strcpy(ip_addr, inet_ntoa(sin->sin_addr));

    close(sock);

    return 1;
}

// Function to get netmask
int get_current_netmask(char *subnet_mask){
    int sock;
    struct ifreq ifr;
    struct sockaddr_in *sin;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

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

// Function to get gateway
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

// Function to get DNS address
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
        if (strncmp(line, "nameserver", 10) == 0 && strstr(line, "# eth0") != NULL) {
            char *token = strtok(line, " ");
            token = strtok(NULL, " \n#");

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

// Function to get MAC address
void get_mac_address(unsigned char *mac_address) {
    int sock;
    struct ifreq ifr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    strncpy(ifr.ifr_name, INTERFACE_NAME, IFNAMSIZ-1);
    ifr.ifr_name[IFNAMSIZ-1] = '\0';

    if (ioctl(sock, SIOCGIFHWADDR, &ifr) == -1) {
        perror("ioctl SIOCGIFHWADDR");
        close(sock);
        exit(EXIT_FAILURE);
    }

    close(sock);

    memcpy(mac_address, ifr.ifr_hwaddr.sa_data, 6);
}

// Function to check whether the DHCP function is operating
int is_dhcp_client_running() {
    int dhcp_status = system("ps aux | grep -E 'dhclient|dhcpcd|udhcpc' | grep -v grep > /dev/null");
    if (dhcp_status == 0) {
        return E_NET_DHCP;
    }

    int pppoe_status = system("ps aux | grep -E 'pppd|pppoe' | grep -v grep > /dev/null");
    if (pppoe_status == 0) {
        return E_NET_PPPoE;
    }

    return E_NET_STATIC;
}

int get_http_port() {
    return 80;
}

// Function to get runtime
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

void make_packet(NETCONF_MESSAGE *packet, char opcode) {
    char ip_address[INET_ADDRSTRLEN];
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
    // packet->xid = get_system_uptime();

    packet->xid = ID;

    packet->magic = htonl(MAGIC);
    packet->ciaddr = inet_addr(ip_address);
    memcpy(packet->chaddr, mac, 6);
    packet->miaddr = inet_addr(subnet_mask);
    packet->giaddr = inet_addr(gateway);
    packet->d1iaddr = inet_addr(dns1);
    packet->d2iaddr = inet_addr(dns2);
    packet->http = htonl(http_port);
    packet->stream_port = htonl(554);
    strcpy((char*)packet->fw_ver, "1.0.0");

    packet->flag = 0;

    if(packet->nettype == DHCP_USE_OFF)
        packet->flag &= 0xFFFD;
    else
        packet->flag |= ((0x1)<< 1);

    packet->flag = htonl(packet->flag); // Convert to network byte order

    packet->runningtime = ntohl(get_system_uptime());

    memset (strNodeName, 0, sizeof(strNodeName));
	strcpy (strNodeName, "cp100");

	sprintf((char *)(packet->vend), "%s; %s; %s; %s; %s",
			strNodeName, "svrsysname", "svrrelease", "svrversion", "svrmachine");
}

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

void send_packet(NETCONF_MESSAGE *pbuf) {
    char broadcast_ip[] = "255.255.255.255";
    int broadcast_port = IP_UTIL_SEND_PORT;

    // printf("Sending broadcast packet to %s:%d\n", broadcast_ip, broadcast_port);

    int send_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int broadcast_enable = 1;
    if (setsockopt(send_sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("setsockopt (SO_BROADCAST)");
        close(send_sock);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = inet_addr(broadcast_ip);
    broadcast_addr.sin_port = htons(broadcast_port);

    int ret = sendto(send_sock, pbuf, sizeof(NETCONF_MESSAGE), 0, (struct sockaddr*)&broadcast_addr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        perror("sendto");
        close(send_sock);
        exit(EXIT_FAILURE);
    }

    // printf("Sent broadcast packet:\n");
    // print_packet_hex((char*)pbuf, sizeof(NETCONF_MESSAGE));

    close(send_sock);
}

// Function to change IP address
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

    if (ioctl(sockfd, SIOCSIFADDR, &ifr) == -1) {
        perror("ioctl SIOCSIFADDR");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    close(sockfd);


}

// Function to change gateway
void set_gateway(const char *gateway) {
    char cmd[100];
    snprintf(cmd, sizeof(cmd), "ip route del default dev %s", INTERFACE_NAME);
    system(cmd);

    snprintf(cmd, sizeof(cmd), "ip route add default via %s dev %s", gateway, INTERFACE_NAME);
    system(cmd);
}

// Function to change netmask
void set_netmask(const char *netmask) {
    int sockfd;
    struct ifreq ifr;
    struct sockaddr_in *addr;

    char gateway[INET_ADDRSTRLEN];
    get_current_gateway(gateway);

    char cmd[100];
    snprintf(cmd, sizeof(cmd), "ifconfig %s down", INTERFACE_NAME);
    system(cmd);

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

    set_gateway(gateway);

    snprintf(cmd, sizeof(cmd), "ifconfig %s up", INTERFACE_NAME);
    system(cmd);
    set_gateway(gateway);

    close(sockfd);
}

// Function to change DNS address
void set_dns_server(const char *dns1, const char *dns2) {
    FILE *fp;
    char line[256];
    char content[2048] = "";
    int dns1_set = 0;
    int dns2_set = 0;

    fp = fopen("/etc/resolv.conf", "r");
    if (fp == NULL) {
        perror("fopen");
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, "nameserver") != NULL && strstr(line, "# eth0") != NULL) {
            if (!dns1_set) {
                snprintf(line, sizeof(line), "nameserver %s # eth0\n", dns1);
                dns1_set = 1;
            } else if (!dns2_set) {
                snprintf(line, sizeof(line), "nameserver %s # eth0\n", dns2);
                dns2_set = 1;
            }
        }
        strncat(content, line, sizeof(content) - strlen(content) - 1);
    }
    fclose(fp);

    fp = fopen("/etc/resolv.conf", "w");
    if (fp == NULL) {
        perror("fopen");
        return;
    }

    fputs(content, fp);
    fclose(fp);
}

// Validate IP address (check if 4 dotted numbers are between 0 and 255)
bool is_valid_ip(const char *ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip, &(sa.sin_addr)) != 0;
}

// // Validate netmask
// bool is_valid_netmask(const char *netmask) {
//     struct in_addr addr;
//     if (inet_pton(AF_INET, netmask, &addr) == 0) {
//         return false;
//     }

//     uint32_t mask = ntohl(addr.s_addr);
//     // printf("mask: %u\n", mask);
//     bool found_zero = false;

//     for (int i = 0; i < 32; i++) {
//         printf("Bit %d: %d\n", i, (mask & (1 << (31 - i))) != 0);
//         if (mask & (1 << (31 - i))) {
//             if (found_zero) {
//                 return false;
//             }
//         } else {
//             found_zero = true;
//         }
//     }
//     return true;
// }

// // Validate gateway
// bool is_valid_gateway(const char *gateway, const char *ip, const char *netmask) {
//     struct in_addr gateway_addr, ip_addr, netmask_addr, network_addr;

//     if (inet_pton(AF_INET, gateway, &gateway_addr) == 0 ||
//         inet_pton(AF_INET, ip, &ip_addr) == 0 ||
//         inet_pton(AF_INET, netmask, &netmask_addr) == 0) {
//         return false;
//     }

//     network_addr.s_addr = ip_addr.s_addr & netmask_addr.s_addr;

//     if ((gateway_addr.s_addr & netmask_addr.s_addr) != network_addr.s_addr) {
//         return false;
//     }

//     if (gateway_addr.s_addr == network_addr.s_addr ||
//         gateway_addr.s_addr == (network_addr.s_addr | ~netmask_addr.s_addr)) {
//         return false;
//     }

//     return true;
// }

// Validate DNS address
bool is_valid_dns(const char *dns) {
    return is_valid_ip(dns);
}

static int setup_network(NETCONF_MESSAGE *packet, void *ext) {
    int i;
    int bMacSame = 1;
    unsigned char current_mac[6];

    get_mac_address(current_mac);

    for (i = 0; i < 6; i++) {
        if (packet->chaddr[i] != current_mac[i]) {
            bMacSame = 0;
            printf("Network settings not updated because MAC addresses are different\n");
            break;
        }
    }
    if (packet->nettype == E_NET_STATIC) {
        if (bMacSame) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &packet->yiaddr, ip, INET_ADDRSTRLEN);

            char netmask[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &packet->miaddr, netmask, INET_ADDRSTRLEN);

            char gateway[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &packet->giaddr, gateway, INET_ADDRSTRLEN);

            char dns1[INET_ADDRSTRLEN];
            char dns2[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &packet->d1iaddr, dns1, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &packet->d2iaddr, dns2, INET_ADDRSTRLEN);

            if (!is_valid_ip(ip)) {
                printf("Invalid IP address: %s\n", ip);
                return -1;
            }

            // if (!is_valid_netmask(netmask)) {
            //     printf("유효하지 않은 서브넷 마스크: %s\n", netmask);
            //     return -1;
            // }

            // if (!is_valid_gateway(gateway, ip, netmask)) {
            //     printf("유효하지 않은 게이트웨이: %s\n", gateway);
            //     return -1;
            // }

            // if (!is_valid_dns(dns1) || !is_valid_dns(dns2)) {
            //     printf("유효하지 않은 DNS 서버: %s, %s\n", dns1, dns2);
            //     return -1;
            // }

            set_ip_address(ip);
            printf("IP address to change: %s\n", ip);

            set_netmask(netmask);
            printf("Subnet mask to change: %s\n", netmask);

            set_gateway(gateway);
            printf("Change gateway: %s\n", gateway);

            set_dns_server(dns1, dns2);
            printf("DNS Server 1 to change: %s\n", dns1);
            printf("DNS Server 2 to change: %s\n", dns2);
        }
    } else if (packet->nettype == E_NET_DHCP) {
        printf("ERROR: DHCP not implemented!\n");
    } else if (packet->nettype == E_NET_PPPoE) {
        printf("ERROR: PPPoE not implemented!\n");
    }

    return 0;
}

// Function for reboot function
static int setup_Reboot()
{
	printf("System Reboot!\r\n\r\n");
    sleep(1);
	system("reboot");

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
    int opt = 1;

    // Create server socket: Communicate using the IPv4 addressing scheme and UDP protocol
    recv_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (recv_sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set socket options: Use the "SO_REUSEADDR" option to enable reuse even if the port is in use.
    if (setsockopt(recv_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(recv_sock);
        exit(EXIT_FAILURE);
    }

    // Server address settings and binding: 
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(IP_UTIL_LISTEN_PORT);

    if (bind(recv_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(recv_sock);
        exit(EXIT_FAILURE);
    }
    // print_packet_hex(buffer, n);

    // Join a multicast group:
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(recv_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("setsockopt");
        close(recv_sock);
        exit(EXIT_FAILURE);
    }

    printf("Listening for messages on port %d and joined multicast group %s...\n", IP_UTIL_LISTEN_PORT, MULTICAST_GROUP);

    // Message receiving loop:
    while (1) {
        // Receive UDP packets.
        int n = recvfrom(recv_sock, buffer, BUF_SIZE, 0, (struct sockaddr*)&client_addr, &addrlen);
        if (n < 0) {
            perror("recvfrom");
            close(recv_sock);
            exit(EXIT_FAILURE);
        }

        // packet processing
        NETCONF_MESSAGE *rnet_conf = (NETCONF_MESSAGE*)buffer;
        if (ntohl(rnet_conf->magic) != MAGIC) {
            printf("Invalid magic number: %x\n", ntohl(rnet_conf->magic));
            continue;
        }

        if (rnet_conf->opcode == MSG_IP_SEARCH) {
            if (searchXid == rnet_conf->xid)
                continue;

            searchXid = rnet_conf->xid;
            make_packet(&snet_conf, MSG_CAM_ACK);
            send_packet(&snet_conf);
            s_curXid = snet_conf.xid;
            // printf("iputil : MSG_IP_SEARCH E s_curXid %d\r\n",s_curXid);
        } 

        else if (rnet_conf->opcode == MSG_IP_SET) {
            if (s_curXid == rnet_conf->xid) {
                setup_network(rnet_conf, 0);
                printf("Request network settings\n");
                s_curXid = 0;
            } else {
                printf("not same s_curXid xid\n");
                printf("%02x, %02x\n", s_curXid, rnet_conf->xid);
            }

        } 

        else if (rnet_conf->opcode == MSG_IP_ACK) {
            // printf ("iputil : MSG_IP_ACK\r\n");
        } 

        else if (rnet_conf->opcode == MSG_IP_REBOOT) {
            if (s_curXid == rnet_conf->xid) {
                setup_Reboot();
                printf("Device reboot request\n");
                s_curXid = 0;
            } else {
                printf("boot fail\n");
                printf("%02x, %02x\n", s_curXid, rnet_conf->xid);
            }
        } 
    }

    close(recv_sock);
    return 0;
}
