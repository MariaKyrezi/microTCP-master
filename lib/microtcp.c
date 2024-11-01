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

#include "microtcp.h"
#include "../utils/crc32.h"


/* timeouts , address */
microtcp_sock_t microtcp_socket (int domain, int type, int protocol)
{
  microtcp_sock_t my_sock;

  if((my_sock.sd = socket (domain, type, protocol )) == -1) { // !
    perror ( " SOCKET COULD NOT BE OPENED " );
    exit (EXIT_FAILURE);
  }

  my_sock.ack_number = 0;
  my_sock.seq_number = 0;
  my_sock.state = CLOSED;
  my_sock.bytes_lost = 0;
  my_sock.bytes_received = 0;
  my_sock.bytes_send = 0;
  my_sock.cwnd = MICROTCP_INIT_CWND;
  my_sock.ssthresh = MICROTCP_INIT_SSTHRESH;
  my_sock.packets_lost = 0;
  my_sock.packets_received = 0;
  my_sock.packets_send = 0;
  my_sock.buf_fill_level = 0;
  my_sock.recvbuf = NULL; /**< The *receive* buffer of the TCP
                                    connection. It is allocated during the connection establishment and
                                     is freed at the shutdown of the connection. This buffer is used
                                     to retrieve the data from the network. */
  my_sock.curr_win_size = MICROTCP_WIN_SIZE;
  my_sock.init_win_size = MICROTCP_WIN_SIZE;
  

  return my_sock;
}

int microtcp_bind (microtcp_sock_t *socket, const struct sockaddr *address, socklen_t address_len)
{
  if(socket->state != CLOSED) exit(EXIT_FAILURE);

  if(bind(socket->sd, address, address_len) == -1){
    perror("TCP BIND");
    exit(EXIT_FAILURE);
  }

  socket->state = LISTEN;
  return EXIT_SUCCESS;
}

int microtcp_connect (microtcp_sock_t *socket, const struct sockaddr *address, socklen_t address_len)
{

  microtcp_header_t send_packet, recv_packet; 
  size_t sent_bytes,recv_bytes;
  struct timeval timeout; 

  
  if (socket->state != LISTEN && socket->state != CLOSED) return EXIT_FAILURE;
  srand(time(NULL));

  memset(&send_packet, 0, sizeof(microtcp_header_t)); // initiallize bytes of the structure to zero

  //  Set sequence number
  socket->seq_number = rand() % 100;

  /* SEND SYN , seq = N */
  send_packet.control |= htons(1 << 1);  //set as 1 the second bit from right to left(SYN) 
  send_packet.seq_number = htonl(socket->seq_number);
  send_packet.window = htons(MICROTCP_WIN_SIZE); 
  send_packet.checksum = crc32 ((const uint8_t *)&send_packet, sizeof(microtcp_header_t));
  
  /* SEND PACKET USING  RECIEVE THE NUMBER OF BYTES SENT (send_bytes)*/

  sent_bytes = sendto(socket->sd, (const void *)&send_packet, sizeof(microtcp_header_t), 0, address, address_len); // do not modify the values of send_packet
  if(sent_bytes < 0){
    perror("ERROR! 3-way handshake: SYN.\n");
    return EXIT_FAILURE;
  }

  socket->packets_send++; // increase the number of sent packets
  socket->bytes_send += sent_bytes;
  socket->seq_number++; //increase the sequence number of the socket by 1

  /****************************
   *  CHECK FOR TIMEOUT EVENT
  *****************************/

  /*timeout.tv_sec = 0;
  timeout.tv_usec = MICROTCP_ACK_TIMEOUT_US;
  if (setsockopt(socket->sd, SOL_SOCKET, SO_RCVTIMEO, &timeout,sizeof(struct timeval)) < 0) { // Sets the receive timeout option (SO_RCVTIMEO) for the socket.
    perror("ERROR! 3-way handshake: setsockopt.\n");
    return EXIT_FAILURE;
  }*/

  /* RECEIVE SYN, ACK, seq = M, ack = N+1 */
  if ((recv_bytes = recvfrom(socket->sd, &recv_packet, sizeof(microtcp_header_t), 0, (struct sockaddr *)address, &address_len)) < 0){ // timeout event
    perror("ERROR! SYN ACK failed.\n");
    return EXIT_FAILURE;
  }  

  /* the packet was received before the timeout, stop timer 
  timeout.tv_usec = 0;
  if (setsockopt(socket->sd, SOL_SOCKET, SO_RCVTIMEO, &timeout,sizeof(struct timeval)) < 0) { // Sets the receive timeout option (SO_RCVTIMEO) for the socket.
    perror("ERROR! 3-way handshake: setsockopt.\n");
    return EXIT_FAILURE;
  }*/
  
  socket->packets_received++;
  socket->bytes_received += recv_bytes;

  /* CHECK FOR THE VALUES RECEIVED IF THEY ARE CORRECT (SYN bit == 1, ACK bit == 1, sequence number, ack, (checksum?))*/

  if (ntohs(recv_packet.control) != (1 << 3)) { // ACK
    perror("ERROR! 3-way handshake : Control field: ACK.\n");
    recv_packet.control = htons(1 << 2);    //SET RST AS 1
    return EXIT_FAILURE;
  }

  if (ntohs(recv_packet.control) != (1 << 1)) { // SYN
    perror("ERROR! 3-way handshake : Control field: SYN.\n");
    recv_packet.control = htons(1 << 2);    //SET RST AS 1
    return EXIT_FAILURE;
  }

  /* Check if the ACK is received is different than the sequence number sent */
  if((size_t)(ntohl(recv_packet.ack_number)) != socket->seq_number){  // sequence number has beeen previously increased by one 
    perror("ERROR! 3-way handshake: received ACK.\n");
    recv_packet.control = htons(1 << 2);    //SET RST AS 1
    return EXIT_FAILURE;
  }

  if(send_packet.checksum != ntohl(recv_packet.checksum)){
    perror("ERROR! 3-way handshake: Checksum.\n");
    recv_packet.control = htons(1 << 2);    //SET RST AS 1
    return EXIT_FAILURE;
  }

  /* UPDATE SOCKET*/
  socket->seq_number = ntohl(recv_packet.ack_number);
  socket->ack_number = ntohl(recv_packet.seq_number) + 1;
  socket->init_win_size = ntohl(recv_packet.window);  // The window size negotiated at the 3-way handshake

  /* SEND ACK, seq = N+1, ack = M+1 */
  send_packet.control &= ~htons(1 << 1); //change the value of the SYN bit from 1 to 0 (ACK bit still has the value 1)
  send_packet.seq_number = htonl(socket->seq_number);
  send_packet.ack_number = htonl(socket->ack_number);

  sent_bytes = sendto(socket->sd, &send_packet, sizeof(microtcp_header_t), 0, address, address_len);
  if(sent_bytes < 0){
    perror("ERROR! 3-way handshake: ACK.\n");
    return EXIT_FAILURE;
  }

  socket->packets_send++;
  socket->bytes_send += sent_bytes;

  socket->recvbuf = (uint8_t *)malloc(sizeof(uint8_t) * MICROTCP_RECVBUF_LEN); /*The receive buffer of the TCP connection is allocated during the connection establishment*/
  socket->state = ESTABLISHED;

  printf("-------- CONNECTION ESTABLISHED --------\n");
  return 0;

}

