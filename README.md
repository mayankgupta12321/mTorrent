# mTorrent:Peer-to-Peer-Group-Based-File-Sharing-System

## Description

This project is a peer-to-peer file sharing network that enables users to share, download, and remove files from the group they belong to. Download happens parallelly with multiple pieces from multiple peers. It follows a similar concept to popular file sharing protocols like BitTorrent found on the internet.

The architecture of this mTorrent project consists of multiple clients (users) and a tracker. The tracker plays a crucial role in maintaining the network by storing metadata related to the files and users. Specifically, the tracker maintains a mapping of files to the users who possess them. This mapping helps clients discover other users who have the desired files they wish to download.


## Functional Requirements

The network for the Mini-torrent project will consist of the following entities:

1. Tracker(Centralized Server):

   - Maintain information of clients with their files (shared by client) to assist the clients for communication between peers.
   - Tracker should be online all the time.

2. Clients:
   - Users should create an account and register with the tracker.
   - Login using the user credentials.
   - Create a group and become the owner of that group.
   - Fetch a list of all groups in the server.
   - Request to join a group.
   - Leave a group.
   - Accept group join requests (if the client is the owner).
   - Share a file across the group: Share the filename and SHA1 hash of the complete file, as well as piecewise SHA1, with the tracker.
   - Fetch a list of all shareable files in a group.
   - Download a file:
     - Retrieve peer information from the tracker for the file.
     - **Core Part**: Download the file from multiple peers simultaneously, obtaining different pieces of the file from different peers (using a piece selection algorithm).
     - All downloaded files by the client will be shareable to other users in the same group.
     - Ensure file integrity through **SHA1** comparison.
   - Show ongoing downloads.
   - Stop sharing a file.
   - Stop sharing all files (Logout).
   - Whenever a client logs in, all previously shared files before logout should automatically be in sharing mode.


### Prerequisites

- **G++ Compiler**:

  ```
  sudo apt-get install g++
  ```

- **OpenSSL Library**:

  ```
  sudo apt-get install openssl
  ```

- **Platform**: `Linux`

### Execution

- **Tracker**:

    ```
    cd tracker
    g++ tracker.cpp -o tracker 
    ./tracker tracker_info.txt tracker_no 

    eg : ./tracker tracker_info.txt 1
    ```

- **Client**:

    ```
    cd client
    g++ client.cpp -lcrypto -o client
    ./client ./client <IP>:<PORT> tracker_info.txt

    eg : ./client 127.0.0.1:7600 tracker_info.txt
    ```

## Client Commands

Commands:

1. Create User Account:
   `create_user <user_id> <password>`

2. Login:
   `login <user_id> <password>`

3. Create Group:
   `create_group <group_id>`

4. Join Group:
   `join_group <group_id>`

5. Leave Group:
   `leave_group <group_id>`

6. List Pending Join:
   `list_requests <group_id>`

7. Accept Group Joining Request:
   `accept_request <group_id> <user_id>`

8. List All Groups in Network:
   `list_groups`

9. List All Sharable Files in Group:
   `list_files <group_id>`

10. Upload File:
    `upload_file <file_path> <group_id>`

11. Download File:
    `download_file <group_id> <file_name> <destination_path>`

12. Logout:
    `logout`

13. Show Downloads:
    `show_downloads`

- Output format: `[D] [grp_id] filename` or `[C] [grp_id] filename` or `[F] [grp_id] filename` (D - Downloading, C - Complete, F - Failed)

14. Stop Sharing:
    `stop_share <group_id> <file_name>`
