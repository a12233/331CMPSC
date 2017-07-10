////////////////////////////////////////////////////////////////////////////////
//
//  File          : cart_client.c
//  Description   : This is the client side of the CART communication protocol.
//
//   Author       : ????
//  Last Modified : ????
//

// Include Files
#include <stdio.h>

// Project Include Files
#include <cart_network.h>
#include <cart_driver.h>
#include <cmpsc311_util.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <cmpsc311_log.h>
//
//  Global data
int client_socket = -1;
int                cart_network_shutdown = 0;   // Flag indicating shutdown
unsigned char     *cart_network_address = NULL; // Address of CART server
unsigned short     cart_network_port = 0;       // Port of CART serve
unsigned long      CartControllerLLevel = LOG_INFO_LEVEL; // Controller log level (global)
unsigned long      CartDriverLLevel = 0;     // Driver log level (global)
unsigned long      CartSimulatorLLevel = 0;  // Driver log level (global)
int socket_handle = -1; 
int sockfd;
//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : client_cart_bus_request
// Description  : This the client operation that sends a request to the CART
//                server process.   It will:
//
//                1) if INIT make a connection to the server
//                2) send any request to the server, returning results
//                3) if CLOSE, will close the connection
//
// Inputs       : reg - the request reqisters for the command
//                buf - the block to be read/written from (READ/WRITE)
// Outputs      : the response structure encoded as needed

CartXferRegister client_cart_bus_request(CartXferRegister reg, void *buf) {

char *buff = (char*)buf; 

uint16_t KY1 = 0;
KY1 = extract_cart_opcode(reg,1);

if(KY1 >= CART_OP_MAXVAL)
	return(-1); 

CartXferRegister value; //opcode to be sent and recieved across network 
char *ip = CART_DEFAULT_IP;
unsigned short port = CART_DEFAULT_PORT;
struct sockaddr_in caddr; 


	if(socket_handle == -1) {//no connection,  create connection , Connect to address CART_DEFAULT_IP and port CART_DEFAULT_PORT.

		//creating socket
		
		sockfd = socket(PF_INET, SOCK_STREAM, 0);
		if (sockfd == -1) {
		 printf( "Error on socket creation [%s]\n", strerror(errno) );
		 return( -1 );
		}
		// Setup the address information
		caddr.sin_family = AF_INET;
		caddr.sin_port = htons(port);
		if ( inet_aton(ip, &caddr.sin_addr) == 0 ) 
			return( -1 );
		 
		//connect	
		if ( connect(sockfd, (const struct sockaddr *)&caddr,sizeof(caddr)) == -1 ) 
			return( -1 );
		socket_handle = 0; 
	}//if

	//decoding opcode
	switch(KY1) {

	case CART_OP_INITMS: 
	case CART_OP_BZERO: 
	case CART_OP_LDCART: 	
				value = htonll64(reg);  //send and receive reg
				int check = write( sockfd, &value, sizeof(value)) ; 
				 if ( check != sizeof(value) ) {
				 	printf( "Error writing network data1 [%s]\n", strerror(errno) );
				 return( -1 );
				 } 
				//printf("reg sent = %lu \n", ntohll64(value)); 
				int check2 = read( sockfd, &value, sizeof(value)); 
				if ( check2 != sizeof(value) ) {
				 printf( "Error reading network data9 [%s]\n", strerror(errno) );
				 return( -1 );
				 }
				 value = ntohll64(value); 
				//printf("reg recieved = %lu  \n", value); 
				break; 
	case CART_OP_RDFRME: 	
				value = htonll64(reg);//rd
				 if ( write( sockfd, &value, sizeof(value)) != sizeof(value) ) {
				 	printf( "Error writing network data2 [%s]\n", strerror(errno) );
				// return( -1 );
				 } 
				//printf("reg sent = %lu \n", ntohll64(value)); 

				if ( read(sockfd, &value, sizeof(value)) != sizeof(value) ) {
				 printf( "Error reading network data8 [%s]\n", strerror(errno) );
				 return( -1 );
				 }
				value = ntohll64(value);
				//printf("reg recieved = %lu \n", value); 
				if ( read( sockfd, buf, 1024) != 1024) {
				 printf( "Error reading network data7 [%s]\n", strerror(errno) );
				 return( -1 );
				 }
				//printf("frame recieved = %s \n", (char*)buf); 
				break; 
	case CART_OP_WRFRME: 	
				value = htonll64(reg);//wr
				 if ( write( sockfd, &value, sizeof(value)) != sizeof(value) ) {
				 	printf( "Error writing network data3 [%s]\n", strerror(errno) );
				 return( -1 );
				 } 
				//printf("reg sent = %lu \n", ntohll64(value)); 
				 if ( write(sockfd, buff, 1024) != 1024 ) {
				 	printf( "Error writing network data4 [%s]\n", strerror(errno) );
				 return( -1 );
				 } 	
				//printf("frame sent = %s \n", (char*)buf); 
				if ( read( sockfd, &value, sizeof(value) )!= sizeof(value) ){
				 printf( "Error reading network data6 [%s]\n", strerror(errno) );
				 return( -1 );
				 }
				value = ntohll64(value);
				//printf("reg recieved = %lu \n", value); 
				break; 
	case CART_OP_POWOFF: 	
				value = htonll64(reg);  //powereoff
				 if ( write( sockfd, &value, sizeof(value)) != sizeof(value) ) {
				 	printf( "Error writing network data5 [%s]\n", strerror(errno) );
				 return( -1 );
				 } 
				//printf("reg sent = %lu \n", ntohll64(value)); 
				if ( read( sockfd, &value, sizeof(value) )!= sizeof(value) ){
				 printf( "Error reading network data5 [%s]\n", strerror(errno) );
				 return( -1 );
				 }
				value = ntohll64(value); 
				//printf("reg recieved = %lu \n", value); 
				close( sockfd );
				socket_handle = -1;
				break; 
 

	default: return (-1); //not valid opcode

	}

  
     
   
//value = ntohll64(value);
return (value);
}










