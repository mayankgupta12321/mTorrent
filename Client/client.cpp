// Pre Processors
#include <netinet/in.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <pthread.h>
#include <dirent.h>

using namespace std;

/*Define Macros*/
#define LSITEN_QUEUE_LENGTH 10
#define BUFFER_SIZE 1024

#define ERROR(x) cout << "[ERROR] :: " << x << "\n";
#define MSG(x) cout << "[+] " << x << "\n";
#define DEBUG(x) cout << "[DEBUG] :: " << x << "\n";

/* Variable declaration*/
string PEER_LISTEN_IP;
int PEER_LISTEN_PORT;
string TRACKER_FILE_NAME;

/*--------------------------------*/


bool searchFile(string filename) {
	struct dirent *de;
	string path = "./";
    DIR *dr = opendir(path.c_str());
    if (dr != NULL) {
        while ((de = readdir(dr)) != NULL) {
        	if(de->d_name == filename) {
        	    return true;
        	}
        }
    }
    closedir(dr);
	return false;
}

void *processOtherPeersRequest(void *arg)
{
	int new_socket = long(arg);
	char buffer[BUFFER_SIZE];
	bzero(buffer, sizeof(buffer));

	int noOfBytesReceived = recv(new_socket, buffer, sizeof(buffer), 0);
	if (noOfBytesReceived <= 0)
	{
		pthread_exit(NULL);
	}

	/* PROCESS OTHER PEER REQUEST */
	string filename = buffer;
	string message = to_string(PEER_LISTEN_PORT);

	if(searchFile(filename)) {
		message += " : " + filename + " : File Found";
	}
	else {
		message += " : " + filename + " : File Not Found";
	}

	send(new_socket, message.c_str(), message.size(), 0);

	/*--------------------------------------*/

	cout << "Peer Requested processed successfully.\n";

	close(new_socket);
	pthread_exit(NULL);
}

void *listenToOtherPeers(void *arg) {
	
	int server_fd; // File Descriptor
	int new_socket; // 

	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	char buffer[1024] = {0};

	// Creating socket file descriptor
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		ERROR("SOCKET CREATION FAILURE!!!");
		exit(EXIT_FAILURE);
	}

	// Setting setsocketopt to REUSE IP, PORT.
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		ERROR("SETSOCKETOPT FAILURE!!!");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	
	// Attaching IP Address to socket address
	if (inet_pton(AF_INET, PEER_LISTEN_IP.c_str(), &address.sin_addr) <= 0) {
		ERROR("INVALID IP ADDRESS!!!");
		exit(EXIT_FAILURE);
	}
	
	address.sin_port = htons(PEER_LISTEN_PORT);

	// Forcefully attaching socket to the LISTEN PORT.
	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		ERROR("CONNECTION FAILURE!!!");
		exit(EXIT_FAILURE);
	}

	if (listen(server_fd, LSITEN_QUEUE_LENGTH) < 0)
	{
		ERROR("SOME ERROR WHILE LISTENING!!!");
		exit(EXIT_FAILURE);
	}

	MSG("Peer is up for listening at " + PEER_LISTEN_IP + ":" + to_string(PEER_LISTEN_PORT));

	while (1)
	{
		if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
		{
			ERROR("NOT ABLE TO ACCEPT REQUEST");
			exit(EXIT_FAILURE);
		}

		DEBUG((string)inet_ntoa(address.sin_addr) + " - " + to_string(address.sin_port) + " - " + to_string(ntohs(address.sin_port)));

		pthread_t thread_to_process_other_peers_request;
		pthread_create(&thread_to_process_other_peers_request, NULL, &processOtherPeersRequest, (void *)new_socket);
	}
	
	// closing the listening socket
	shutdown(server_fd, SHUT_RDWR);
}

int main(int argc, char** argv)
{

	// ERROR(2);
	// return 0;
	
	PEER_LISTEN_IP = argv[1];
	PEER_LISTEN_PORT = stoi(argv[2]);
	TRACKER_FILE_NAME = argv[3];

	// cout << PEER_LISTEN_IP << " : " << PEER_LISTEN_PORT << " : " << TRACKER_FILE_NAME << "\n";

	pthread_t thread_listen_to_other_peers;
	pthread_create(&thread_listen_to_other_peers, NULL, &listenToOtherPeers, NULL);




	// while(1) {};

	char buffer[1024] = { 0 };
	
	while(1) {
		string IP = "127.0.0.1";

		int PORT ; 
		// cin >> PORT;
		string mes;
		// cout << "Enter the <PORT> <MESSAGE> : ";
		cin >> PORT >> mes;
		// cout << "PORT is : " << PORT << " : " << htons(PORT) << "\n";
		
		// cout << "\n";

		int sock = socket(AF_INET, SOCK_STREAM, 0);
		int  valread, client_fd;

		struct sockaddr_in serv_addr;
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(PORT);

		// Convert IPv4 and IPv6 addresses from text to binary form
		if (inet_pton(AF_INET, IP.c_str() , &serv_addr.sin_addr)<= 0) {
			printf(
				"\nInvalid address/ Address not supported \n");
			return -1;
		}

		if ((client_fd
			= connect(sock, (struct sockaddr*)&serv_addr,
					sizeof(serv_addr)))
			< 0) {
			printf("\nConnection Failed \n");
			continue;
			// return -1;
		}
		// cout << "$$$\n";
		// pthread_t ptid;
		// pthread_create(&ptid, NULL, &readFromServer, (void *)sock);
		send(sock, mes.c_str(), mes.size(), 0);
		bzero(buffer, sizeof(buffer));
		recv(sock, buffer, 1024, 0);

		cout << buffer << "\n";
		close(sock);

	}

	return 0;
}
