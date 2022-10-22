// Pre Processors
#include <netinet/in.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <pthread.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <cstdlib>

using namespace std;

/*Define Macros*/
#define LSITEN_QUEUE_LENGTH 10
#define BUFFER_SIZE 524288
#define CHOTA_CHUNK_SIZE 16384

#define ERROR(x) cout << "[ERROR] :: " << x << "\n";
// #define SUCCESS(x) cout << "[SUCCESS] :: " << x << "\n";
#define MSG(x) cout << "[+] " << x << "\n";
#define PRINT(x) cout << x << "\n";
#define DEBUG(x) cout << "[DEBUG] :: " << x << "\n";

struct fileMetaDataAtClient {
	string filename;
	string fileFullPath;
	string fileSHA;
	string groupID;
	int no_of_chunks_in_file;
	vector<bool> chunks_bitmap;
	int no_of_downloaded_chunks;
	string status; // uploaded, in_progress, downloaded, failed
};

/* Variable declaration*/
string PEER_LISTEN_IP;
int PEER_LISTEN_PORT;
string TRACKER_FILE_NAME;

string TRACKER_IP;
int TRACKER_PORT;
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
		for(int i = 0 ; i < 20; i++) {
			if(temp_sha[i] != '\0' && temp_sha[i] != '\n' && temp_sha[i] != '\t') {
				sha += temp_sha[i];
			}
			else{
				sha += "_";
			}
		}
    }
}


void *processOtherPeersRequest(void *arg)
{
	int new_socket = long(arg);
	char buffer[BUFFER_SIZE];
	bzero(buffer, sizeof(buffer));
	string message = "";

	int noOfBytesReceived = recv(new_socket, buffer, sizeof(buffer), 0);
	if (noOfBytesReceived <= 0)
	{
		close(new_socket);
		pthread_exit(NULL);
	}

	string inputString = buffer;
	vector<string> inputVector;
	stringstream ss(inputString);
    string token;
    while (ss >> token) {
		inputVector.push_back(token);
    }

	if(inputVector[0] == "send_file_chunks_metadata") {
		string filename = inputVector[1];
		string user_id = inputVector[2];
		if(myLoginInfo != user_id) {
			message = "ERROR";
			send(new_socket, message.c_str(), message.size(), 0);
			close(new_socket);
			pthread_exit(NULL);
		}
		string message = "";
		for(int i = 0; i < mySharableFilesInfo[filename].chunks_bitmap.size(); i++) {
			message += to_string(mySharableFilesInfo[filename].chunks_bitmap[i]);
		}
		send(new_socket, message.c_str(), message.size(), 0);
		close(new_socket);
		pthread_exit(NULL);
	}

	else if(inputVector[0] == "send_file_chunks") {
		string filename = inputVector[1];
		int whichChunk = stoi(inputVector[2]);
		string user_id = inputVector[3];

		if(myLoginInfo != user_id || mySharableFilesInfo.find(filename) == mySharableFilesInfo.end()) {
			message = "ERROR";
			send(new_socket, message.c_str(), message.size(), 0);
			close(new_socket);
			pthread_exit(NULL);
		}

		string filePath = mySharableFilesInfo[filename].fileFullPath;
		int fp = open(filePath.c_str(), O_RDONLY | O_LARGEFILE);

		bzero(buffer, sizeof(buffer));
		int nread = pread64(fp,buffer,sizeof(buffer), whichChunk * BUFFER_SIZE);
		int sz = nread;
		string xx = to_string(nread);
		send(new_socket, xx.c_str(), xx.size(), 0);

		bzero(buffer, sizeof(buffer));
		int noOfBytesReceived = recv(new_socket, buffer, sizeof(buffer), 0);
		if (noOfBytesReceived <= 0)
		{
			close(new_socket);
			pthread_exit(NULL);
		}

		int chota_chunk_no = 0;
		while(sz > 0) {
			
			bzero(buffer, sizeof(buffer));
			nread = pread64(fp,buffer, CHOTA_CHUNK_SIZE, whichChunk * BUFFER_SIZE + chota_chunk_no * CHOTA_CHUNK_SIZE);
			send(new_socket, buffer, nread, 0);
			int noOfBytesReceived = recv(new_socket, buffer, sizeof(buffer), 0);
			if (noOfBytesReceived <= 0) {
				break;
			}
			chota_chunk_no++;
			sz = sz - nread;
		}

		close(fp);
		close(new_socket);
		pthread_exit(NULL);
	}
	else {
		message = "ERROR";
		send(new_socket, message.c_str(), message.size(), 0);
	}
	

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

		pthread_t thread_to_process_other_peers_request;
		pthread_create(&thread_to_process_other_peers_request, NULL, &processOtherPeersRequest, (void *)new_socket);
	}
	
	// closing the listening socket
	shutdown(server_fd, SHUT_RDWR);
}

