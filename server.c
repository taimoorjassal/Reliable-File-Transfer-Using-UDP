# include <stdio.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <stdlib.h>
# include <errno.h>
# include <string.h>
# include <arpa/inet.h>
# include <unistd.h>
# include <netinet/in.h>
# include <fcntl.h>
#include <pthread.h>
# include <fcntl.h>
# include <time.h>


# define recVideoFile "recieved.mov"
# define BUFSIZE 500 // Limiting each packet size to 500 bytes
#define segLength 7 // Defining size for 1 segment


// Variables for creating socket
int PORT;
int _bind;
int _socket;
struct sockaddr_in address;
socklen_t addr_length = sizeof(struct sockaddr_in);

// Required Variables
int receivedLength = 0;
int sendLength = 0;
int file;
int fileSize;
int dataRemaining = 0;
int dataReceived = 0;



struct packet {  //defining structure for one packet
	// each packet has its sequence number, size and an array for data
	int seqNum;
	int size;
	char data[BUFSIZE];
};


// Variables for 1 segment

int length = segLength; // Number of packets to be sent at a time
struct packet _packet;
struct packet packets[segLength];
int _acks;
struct packet acks[segLength];
int num;




//Thread defined to run simultaneously witht the main program
void* receiveSegments(void *variable) {

	// For loop to recieve all packets in a segment
	for (int i = 0; i < length; i++) {
	//flag to keep receiveing segments.
        RECEIVE:
		receivedLength = recvfrom(_socket, &_packet, sizeof(struct packet), 0, (struct sockaddr*) & address, &addr_length);

		// If duplicate packet was sent

        if (packets[_packet.seqNum].size != 0 ){ 
            // Reinitializing array
            packets[_packet.seqNum] = _packet; 
			num = _packet.seqNum;
			acks[num].size = 1;
			acks[num].seqNum = packets[num].seqNum;
			// Sending ack
			sendLength = sendto(_socket, &acks[num], sizeof(acks[num]), 0, (struct sockaddr*) & address, addr_length);
			fprintf(stdout,"Duplicate Ack Sent:%d of %d",acks[num].seqNum+1, segLength);

			// Keep returning to Receive flag to ensure all packets are received
			goto RECEIVE;
		}

		// If last packet is recieved
		// if statement to check for size alloted to the last packet
		if (_packet.size == -999) {
			printf("Last Packet Received\n");
			// Counter is updated
			length = _packet.seqNum + 1;
		}

		// Any othe packet received 
		if (receivedLength > 0) {
			fprintf(stdout, "Receiving Packet Number:%d of %d\n", _packet.seqNum+1, segLength);
			// Allocating Packet to right array index
			packets[_packet.seqNum] = _packet;
		}
              
	}
	fprintf(stdout, "\n");
	return NULL;
}




// Main Function
int main(int argc, char* argv[]) {
	// Validating port number from command line
	if (argc < 2) {
		perror("Please enter valid port number.\n");
                return 0;

	}

	// Creating port using the number entered by the user
	// atoi() used to convert string to integer
	PORT = atoi(argv[1]);

   	// Variables to introduce time delay
   	// Small time delay is added to compensate for packets on their way.
	struct timespec time1, time2;
	time1.tv_sec = 0;
	time1.tv_nsec = 50000000L; // this adds a delay of 0.05 seconds
    
	// Thread created
	pthread_t thread_id;

	// Socket Created
	_socket = socket(AF_INET, SOCK_DGRAM, 0);


	// Verifiying Socket creation
	if (_socket < 0) {
		perror("Cannot create socket./n");
		return 0;
	}

	// Updating values for address 
	memset((char*)& address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(PORT);

	// Socket Binding
	_bind = bind(_socket, (struct sockaddr*) & address, sizeof(address));

	// Verifying socket binding
	if (_bind < 0) {
		perror("Could not bind\n");
		return 0;
	}

	// Creating new file with all required permissions for Read and Write
	file = open(recVideoFile, O_RDWR | O_CREAT, 0755);

	
	// Receiving File size from client
	receivedLength = recvfrom(_socket, &fileSize, sizeof(off_t), 0, (struct sockaddr*) & address, &addr_length);
	fprintf(stdout, "The file to be received is %d bytes\n", fileSize);

	// declaring and initializing more required variables
	int segNum = 1;
	receivedLength = 1;
	dataRemaining = fileSize;
	
	// main while loop to recieve all the packets.
	while (dataRemaining > 0 || (length == 7)) {

	fprintf(stdout, "Segment Number: %d", segNum++); //printing segmet being sent		

	// Initializing the size of packets to 0
		memset(packets, 0, sizeof(packets));
        for (int i = 0; i < length; i++){ packets[i].size = 0; }

	// Initializing the size of acks to 0
        memset(acks, 0, sizeof(packets));
        for (int i = 0; i < length; i++){ acks[i].size = 0; }



               
        // The thread is created and executed
		pthread_create(&thread_id, NULL, receiveSegments, NULL);

        // Code is halted for some time to allow transmission delay for packet.
        nanosleep(&time1, &time2);

		_acks = 0;

		//Sendng Acks for received packets

		RESEND_ACK:
		for (int i = 0; i < length; i++) {
			num = packets[i].seqNum;
			
			if (acks[num].size != 1 ) {
				// Generating acks
				if (packets[i].size != 0) {
					// assigning required variable names ot acks
					acks[num].size = 1;
					acks[num].seqNum = packets[i].seqNum;

					// Acks sent to client
					sendLength = sendto(_socket, &acks[num], sizeof(acks[num]), 0, (struct sockaddr*) & address, addr_length);
					if (sendLength > 0) {
						_acks++;
						fprintf(stdout, "Ack sent: %d of %d\n", acks[packets[num].seqNum].seqNum+1, segLength);
					}
				}
			}
		}
		fprintf(stdout, "\n");

		// Adding time delay
		nanosleep(&time1, &time2);
		nanosleep(&time1, &time2);

		// If packets are not recieved
		if (_acks < length) {
			goto RESEND_ACK;
		}
                
		pthread_join(thread_id, NULL);
                 
		// Writing packet data to the file
		for (int i = 0; i < length; i++) {
			// Verifying packte is not empty and is not the last packet
			if (packets[i].size != 0 && packets[i].size !=-999)
			{
				fprintf(stdout, "Writing Packet number: %d of %d\n", packets[i].seqNum+1, segLength);
				// Writing data to the file
				write(file, packets[i].data, packets[i].size);
				// Calculating remaining and received data
				dataRemaining = dataRemaining - packets[i].size;
				dataReceived = dataReceived + packets[i].size;
				
			}
		}

		// Printing required information to the user
		fprintf(stdout, "\nData Recieved: %d bytes\nData Remaining: %d bytes\n", dataReceived, dataRemaining);
		fprintf(stdout, "\n\n");

		// Restart for next segment
	}
	// Priinting required info after all segments are received
	fprintf(stdout, "Total %d segmetnts recieved", segNum-1);
	fprintf(stdout, "\n\n File Recieved.\nThe file name is %s\nPlease confirm from destination folder\n\n", recVideoFile);

    // socket closed
    close(_socket);
    return 0;


}
