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
string TRACKER_LISTEN_IP;
int TRACKER_LISTEN_PORT;
string TRACKER_FILE_NAME;
int MY_TRACKER_NO;

map<string, string> userInfo; // userid, password
map<string, pair<string, int>> userLoggedIn; // userid, {ip, port}

map<string, string> groupInfo; // groupid, userid(owner)-- Owner Info
map<string, set<string>> groupMemberInfo; // groupid, {userid's} -- infor of group members
map<string, set<string>> groupPendingRequestInfo; // groupid, {userid's} -- Pending Request.



/*--------------------------------*/


void *connectToClients(void *arg)
{
	int new_socket = long(arg);
	string message;

	
	message = "Connected to Tracker" + to_string(MY_TRACKER_NO) + " at " + TRACKER_LISTEN_IP + ":" + to_string(TRACKER_LISTEN_PORT); 
	send(new_socket, message.c_str(), message.size(), 0);
	
	char buffer[BUFFER_SIZE];

	while(1) {
		bzero(buffer, sizeof(buffer));
		int noOfBytesReceived = recv(new_socket, buffer, sizeof(buffer), 0);
		
		// Maybe due to Connection Breaks Between Client & Server
		if (noOfBytesReceived <= 0) break;

		string inputString = buffer;
		vector<string> inputVector;
		stringstream ss(inputString);
    	string token;
    	while (ss >> token) {
			inputVector.push_back(token);
    	}
		
		// create user
		if(inputVector[0] == "create_user")	{
			string user_id = inputVector[1];
			string password = inputVector[2];

			if(userInfo.find(user_id) != userInfo.end()) { // already there;
				message = "User Already Exists";
			}
			else {
				userInfo[user_id] = password;
				message = "User Created.";
			}
			send(new_socket, message.c_str(), message.size(), 0);
		}
		
		// login
		else if(inputVector[0] == "login") {
			string user_id = inputVector[1];
			string password = inputVector[2];
			string ip = inputVector[3];
			int port = stoi(inputVector[4]);

			if(userInfo.find(user_id) == userInfo.end()) { // 
				message = "User doesn't exists";
			}
			else if(userLoggedIn.find(user_id) != userLoggedIn.end()) { // already there;
				message = "User already loggedin with other system at " + userLoggedIn[user_id].first + ":" + to_string(userLoggedIn[user_id].second);
			}
			else {
				userInfo[user_id] = password;
				userLoggedIn[user_id] = {ip, port};
				message = "user_logged_in";
			}
			send(new_socket, message.c_str(), message.size(), 0);
		}

		// create_group
		else if(inputVector[0] == "create_group") {
			string group_id = inputVector[1];
			string user_id = inputVector[2];

			if(groupInfo.find(group_id) != groupInfo.end()) { // 
				message = "Group Already exists";
			}

			else {
				groupInfo[group_id] = user_id; // Owner
				groupMemberInfo[group_id].insert(user_id); // Will also be a part of group.
				message = "Group Created.";
			}
			send(new_socket, message.c_str(), message.size(), 0);
		}

		// join_group
		else if(inputVector[0] == "join_group") {
			string group_id = inputVector[1];
			string user_id = inputVector[2];

			if(groupInfo.find(group_id) == groupInfo.end()) { // 
				message = "Group doesn't exists";
			}

			else if(groupMemberInfo[group_id].count(user_id)) {
				message = "You are already the part of group.";
			}

			else if(groupPendingRequestInfo[group_id].count(user_id)) {
				message = "You have already requested to join the group.";
			}

			else {
				groupPendingRequestInfo[group_id].insert(user_id); // inserted in pending list.
				message = "Request Created to join group.";
			}
			send(new_socket, message.c_str(), message.size(), 0);
		}

		// leave_group
		else if(inputVector[0] == "leave_group") {
			string group_id = inputVector[1];
			string user_id = inputVector[2];

			if(groupInfo.find(group_id) == groupInfo.end()) { // 
				message = "Group doesn't exists";
			}

			else if(groupPendingRequestInfo[group_id].count(user_id)) {
				groupPendingRequestInfo[group_id].erase(user_id);
				message = "Your requested to join the group has been cancelled.";
			}

			else if(groupMemberInfo[group_id].count(user_id) == 0) {
				message = "You are not the part of group.";
			}

			if(groupInfo[group_id] != user_id) { // You are not the owner
				groupMemberInfo[group_id].erase(user_id);
				message = "You have left the group successfully.";
			}

			else if(groupMemberInfo[group_id].size() == 1) { 
				// Owner is the last member of group, then delete the group.
				groupInfo.erase(group_id);
				groupMemberInfo.erase(group_id);
				groupPendingRequestInfo.erase(group_id);
				message = "You left the group, and Group Deleted as you were the last member of group.";
			}

			else { // Need to transfer the ownership.
				for(auto user : groupMemberInfo[group_id]) {
					if(user != user_id) {
						groupInfo[group_id] = user;
						groupMemberInfo[group_id].erase(user_id);
						message = "You left the group, and ownership is transfered to '" + user + "'.";
						break;
					}
				}
			}

			send(new_socket, message.c_str(), message.size(), 0);
		}


		// list_requests
		else if(inputVector[0] == "list_requests") {
			string group_id = inputVector[1];
			string user_id = inputVector[2];

			if(groupInfo.find(group_id) == groupInfo.end()) { // 
				message = "Group doesn't exists";
			}

			else if(groupMemberInfo[group_id].count(user_id) == 0) {
				message = "Only Group Members are authorised to see pending requests in group.";
			}

			else if(groupPendingRequestInfo[group_id].size() == 0) {
				message = "Currently No Pending requests in the group.";

			}

			else {
				message = "Pending Members request in group : '" + group_id + "' : \n";
				int i = 0;
				for(auto member : groupPendingRequestInfo[group_id]) {
					i++;
					message += to_string(i) + ". " + member + "\n";
				}
			}
			send(new_socket, message.c_str(), message.size(), 0);
		}

		// accept_request
		else if(inputVector[0] == "accept_request") {
			string group_id = inputVector[1];
			string user_id = inputVector[2];
			string pending_member_group_id = inputVector[3];

			if(groupInfo.find(group_id) == groupInfo.end()) { // 
				message = "Group doesn't exists";
			}

			if(groupInfo[group_id] != user_id) { 
				message = "Only Group Admins can accept the request.";
			}

			else if(groupPendingRequestInfo[group_id].count(pending_member_group_id) == 0) {
				message = "No Pending request for given user.";

			}

			else {
				groupMemberInfo[group_id].insert(pending_member_group_id);
				groupPendingRequestInfo[group_id].erase(pending_member_group_id);
				message = "'" + pending_member_group_id + "' has been added to group '" + group_id + "'";
			}
			send(new_socket, message.c_str(), message.size(), 0);
		}


		// Aise hi
		else {
			buffer[0] = '$';
			send(new_socket, buffer, sizeof(buffer), 0);
		}
		
	}

	close(new_socket);
	pthread_exit(NULL);
}

