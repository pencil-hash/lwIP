#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#define NO_SYS                      1   /* 不使用操作系统 */
#define LWIP_NETCONN                0   /* 禁用 Netconn API（只用 Raw API） */
#define LWIP_SOCKET                 0   /* 禁用 Socket API */
#define LWIP_IPV4                   1   /* 启用 IPv4 */
#define LWIP_ICMP                   1   /* 启用 ICMP（用于 ping） */
#define MEM_ALIGNMENT               4   /* 内存对齐 */

#endif

/* 禁用所有应用层模块，避免编译错误 */
#define LWIP_HTTPD               0
#define LWIP_SNMP                0
#define LWIP_MDNS_RESPONDER      0
#define LWIP_LWIPERF             0
#define LWIP_TFTP                0
#define LWIP_MQTT                0
#define LWIP_PPP                 0
#define LWIP_TCP                    1
#define LWIP_ICMP                   1
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