int microtcp_accept (microtcp_sock_t *socket, struct sockaddr *address, socklen_t address_len)
{
  microtcp_header_t send_packet, recv_packet; 
  size_t sent_bytes,recv_bytes;
  struct timeval timeout; 

  // Check if the socket is in an invalid state
  if (socket->state != LISTEN){
    perror("CANNOT CONNECT: WRONG STATE.\n");
    return EXIT_FAILURE;
  }

  srand(time(NULL));
  memset(&send_packet, 0, sizeof(microtcp_header_t)); // initiallize bytes of the structure to zero

  recv_bytes = recvfrom(socket->sd, &recv_packet, sizeof(microtcp_header_t), 0, (struct sockaddr *)address, &address_len);
  
  if(recv_bytes < 0){
      perror("ERROR! 3-way handshake : Control field: SYN.\n");
      return EXIT_FAILURE; 
  }

  socket->bytes_received += recv_bytes;
  socket->packets_received++;


  if (ntohs(recv_packet.control) != (1 << 1)) { // SYN
      perror("ERROR! 3-way handshake : Control field: SYN.\n");
      recv_packet.control = htons(1 << 2);    //SET RST AS 1
      return EXIT_FAILURE;
  }
  
  /* UPDATE SOCKET*/
  socket->seq_number = rand() % 100;
  socket->ack_number = ntohl(recv_packet.seq_number) + 1;
  socket->curr_win_size = ntohs(recv_packet.window);  // The window size negotiated at the 3-way handshake //!!

  /* CREATE PACKT. SEND SYN, ACK, seq = M, ack = N+1 */
  send_packet.control |= htons(1 << 1); // change the value of the SYN bit 
  send_packet.control |= htons(1 << 3);   // ACK
  send_packet.seq_number = htonl(socket->seq_number);
  send_packet.ack_number = htonl(socket->ack_number);

  sent_bytes = sendto(socket->sd, (const void *)&send_packet, sizeof(microtcp_header_t), 0, address, address_len); // do not modify the values of send_packet

  if(sent_bytes < 0){
    perror("ERROR! 3-way handshake: SYN-ACK.\n");
    return EXIT_FAILURE;
  }

  socket->bytes_send += sent_bytes;
  socket->packets_send++;

  /****************************
  *  CHECK FOR TIMEOUT EVENT
  *****************************/

  recv_bytes =  recvfrom(socket->sd, &recv_packet, sizeof(microtcp_header_t), 0, (struct sockaddr *)address, &address_len);

  if (recv_bytes < 0){
    perror("ERROR! 3-way handshake: ACK.\n");
    return EXIT_FAILURE;
  }

  if (ntohs(recv_packet.control) != (1 << 3)) { // ACK
    perror("ERROR! 3-way handshake : Control field: ACK.\n");
    recv_packet.control = htons(1 << 2);    //SET RST AS 1
    return EXIT_FAILURE;
  }

  if((socket->seq_number + 1) != (size_t)ntohs(recv_packet.ack_number) && socket->ack_number != (size_t)ntohs(recv_packet.seq_number)){
    perror("ERROR! 3-way handshake : Control field: ACK.\n");
    return EXIT_FAILURE;
  }

  socket->recvbuf = (uint8_t *)malloc(sizeof(uint8_t) * MICROTCP_RECVBUF_LEN);
  printf("-------- CONNECTION ESTABLISHED --------\n");
  socket->state = ESTABLISHED;

  return 0;
}

