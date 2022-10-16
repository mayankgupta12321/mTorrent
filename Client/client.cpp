// Pre Processors
#include <netinet/in.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <pthread.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <openssl/sha.h>

using namespace std;

/*Define Macros*/
#define LSITEN_QUEUE_LENGTH 10
#define BUFFER_SIZE 524288

#define ERROR(x) cout << "[ERROR] :: " << x << "\n";
// #define SUCCESS(x) cout << "[SUCCESS] :: " << x << "\n";
#define MSG(x) cout << "[+] " << x << "\n";
#define PRINT(x) cout << x << "\n";
#define DEBUG(x) cout << "[DEBUG] :: " << x << "\n";

struct fileMetaDataAtClient {
	string filename;
	string fileFullPath;
	string fileSHA;
	int no_of_chunks_in_file;
	vector<bool> chunks_bitmap;
	int no_of_downloaded_chunks;
};

/* Variable declaration*/
string PEER_LISTEN_IP;
int PEER_LISTEN_PORT;
string TRACKER_FILE_NAME;

string TRACKER_IP = "127.0.0.1";
int TRACKER_PORT = 8080;
int TRACKER_SOCKET;

string myLoginInfo = "";
map<string, fileMetaDataAtClient> mySharableFilesInfo; // filename, metadata

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

