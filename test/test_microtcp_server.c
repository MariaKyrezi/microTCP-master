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
 * You can use this file to write a test microTCP server.
 * This file is already inserted at the build system.
 */

#include <../lib/microtcp.h>
#include <../utils/crc32.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    /* CREATE SOCKET */
    microtcp_sock_t sock = microtcp_socket(AF_INET, SOCK_STREAM, 0);

    /* CREATE ADDRESS */
    struct sockaddr_in server_address, client_address;
    memset(&sin, 0, sizeof(struct sockaddr_in));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(54321);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    /* BIND SOCKET */
    int bind_ret = microtcp_bind(&sock, (struct sockaddr*)&server_address,sizeof(struct sockaddr_in));
    if(bind_ret == 1){
        perror("TCP BIND");
        exit(EXIT_FAILURE);
    }
    
    if (listen(sock, 3) == -1) {
        perror("Error listening for connections");
        exit(EXIT_FAILURE);
    }
    //sock.state = LISTEN;
    //sleep(1.2);
    
    /* ACCEPT CONNECTION */
    socklen_t address_len = sizeof(struct sockaddr_in);
    int client_sock;
    
    client_sock = microtcp_accept(&sock, (struct sockaddr*)&client_address, address_len);///&
    
    if( client_sock == 1){
        perror("COULD NOT CONNECT TO CLIENT");
        exit(EXIT_FAILURE);
    }
    printf("CONNECTED TO CLIENT\n");
    return 0;
}