int microtcp_shutdown (microtcp_sock_t *socket, int how)
{
  microtcp_header_t receive_header, send_header;
  struct sockaddr_in addr;   
  socklen_t addr_len; 
  ssize_t sent,received;
  int result_addr;

  if(socket->state != ESTABLISHED){
    perror("SHUTDOWN : Invalid connection state for termination.\n");
    return EXIT_FAILURE;
  }

  /* client sends server a FIN */

  memset(&send_header, 0, sizeof(microtcp_header_t));

  send_header.control |= htons(1<<0); // FIN  
  send_header.control |= htons(1<<3); // ACK
  send_header.seq_number = htonl(socket -> seq_number);
  
  result_addr = getsockname(socket->sd, (struct sockaddr *)&addr, &addr_len);
  
  if (result_addr == -1){
    perror("SHUTDOWN : Error acquiring the socket address!\n");
    return EXIT_FAILURE;
  }

  sent = sendto(socket->sd, &send_header, sizeof(microtcp_header_t), 0, (struct sockaddr *)&addr, addr_len);
  
  if(sent < 0){
    perror("SHUTDOWN : Error sending FIN to server\n");
    return EXIT_FAILURE;
  }

  socket->packets_send++;
  socket->bytes_send += sent;

  /* server responds with an ACK*/

  received = recvfrom(socket->sd, (void *)&receive_header, sizeof(microtcp_header_t), 0, (struct sockaddr *)&addr, &addr_len);
  
  if(received < 0){
    perror("SHUTDOWN : Error receiving ACK from server\n");
    return EXIT_FAILURE;
  }

  socket->packets_received++;
  socket->bytes_received += received;
  socket->ack_number = ntohl(receive_header.ack_number);
  
  /* server makes state CLOSING BY PEER*/

  socket->state = CLOSING_BY_PEER;

  /* When client receives the ACK*/
  if(ntohs(receive_header.control) == (1<<3)){ //== ACK
    /* client makes state CLOSING BY HOST*/
    socket->state = CLOSING_BY_HOST;

  }else{
    perror("SHUTDOWN : Error when receiving ACK\n");
    return EXIT_FAILURE;
  }
  
  /* server finishes all the remaining processes beafore termination*/

  /* server sends a FIN to client*/

  received = recvfrom(socket->sd, (void *)&receive_header, sizeof(microtcp_header_t), 0, (struct sockaddr *)&addr, &addr_len);

  if(received < 0){
    perror("SHUTDOWN : Error receiving FIN from server\n");
    return EXIT_FAILURE;
  }

  socket->packets_received++;
  socket->bytes_received += received;

  /* When client receives FIN */

  if(ntohs(receive_header.control) == (1<<0)){

    /* client responds with an ACK*/

    send_header.seq_number = htonl(socket->ack_number); // SEQ = N + 1 where previous ACK = N + 1
    send_header.ack_number = htonl(ntohl(receive_header.seq_number) + 1); // ACK = SEQ + 1 where SEQ is this procedures sequence number
    
    socket->seq_number = socket->ack_number;
    socket->ack_number = ntohl(send_header.ack_number);
    send_header.control &= ~htons(1<<0); //UNSET FIN
    send_header.control |= htons(1<<3); //SET ACK

    sent = sendto(socket->sd, &send_header, sizeof(microtcp_header_t), 0, (struct sockaddr *)&addr, addr_len);
    
    if(sent < 0){
      perror("SHUTDOWN : Error sending FIN to server\n");
      return EXIT_FAILURE;
    }

    socket->packets_send++;
    socket->bytes_send += sent;

  }else{
    perror("SHUTDOWN : Error when client receives FIN\n");
    return EXIT_FAILURE;
  }
  
  socket->state = CLOSED;
  free(socket->recvbuf);
  printf("\n-------------------------------------SHUTDOWN completed-----------------------------\n");
  return EXIT_SUCCESS;
}

ssize_t microtcp_send (microtcp_sock_t *socket, const void *buffer, size_t length, int flags)
{
  /* Your code here */
}

ssize_t microtcp_recv (microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
  /* Your code here */
}
