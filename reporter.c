#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include "file_helper.h"

/**
 * Helper function for getting IP address 
 */
char** split (int * const size, const char* const src, const char delimiter) {
    if (src == NULL || strlen (src) == 0) {
        *size = 0;
        return NULL;
    }
    else {
        int cnt = 1;
        int len = strlen (src);
        for ( int i = 0; i < strlen (src); ++ i) {
            if ( src[i] == delimiter && i < strlen (src) - 1) {
                cnt += 1;
            }
        }

        char** substrings = (char **) malloc (cnt * sizeof (char *));
        int start = 0, increment = 0, idx = 0;
        for (; start + increment <= len; ) {
            if (src[start + (increment)] == delimiter || src[start + increment] == '\0') {
                increment ++;
                char* tmp = (char *) malloc (sizeof (char) * increment);
                if ( tmp ) {
                    strncpy (tmp, src + start, increment -1);
                    tmp[increment-1] = '\0';
                    start += increment;
                    increment = 0;
                    substrings[idx++] = tmp;
                }
            }
            else {
                increment ++;
            }
        }
        *size = cnt;
        return substrings;
    }
}

/**
 * This method get the IP address of th specified ifname. If no address exists for this  ifname, the method should return NULL
 */
char* get_if_address (const char* const peer_ipv4_addr, const char* const ifname) {
    struct ifaddrs *ifaddr, *ifa;
    int family, s, n;
    char host[NI_MAXHOST];
    if (getifaddrs (&ifaddr) == -1) {
        perror ("getifaddrs: ");
        return NULL;
    }

    char* local_ip = NULL;
    int decimals_cnt = 0;
    char** decimals = split (&decimals_cnt, peer_ipv4_addr, '.');
    for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n ++) {
        if ( ifa->ifa_addr == NULL) {
            continue;
        }
        family = ifa->ifa_addr->sa_family;
        if (family == AF_INET) {
            s = getnameinfo (ifa->ifa_addr, sizeof (struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if ( s != 0) {
                continue;
            }
            if ( ifname ) {
                if (strcmp (ifname, ifa->ifa_name) == 0) {
                    local_ip = (char *) malloc (strlen (host) + 1);
                    strcpy (local_ip, host);
                    break;
                }
                continue;
            }
            int if_decimals_cnt = 0;
            char** if_decimals = split (&if_decimals_cnt, host, '.');
            if ( if_decimals_cnt == decimals_cnt && decimals_cnt == 4) {
                if (strcmp (decimals[0], if_decimals[0]) == 0 &&
                        strcmp (decimals[1], if_decimals[1]) == 0 &&
                        strcmp (decimals[2], if_decimals[2]) == 0
                   ) {
                    local_ip = (char *) malloc (strlen (host) + 1);
                    strcpy (local_ip, host);

                    for (int i = 0; i < if_decimals_cnt; ++ i) {
                        free (if_decimals[i]);
                    }
                    free (if_decimals);

                    break;
                }
            }
        }
    }
    if (ifname == NULL) {
        for (int i = 0; i < decimals_cnt; ++ i) {
            free (decimals[i]);
        }
        free (decimals);
    }
    return local_ip;
}


/**
 * Helper function to create a UDP socket, without Socket options set.
 */
int create_udp_socket(const char* const ifname, const unsigned short port) {
    int sockfd;
    struct sockaddr_in selfaddr;
    sockfd = socket (AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        fprintf (stderr, "Cannot create service socket, quit thread!\n");
        return -1;
    }
    bzero (&selfaddr, sizeof (selfaddr));
    selfaddr.sin_family = AF_INET;
    selfaddr.sin_addr.s_addr = htonl (INADDR_ANY);
    char* self_ip_to_use = get_if_address (NULL, ifname);
    if (self_ip_to_use) {
        struct in_addr self_in;
        if (inet_aton (self_ip_to_use, &self_in) == 0 ) {
            fprintf (stderr, "Cannot convert broadcast address %s to its binary form, quit thread!\n", self_ip_to_use);
            return -1;
        }
        selfaddr.sin_addr.s_addr = self_in.s_addr;
        free (self_ip_to_use);
    }
    selfaddr.sin_port = htons (port);

    if (bind (sockfd, (const struct sockaddr *)&selfaddr, sizeof (selfaddr)) == -1 ) {
        fprintf (stderr, "Cannot bind socket file descriptor, quit thread!\n");
        return -1;
    }
    return sockfd;
}


int main (int argc, char* argv[]) {
    int sockfd = -1; 
    while (sockfd == -1) {
        sockfd = create_udp_socket ("wlan0", 12000);
        if (sockfd == -1) {
            fprintf(stderr, "Unable to create socket... Will try in 5 seconds.\n");
            sleep (5);		
        }
    }
    struct sockaddr_in peeraddr;
    const int on = 1; 
    // we always send broadcast messages
    if (setsockopt (sockfd, SOL_SOCKET, SO_BROADCAST, &on, sizeof (on)) == -1 ) {
        fprintf(stderr, "Set socket option to broadcast failed. quit thread!\n");
    }
    bzero (&peeraddr, sizeof(peeraddr));
    peeraddr.sin_family = AF_INET;
    struct in_addr in;
    //AP of CVGW is on 192.168.5.x/24 subnet.
    if (inet_aton("192.168.5.255", &in) == 0 ) {
        fprintf (stderr, "Cannot convert broadcast address 192.168.5.255 to its binary form, quit\n");
        exit(-1);
    }
    peeraddr.sin_addr.s_addr = in.s_addr;
    peeraddr.sin_port = htons (12000); //predefined port number
    char subfolders[1024][50]; // at most can scan 1024 folders in /home/pi/hzlogger folder.

    while (1) {
        //I did not check if self_ip_to_use is null or not because it will not be null. The condition has been checked by function call create_udp_socket().
        char* self_ip_to_use = get_if_address (NULL, "wlan0");
        //sendto (sockfd, self_ip_to_use, strlen (self_ip_to_use)+1, 0, (const struct sockaddr *) &peeraddr, sizeof (peeraddr));
        sleep (3);
        int cnt = 0;
        read_files ("/home/pi/hzlogger", subfolders, &cnt);
        if (cnt > 0) {
            for (int i = 0; i < cnt; ++ i) {
                char path[1024];
                strcpy (path, "/home/pi/hzlogger");
                strcat (path, "/");
                strcat (path, subfolders[i]);
                long long file_cnt  = count_regualr_files (path);
                char msg[1024];
                sprintf (msg, "[%s] folder %s contains %lld files", self_ip_to_use, path, file_cnt);
                sendto (sockfd, msg, strlen (msg)+1, 0, (const struct sockaddr *) &peeraddr, sizeof (peeraddr));
            }
        }
        free (self_ip_to_use);

    }

    return 0;
}