struct downloadFileMetadataArgs {
    string filename;
	string fileDestinationPath;
	string fileSHA;
	string groupID;
	int no_of_chunks_in_file;
	map<string, pair<string,int>> userList; // user_id, IP, Port
};

void *handleFileDownload(void *arg) {

	char buffer[BUFFER_SIZE];
	string filename = ((downloadFileMetadataArgs*)arg) -> filename;
	string fileDestinationPath = ((downloadFileMetadataArgs*)arg) -> fileDestinationPath;
	if(fileDestinationPath[fileDestinationPath.size() - 1] != '/') fileDestinationPath += '/';
	string fileSHA = ((downloadFileMetadataArgs*)arg) -> fileSHA;
	int no_of_chunks_in_file = ((downloadFileMetadataArgs*)arg) -> no_of_chunks_in_file;
	map<string, pair<string,int>> userList = ((downloadFileMetadataArgs*)arg) -> userList; // userid, IP, PORT
	string groupID = ((downloadFileMetadataArgs*)arg) -> groupID;
	//--------------------------------------------
	vector<pair<int, vector<string>>> chunksInfo; 
	for(int i = 0 ; i < no_of_chunks_in_file; i++) {
		chunksInfo.push_back({i, {}});
	}

	for(auto user : userList) {
		string IP = user.second.first;
		int PORT = user.second.second;
		int new_socket, client_fd;
		struct sockaddr_in address;
		int opt = 1;
		int addrlen = sizeof(address);

		// Creating socket file descriptor
		if ((new_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			// ERROR("#1 error")
			continue;
		}

		address.sin_family = AF_INET;

		// Attaching IP Address to socket address
		if (inet_pton(AF_INET, IP.c_str(), &address.sin_addr) <= 0) {
			// ERROR("#2 error")
			continue;
		}

		address.sin_port = htons(PORT);

		if((client_fd = connect(new_socket, (struct sockaddr*)&address, sizeof(address))) < 0) {
			// ERROR("#3 error")
			continue;
		}
		string command = "send_file_chunks_metadata " + filename + " " +  user.first;
		send(new_socket, command.c_str(), command.size(), 0);

		bzero(buffer, sizeof(buffer));
		int noOfBytesReceived = recv(new_socket, buffer, sizeof(buffer), 0);
		if (noOfBytesReceived <= 0)
		{
			continue;
		}
		string s = buffer;
		if(buffer == "ERROR") {
			continue;
		}
		
		
		for(int i = 0 ; i < s.size(); i++) {
			if(s[i] == '1') {
				chunksInfo[i].second.push_back(user.first);
			}
		}
		close(new_socket);
	}

	fileMetaDataAtClient f;
	f.filename = filename;
	f.fileFullPath = fileDestinationPath + filename;
	f.fileSHA = fileSHA;
	f.groupID = groupID;
	f.no_of_chunks_in_file = no_of_chunks_in_file;
	f.status = "in_progress";
	for(int i = 0; i < no_of_chunks_in_file; i++) {
		f.chunks_bitmap.push_back(0);
	}
	f.no_of_downloaded_chunks = 0; 
	mySharableFilesInfo[filename] = f;

	string filePath = fileDestinationPath + filename;

	int fwp = open(filePath.c_str(), O_WRONLY | O_CREAT | O_LARGEFILE, S_IRUSR | S_IWUSR);

	for(auto chunk : chunksInfo) {
		int whichChunk = chunk.first;
		while(chunk.second.size() != 0  && mySharableFilesInfo[filename].chunks_bitmap[whichChunk] == 0) {
			int index = rand() % chunk.second.size();
			string user_id =  chunk.second[index];
			string IP = userList[user_id].first;
			int PORT = userList[user_id].second;
			

			int new_socket, client_fd;
			struct sockaddr_in address;
			int opt = 1;
			int addrlen = sizeof(address);

			// Creating socket file descriptor
			if ((new_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			{
				chunk.second.erase (chunk.second.begin() +  index);
				continue;
			}

			address.sin_family = AF_INET;

			// Attaching IP Address to socket address
			if (inet_pton(AF_INET, IP.c_str(), &address.sin_addr) <= 0) {
				chunk.second.erase (chunk.second.begin() +  index);
				continue;
			}

			address.sin_port = htons(PORT);

			if((client_fd = connect(new_socket, (struct sockaddr*)&address, sizeof(address))) < 0) {
				chunk.second.erase (chunk.second.begin() +  index);
				continue;
			}

			string command = "send_file_chunks " + filename + " " + to_string(whichChunk) + " " + user_id;
			send(new_socket, command.c_str(), command.size(), 0);
			bzero(buffer, sizeof(buffer));
			
			int nread = recv(new_socket, buffer, sizeof(buffer), 0);
			string s = buffer;
			if(nread <= 0 || s == "ERROR") {
				chunk.second.erase (chunk.second.begin() +  index);
				continue;
			}
			int sz = stoi(s);
			command = "1";
			send(new_socket, command.c_str(), command.size(), 0);

			int chota_chunk_no = 0;
			while(sz > 0) {
				bzero(buffer, sizeof(buffer));
				nread = recv(new_socket, buffer, CHOTA_CHUNK_SIZE, 0);
				s = buffer;
				if (nread <= 0 || s == "ERROR") {
					break;
				}
				nread = pwrite64(fwp, buffer,nread, whichChunk * BUFFER_SIZE + chota_chunk_no * CHOTA_CHUNK_SIZE);
				chota_chunk_no++;
				sz = sz - nread;

				command = "1";
				send(new_socket, buffer, nread, 0);
			}

			if (sz > 0)
			{
				chunk.second.erase (chunk.second.begin() +  index);
				continue;
			}
			mySharableFilesInfo[filename].chunks_bitmap[whichChunk] = 1;
			mySharableFilesInfo[filename].no_of_downloaded_chunks++;
			// cout << "Chunk " << whichChunk << " is downloaded from " << IP << ":" << PORT << "\n";
			break;
		}
	}
	close(fwp);
	
	if(mySharableFilesInfo[filename].no_of_downloaded_chunks == mySharableFilesInfo[filename].no_of_chunks_in_file) {
		string fsha = "";
		int total_chunks = 0;
		calculateSHAOfFile(mySharableFilesInfo[filename].fileFullPath, fsha, total_chunks);
		// cout << fsha << "\n" << mySharableFilesInfo[filename].fileSHA << "\n";
		if(fsha == mySharableFilesInfo[filename].fileSHA) {
			// cout << filename << " sha matched\n";
			mySharableFilesInfo[filename].status = "downloaded";
		}
		else {
			mySharableFilesInfo[filename].status = "failed";
			// cout << filename << " sha not matched\n";
		}
	}
	else {
		mySharableFilesInfo[filename].status = "failed";
	}
	// Check if whole file is downloaded or not.
	pthread_exit(NULL);

}

void fetchTrackerIpPort(int tracker_no) {
	char buffer[100] = {0};
	FILE *fd = fopen(TRACKER_FILE_NAME.c_str(), "r");
	while(fscanf(fd, "%s", buffer) > 0) {
		string s = buffer;
		int tno = stoi(s);
		bzero(buffer, sizeof(buffer));
		if(fscanf(fd, "%s", buffer) < 0) {
			cout << TRACKER_FILE_NAME << " doesn't have data in proper format.\n";
			exit(EXIT_FAILURE);
		}
		string ip_port = buffer;
		int found = ip_port.find_last_of(':');
		string ip = ip_port.substr(0, found);
		string port = ip_port.substr(found + 1 , ip_port.size() - found + 1);

		if(tno == tracker_no) {
			TRACKER_IP = ip;
			TRACKER_PORT = stoi(port);
			return;
		}
		bzero(buffer, sizeof(buffer));
	}
	cout << "Tracker No. " <<  tracker_no << " not found in " << TRACKER_FILE_NAME << ".\n";
	exit(EXIT_FAILURE);
}

int connectMeToTracker(int tracker_no) {
	fetchTrackerIpPort(tracker_no);
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
	if(argc < 3) {
		cout << "Not Enough Arguments\n";
		cout << "<IP> <PORT> <tracker_info.txt>\n";
		exit(EXIT_FAILURE);
	}

	string ip_port = argv[1];
	TRACKER_FILE_NAME = argv[2];

	int found = ip_port.find_last_of(':');
	PEER_LISTEN_IP = ip_port.substr(0, found);
	PEER_LISTEN_PORT = stoi(ip_port.substr(found + 1 , ip_port.size() - found + 1));

	int TRACKER_SOCKET = connectMeToTracker(1);
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
				f.groupID = group_id;
				f.no_of_chunks_in_file = total_chunks;
				f.status = "uploaded";
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

			string group_id = inputVector[1];
			string filename = inputVector[2];
			string fileDestinationPath = inputVector[3];

			string command = "get_file_metadata " + group_id + " " + filename +  " " + myLoginInfo;
			send(TRACKER_SOCKET, command.c_str(), command.size(), 0);
			int nread = recv(TRACKER_SOCKET, buffer, sizeof(buffer), 0);
			if(nread < 0) {
				ERROR("SOME ERROR")
				continue;
			}
			string mes = buffer;
			if(mes != "SUCCESS") {
				ERROR(mes)
				continue;
			}
	
			send(TRACKER_SOCKET, "1", command.size(), 0);
			bzero(buffer, sizeof(buffer));

			nread = recv(TRACKER_SOCKET, buffer, sizeof(buffer), 0);
			if(nread < 0) {
				ERROR("SOME ERROR")
				continue;
			}
			string fileSHA = buffer;
			send(TRACKER_SOCKET, "1", command.size(), 0);

			bzero(buffer, sizeof(buffer));
			nread = recv(TRACKER_SOCKET, buffer, sizeof(buffer), 0);
			if(nread < 0) {
				ERROR("SOME ERROR")
				continue;
			}
			int no_of_chunks_in_file = stoi(buffer);

			send(TRACKER_SOCKET, "1", command.size(), 0);

			bzero(buffer, sizeof(buffer));
			nread = recv(TRACKER_SOCKET, buffer, sizeof(buffer), 0);
			if(nread < 0) {
				ERROR("SOME ERROR")
				continue;
			}
			string users = buffer;

			map<string, pair<string,int>> userList;
			stringstream ss(users);
			string token, token1, token2;
			while (ss >> token && ss >> token1 && ss >> token2) {
				userList[token] = {token1, stoi(token2)};
			}

			downloadFileMetadataArgs *dfma = new downloadFileMetadataArgs();
			dfma -> filename = filename;
			dfma -> fileDestinationPath = fileDestinationPath;
			dfma -> fileSHA = fileSHA;
			dfma -> no_of_chunks_in_file = no_of_chunks_in_file;
			dfma -> userList = userList;
			dfma -> groupID = group_id;

			PRINT("Downloading started...")
			pthread_t thread_to_handle_file_download;
			pthread_create(&thread_to_handle_file_download, NULL, &handleFileDownload, (void *)dfma);

		}

		// logout
		else if(inputVector[0] == "logout") {

			if(myLoginInfo == "") {
				ERROR("Login first, then logout.");
				continue;
			}

			string command = "logout " + myLoginInfo;

			send(TRACKER_SOCKET, command.c_str(), command.size(), 0);

			recv(TRACKER_SOCKET, buffer, sizeof(buffer), 0);
			myLoginInfo = "";
			string mes = buffer;
			PRINT(mes);
			
		}

		// show_downloads
		else if(inputVector[0] == "show_downloads") {
			if(mySharableFilesInfo.size() == 0) {
				cout << "No Downloaded Files\n";
			}
			for(auto file : mySharableFilesInfo) {
				if(file.second.status == "in_progress") {
					cout << "[D] " << file.second.groupID << " " << file.first << " " << (file.second.no_of_downloaded_chunks * 100)/file.second.no_of_chunks_in_file << "%\n"; 
				}
				else if(file.second.status == "downloaded") {
					cout << "[C] " << file.second.groupID << " " << file.first << " " << (file.second.no_of_downloaded_chunks * 100)/file.second.no_of_chunks_in_file << "%\n";
				}
				else if(file.second.status == "failed") {
					cout << "[F] " << file.second.groupID << " " << file.first << " " << (file.second.no_of_downloaded_chunks * 100)/file.second.no_of_chunks_in_file << "%\n";
				}
			}
		}

		// stop_share
		else if(inputVector[0] == "stop_share") {
			if(noOfTokensInInputString != 3) {
				ERROR("2 arguments required.")
				continue;
			}

			if(myLoginInfo == "") {
				ERROR("Login first, then logout.");
				continue;
			}
			string group_id = inputVector[1];
			string filename = inputVector[2];


			string command = "stop_share " + group_id + " " + filename + " " + myLoginInfo;

			send(TRACKER_SOCKET, command.c_str(), command.size(), 0);

			recv(TRACKER_SOCKET, buffer, sizeof(buffer), 0);
			mySharableFilesInfo.erase(filename);
			string mes = buffer;
			PRINT(mes);
		}

		else {
			PRINT("Invalid Input.");
		}
	}

	return 0;
}