void *listenToClients(void *arg) {
	
	int server_fd; // File Descriptor
	int new_socket; // 

	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);

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
	if (inet_pton(AF_INET, TRACKER_LISTEN_IP.c_str(), &address.sin_addr) <= 0) {
		ERROR("INVALID IP ADDRESS!!!");
		exit(EXIT_FAILURE);
	}
	
	address.sin_port = htons(TRACKER_LISTEN_PORT);

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

	MSG("Tracker is up for listening at " + TRACKER_LISTEN_IP + ":" + to_string(TRACKER_LISTEN_PORT));

	while (1)
	{
		if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
		{
			ERROR("NOT ABLE TO ACCEPT REQUEST");
			exit(EXIT_FAILURE);
		}

		DEBUG((string)inet_ntoa(address.sin_addr) + " - " + to_string(address.sin_port) + " - " + to_string(ntohs(address.sin_port)));

		pthread_t thread_to_connect_to_clients;
		pthread_create(&thread_to_connect_to_clients, NULL, &connectToClients, (void *)new_socket);
	}
	
	// closing the listening socket
	shutdown(server_fd, SHUT_RDWR);
}

int main(int argc, char** argv)
{
	// TRACKER_FILE_NAME = argv[1];
	// MY_TRACKER_NO = stoi(argv[2]);

	/* HardCoding IP, PORT Of Tracker as of now*/
	TRACKER_LISTEN_IP = "127.0.0.1";
	TRACKER_LISTEN_PORT = 8080;
	MY_TRACKER_NO = 1;

	// cout << PEER_LISTEN_IP << " : " << PEER_LISTEN_PORT << " : " << TRACKER_FILE_NAME << "\n";

	pthread_t thread_listen_to_clients;
	pthread_create(&thread_listen_to_clients, NULL, &listenToClients, NULL);

	while(1) {
		// Will use it for tracker synchronization.
	}

	return 0;
}
