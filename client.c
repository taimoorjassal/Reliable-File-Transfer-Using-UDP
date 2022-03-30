# include <stdio.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <stdlib.h>
# include <errno.h>
# include <string.h>
# include <arpa/inet.h>
# include <unistd.h>
# include <netinet/in.h>
# include <sys/stat.h>
# include <sys/sendfile.h>
# include <fcntl.h>
# include <time.h>
# include <pthread.h>

# define ADDRESS "127.0.0.1"
# define videoFile "earth.mov"
# define BUFSIZE 500 // Limiting each packet size to 500 bytes
# define segLength 7 // Defining size for 1 segment


// Variables for creating socket
int PORT; 
int _socket;
struct sockaddr_in address;
socklen_t addr_length = sizeof(struct sockaddr_in);


// Required Variables
int sendLength;
int data;
int receivedLength;
int file;
int dataRemaining;
int dataSent;
struct stat fileStat;
off_t fileSize;


struct packet {  //defining structure for one packet
	// each packet has its sequence number, size and an array for data
	int seqNum;
	int size;
	char data[BUFSIZE];
};


// Variables for 1 segment
struct packet packets[segLength]; // array for one segment
int _seqNum = 1;
int _acks;
struct packet ack;
struct packet acks[segLength];
int length = segLength;



//Thread defined to run simultaneously witht the main program
void* receiveAcks(void* variable) {

	// For loop to recieve all packets in a segment
	for (int i = 0; i < length; i++) {
		
		
	//flag to keep receiveing acknowledgments.
	    RECEIVE:
		receivedLength = recvfrom(_socket, &ack, sizeof(struct packet), 0, (struct sockaddr*) & address, &addr_length);
		
		// If duplicate ack was sent
		
		if (acks[ack.seqNum].size == 1) {
			// Receiveing ack again until unique ack is received
			goto RECEIVE; 
		}

		// If valid and unique ack is received
		if (ack.size == 1) {
			fprintf(stdout, "Ack Received: %d of %d\n", ack.seqNum+1, segLength);
			// Allocating ack to right array index
			acks[ack.seqNum] = ack;
			_acks++;
		}

	}
	fprintf(stdout, "\n");
    return NULL;
}



// Main Function
int main(int argc, char* argv[]) {

	// Validating port number from command line
	if (argc < 2) {
		// Port Number needs to be specified on the command line
		perror("Port Number not specified.\nTry ./cli.out 9898\n");
        return 0;

	}

	// Creating port using the number entered by the user
	// atoi() used to convert string to integer
	PORT = atoi(argv[1]);

	// Thread created
	pthread_t thread_id;
        
    	// Variables to introduce time delay
   	// Small time delay is added to compensate for packets on their way.
	struct timespec time1, time2;
	time1.tv_sec = 0;
	time1.tv_nsec = 500000000L; // this adds a delay of 0.05 seconds


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
	inet_pton(AF_INET, ADDRESS, &address.sin_addr);


	// Opening the video and file and giving permission to read only
	file = open(videoFile, O_RDONLY);

	// Verifying File can be read
	if (file < 0) {
		perror("Cannot read File\n");
		return 0;
	}

	// File size
	fstat(file, &fileStat);
	fileSize = fileStat.st_size;
	dataRemaining = (int) fileSize;
	dataSent  = 0;
	fprintf(stdout, "File to be sent iss %d bytes\n",(int) fileSize);
	

	// sending file size
	FILESIZE:
	sendLength = sendto(_socket, &fileSize, sizeof(off_t), 0, (struct sockaddr*) & address, addr_length);
	// Verifying File size has been sent
	if (sendLength < 0) {
		goto FILESIZE;
	}


	data = 1; // initializing data variable
	int segNum = 1;
	
	// While loop until all the data is read
	while (data > 0) {

		fprintf(stdout, "Segment Number: %d\n", segNum++); //printing segmet being sent
		// data is written in packets
		_seqNum = 0;
		for (int i = 0; i < length; i++) {
            	// reading file data
			data = read(file, packets[i].data, BUFSIZE);
            	// Sequence numbers are alloted to each packet
			packets[i].seqNum = _seqNum;
            	// Each packet is alloted a size
			packets[i].size = data;
			_seqNum++;

	    // Last packet is different for identification of end of file on server side
            if (data == 0){ 
                      printf("Whole File is read.\n");
                      // Control value alloted to last packet
                      packets[i].size = -999; 
                      // Updating the counter
                      length = i + 1; 
                      break; 
            }
		}


		// Sending packets of a segment
		for (int i = 0; i < length; i++) {
			fprintf(stdout, "Sending packet: %d of %d\n", packets[i].seqNum+1, segLength);
			sendLength = sendto(_socket, &packets[i], sizeof(struct packet), 0, (struct sockaddr*) & address, addr_length);    
			dataRemaining = dataRemaining - packets[i].size;
			dataSent = dataSent + packets[i].size;        
		}
		fprintf(stdout, "\nData Sent: %d bytes\nData Remaining: %d bytes\n", dataSent, dataRemaining);
		fprintf(stdout, "\n");


		
        // Array reinitialized

		memset(acks, 0, sizeof(acks));
        for (int i = 0; i < length; i++){ acks[i].size = 0;}

		_acks = 0;


		// The thread is created and executed
		pthread_create(&thread_id, NULL, receiveAcks, NULL);
                   
		// Code is halted for some time to allow transmission delay for packet.
		nanosleep(&time1, &time2);

		

		// Resending packets whose acks is missing
		RESEND:
		for (int i = 0; i < length; i++) {


			if (acks[i].size == 0) {

				
                fprintf(stdout,"Resending packet number: %d\n",packets[i].seqNum+1);
                		// resending missing packet
				sendLength = sendto(_socket, &packets[i], sizeof(struct packet), 0, (struct sockaddr*) & address, addr_length);
		
			}
		}

		// Resending packets whose acks is not valid
		if (_acks != length) {
            // Time delay introduced to wait for acks
            nanosleep(&time1, &time2);
			goto RESEND;
		     
		}

		pthread_join(thread_id, NULL);


		// Restart for next segment
		
	}
	fprintf(stdout, "Total %d segmetnts were sent.\n", segNum-1);
	fprintf(stdout, "The File size is %d bytes.\n", (int) fileSize);
	fprintf(stdout,"\n\nFile Sent. Confrim from server side\n\n");

	// Socket closed
	close(_socket);

	return 0;
}
