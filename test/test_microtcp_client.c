/*
 * microtcp, a lightweight implementation of TCP for teaching,
 * and academic purposes.
 *
 * Copyright (C) 2015-2017  Manolis Surligas <surligas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * You can use this file to write a test microTCP client.
 * This file is already inserted at the build system.
 */

#include <../lib/microtcp.h>
#include <../utils/crc32.h>
#define SERVER_IP "127.0.0.1"

int main(void) {
    struct sockaddr_in server_address;

    /* CREATE SOCKET */
    microtcp_sock_t client_sock = microtcp_socket(AF_INET, SOCK_STREAM, 0);
    printf("SOCKET SD %d\n", client_sock.sd);

    /* BIND SOCKET */
    //struct sockaddr_in sin;
    //memset(&sin, 0, sizeof(struct sockaddr_in));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(54321);
    //server_address.sin_addr.s_addr = inet_addr("192.168.1.25");//htonl(INADDR_ANY);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }

    printf("OK\n");

    /* CONNECT TO SERVER */
    socklen_t address_len = sizeof(struct sockaddr_in);
    if(microtcp_connect(&client_sock, (struct sockaddr*)&server_address, address_len) == 1) {
        perror("COULD NOT CONNECT TO SERVER");
        exit(EXIT_FAILURE);
    }
    printf("POLI OK.\n");
    printf("CONNECTED TO SERVER\n");

    microtcp_shutdown(&client_sock,0);

    return 0;
}