// Calcultae SHA of a file.
void calculateSHAOfFile(string filepath, string &sha, int &noOfChunks) {
    int nread;
    char temp_sha[SHA_DIGEST_LENGTH];
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    int fp = open(filepath.c_str(), O_RDONLY);
    char buffer[BUFFER_SIZE] = {0};
    while((nread = read(fp, buffer, sizeof(buffer))) > 0) {
        SHA1_Update(&ctx, buffer, nread);
        noOfChunks++;
        bzero(buffer, sizeof(buffer));
    }
    SHA1_Final((unsigned char *)temp_sha, &ctx);
    if(noOfChunks == 0) sha = "";
    else {
        sha = temp_sha;
        replace(sha.begin(), sha.end(), '\n', '_');
    }
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

int connectMeToTracker() {
	int tracker_fd; // File Descriptor
	int new_socket;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	char buffer[BUFFER_SIZE];

	// Creating socket file descriptor
	if ((new_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		ERROR("SOCKET CREATION FAILURE!!!");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	
	// Attaching IP Address to socket address
	if (inet_pton(AF_INET, TRACKER_IP.c_str(), &address.sin_addr) <= 0) {
		ERROR("INVALID IP ADDRESS!!!");
		exit(EXIT_FAILURE);
	}

	address.sin_port = htons(TRACKER_PORT);

	if((tracker_fd = connect(new_socket, (struct sockaddr*)&address, sizeof(address))) < 0) {
		ERROR("UNABLE TO CONNECT THE TRACKERS!!!");
		exit(EXIT_FAILURE);
	}

	int noOfBytesReceived = recv(new_socket, buffer, sizeof(buffer), 0);
	if (noOfBytesReceived <= 0)
	{
		ERROR("UNABLE TO CONNECT THE TRACKERS!!!");
		exit(EXIT_FAILURE);
	}
	MSG(buffer);

	return new_socket;
}

int main(int argc, char** argv)
{
	PEER_LISTEN_IP = argv[1];
	PEER_LISTEN_PORT = stoi(argv[2]);
	TRACKER_FILE_NAME = argv[3];

	int TRACKER_SOCKET = connectMeToTracker();
	// MSG("Connected to Tracker Successfully.");

	pthread_t thread_listen_to_other_peers;
	pthread_create(&thread_listen_to_other_peers, NULL, &listenToOtherPeers, NULL);


	char buffer[BUFFER_SIZE] = {0};
	
	while(1) {
		bzero(buffer, sizeof(buffer));

		string inputString;
		getline(cin, inputString); // Taking Input from Client.
		
		
		// Tokenising the input string, and storing it in vector.
		vector<string> inputVector;
		stringstream ss(inputString);
    	string token;
    	while (ss >> token) {
			inputVector.push_back(token);
    	}

		int noOfTokensInInputString = inputVector.size();

		// no input - do nothing
		if(noOfTokensInInputString == 0) {
			continue;
		}
		// create_user
		else if(inputVector[0] == "create_user") {
			if(noOfTokensInInputString != 3) {
				ERROR("2 arguments required.")
				continue;
			}

			if(myLoginInfo != "") {
				ERROR("You are currently logged in.\nPlease logout to create user.");
				continue;
			}

			string user_id = inputVector[1];
			string password = inputVector[2];

			string command = "create_user " +  user_id + " " + password;

			send(TRACKER_SOCKET, command.c_str(), command.size(), 0);
			
			recv(TRACKER_SOCKET, buffer, sizeof(buffer), 0);

			PRINT(buffer);
		}

		// login
		else if(inputVector[0] == "login") {
			if(noOfTokensInInputString != 3) {
				ERROR("2 arguments required.")
				continue;
			}

			if(myLoginInfo != "") {
				ERROR("You are currently logged in with user_id : '" + myLoginInfo + "'. Please logout to use another account.");
				continue;
			}
			
			string user_id = inputVector[1];
			string password = inputVector[2];

			string command = "login " +  user_id + " " + password + " " + PEER_LISTEN_IP + " " + to_string(PEER_LISTEN_PORT);

			send(TRACKER_SOCKET, command.c_str(), command.size(), 0);
			recv(TRACKER_SOCKET, buffer, sizeof(buffer), 0);
			string mes = buffer;
			if(mes == "user_logged_in") {
				myLoginInfo = user_id;
				PRINT(mes);
			}
			else {
				PRINT(mes);
			}

		}

		// create_group
		else if(inputVector[0] == "create_group") {
			if(noOfTokensInInputString != 2) {
				ERROR("1 argument required.")
				continue;
			}

			if(myLoginInfo == "") {
				ERROR("User not logged in. Login first");
				continue;
			}

			string group_id = inputVector[1];

			string command = "create_group " + group_id + " " + myLoginInfo;

			send(TRACKER_SOCKET, command.c_str(), command.size(), 0);

			recv(TRACKER_SOCKET, buffer, sizeof(buffer), 0);

			string mes = buffer;

			PRINT(mes);

		}

		// join_group
		else if(inputVector[0] == "join_group") {
			if(noOfTokensInInputString != 2) {
				ERROR("1 argument required.")
				continue;
			}

			if(myLoginInfo == "") {
				ERROR("User not logged in. Login first");
				continue;
			}
		
			string group_id = inputVector[1];

			string command = "join_group " + group_id + " " + myLoginInfo;

			send(TRACKER_SOCKET, command.c_str(), command.size(), 0);

			recv(TRACKER_SOCKET, buffer, sizeof(buffer), 0);

			string mes = buffer;

			PRINT(mes);

		}

		// leave_group
		else if(inputVector[0] == "leave_group") {
			if(noOfTokensInInputString != 2) {
				ERROR("1 argument required.")
				continue;
			}

			if(myLoginInfo == "") {
				ERROR("User not logged in. Login first");
				continue;
			}

			string group_id = inputVector[1];

			string command = "leave_group " + group_id + " " + myLoginInfo;

			send(TRACKER_SOCKET, command.c_str(), command.size(), 0);

			recv(TRACKER_SOCKET, buffer, sizeof(buffer), 0);

			string mes = buffer;

			PRINT(mes);


		}

		// list_requests
		else if(inputVector[0] == "list_requests") {
			if(noOfTokensInInputString != 2) {
				ERROR("1 argument required.")
				continue;
			}

			if(myLoginInfo == "") {
				ERROR("User not logged in. Login first");
				continue;
			}

			string group_id = inputVector[1];

			string command = "list_requests " + group_id + " " + myLoginInfo;

			send(TRACKER_SOCKET, command.c_str(), command.size(), 0);

			recv(TRACKER_SOCKET, buffer, sizeof(buffer), 0);

			string mes = buffer;

			PRINT(mes);

		}

		// accept_request
		else if(inputVector[0] == "accept_request") {
			if(noOfTokensInInputString != 3) {
				ERROR("2 arguments required.")
				continue;
			}

			if(myLoginInfo == "") {
				ERROR("User not logged in. Login first");
				continue;
			}

			string group_id = inputVector[1];
			string pending_member_group_id = inputVector[2];

			string command = "accept_request " + group_id + " " + myLoginInfo + " " + pending_member_group_id;

			send(TRACKER_SOCKET, command.c_str(), command.size(), 0);

			recv(TRACKER_SOCKET, buffer, sizeof(buffer), 0);

			string mes = buffer;

			PRINT(mes);
		}

		// list_groups
		else if(inputVector[0] == "list_groups") {
			if(myLoginInfo == "") {
				ERROR("User not logged in. Login first");
				continue;
			}

			string command = "list_groups";

			send(TRACKER_SOCKET, command.c_str(), command.size(), 0);

			recv(TRACKER_SOCKET, buffer, sizeof(buffer), 0);

			string mes = buffer;

			PRINT(mes);
		}

		// list_files
		else if(inputVector[0] == "list_files") {
			if(noOfTokensInInputString != 2) {
				ERROR("1 argument required.")
				continue;
			}

			if(myLoginInfo == "") {
				ERROR("User not logged in. Login first");
				continue;
			}

			string group_id = inputVector[1];

			string command = "list_files " + group_id + " " + myLoginInfo;

			send(TRACKER_SOCKET, command.c_str(), command.size(), 0);

			recv(TRACKER_SOCKET, buffer, sizeof(buffer), 0);

			string mes = buffer;
			PRINT(mes);
		}

		// upload_file
		else if(inputVector[0] == "upload_file") {
			if(noOfTokensInInputString != 3) {
				ERROR("2 arguments required.")
				continue;
			}

			if(myLoginInfo == "") {
				ERROR("User not logged in. Login first");
				continue;
			}

			string fpath = inputVector[1];
			string group_id = inputVector[2];

			string fsha = "";
			int total_chunks = 0;
			calculateSHAOfFile(fpath, fsha, total_chunks);

			if(total_chunks == 0) {
				ERROR("File doesn't Exists.")
			}
			
			int found = fpath.find_last_of('/');
			string fname = fpath.substr(found + 1 , fpath.size() - found + 1);


			string command = "upload_file " + group_id + " " + myLoginInfo + " " + fsha + " " + fname + " " + to_string(total_chunks);

			send(TRACKER_SOCKET, command.c_str(), command.size(), 0);

			recv(TRACKER_SOCKET, buffer, sizeof(buffer), 0);

			string mes = buffer;

			if(mes == "file_uploaded_successfully") {
				fileMetaDataAtClient f;
				f.filename = fname;
				f.fileFullPath = fpath;
				f.fileSHA = fsha;
				f.no_of_chunks_in_file = total_chunks;
				for(int i = 0; i < total_chunks; i++) {
					f.chunks_bitmap.push_back(1);
				}
				f.no_of_downloaded_chunks = total_chunks; 

				mySharableFilesInfo[fname] = f;

				PRINT("File Uploaded Successfully.")
			}

			else {
				ERROR(mes)
			}
		}

		// download_file
		else if(inputVector[0] == "download_file") {
			if(noOfTokensInInputString != 4) {
				ERROR("3 arguments required.")
				continue;
			}

			if(myLoginInfo == "") {
				ERROR("User not logged in. Login first");
				continue;
			}


		}

		// logout
		else if(inputVector[0] == "logout") {

			if(myLoginInfo == "") {
				ERROR("Login first, then logout.");
				continue;
			}
			
		}

		// show_downloads
		else if(inputVector[0] == "show_downloads") {
			
		}

		// stop_share
		else if(inputVector[0] == "stop_share") {
			if(noOfTokensInInputString != 3) {
				ERROR("2 arguments required.")
				continue;
			}
		}

		else {
			PRINT("Invalid Input.");
		}



		/*--------------------------------------------------------------------*/

		// cout << s << "\n";


		// string IP = "127.0.0.1";

		// int PORT ; 
		// string mes;
		// cin >> PORT >> mes;
		
		// int sock = socket(AF_INET, SOCK_STREAM, 0);
		// int  valread, client_fd;

		// struct sockaddr_in serv_addr;
		// serv_addr.sin_family = AF_INET;
		// serv_addr.sin_port = htons(PORT);

		// // Convert IPv4 and IPv6 addresses from text to binary form
		// if (inet_pton(AF_INET, IP.c_str() , &serv_addr.sin_addr)<= 0) {
		// 	printf(
		// 		"\nInvalid address/ Address not supported \n");
		// 	return -1;
		// }

		// if ((client_fd
		// 	= connect(sock, (struct sockaddr*)&serv_addr,
		// 			sizeof(serv_addr)))
		// 	< 0) {
		// 	printf("\nConnection Failed \n");
		// 	continue;
		// }
		// send(sock, mes.c_str(), mes.size(), 0);
		// bzero(buffer, sizeof(buffer));
		// recv(sock, buffer, 1024, 0);

		// cout << buffer << "\n";
		// close(sock);

	}

	return 0;
}
