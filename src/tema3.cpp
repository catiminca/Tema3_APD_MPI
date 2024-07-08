#include <mpi.h>
#include <bits/stdc++.h>


#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

typedef struct {
    int rank;
    std::vector<int> filesWanted;
} clientInfo;

std::unordered_map<int, std::unordered_map<int, std::pair<int, int>>> clientsAllFiles;
std::unordered_map<int, std::unordered_map<int, std::vector<std::string>>> clientsAllData;

void *download_thread_func(void *arg)
{
    auto clientInformations = *(clientInfo *)arg;
    std::unordered_map<int, std::vector<std::string>> fileToHash;
    int msg = -1;
    
    /*
    * Let tracker know the client wanted files to keep the correct number
    */
    for (auto& wantFile : clientInformations.filesWanted) {
        MPI_Send(&msg, 1, MPI_INT, 0, 7, MPI_COMM_WORLD);
        MPI_Send(&wantFile, 1, MPI_INT, 0, 21, MPI_COMM_WORLD);
    }

    for (auto& wantFile : clientInformations.filesWanted) {
        int sizeOwners, client, hashesNr;
        char hashOwner[HASH_SIZE + 1];
        hashOwner[HASH_SIZE] = '\0';
        std::vector<int> peers;
        std::unordered_map<std::string, std::vector<int>> segmentOwn;
        msg = 1;

        /*
        * Send the file number to tracker and receiving the informations about it
        * Receives how many owners are and the owners, then the number of hashes
        * that they own and the hashes
        */

        MPI_Send(&msg, 1, MPI_INT, 0, 7, MPI_COMM_WORLD);
        MPI_Send(&wantFile, 1, MPI_INT, 0, 8, MPI_COMM_WORLD);

        MPI_Recv(&sizeOwners, 1, MPI_INT, 0, 9, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        for (int i = 0; i < sizeOwners; i++) {
            MPI_Recv(&client, 1, MPI_INT, 0, 9, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            peers.push_back(client);
        }

        for (int i = 0; i < sizeOwners; i++) {
            MPI_Recv(&hashesNr, 1, MPI_INT, 0, 9, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            clientsAllFiles[clientInformations.rank][wantFile].first = hashesNr;
            for (int j = 0; j < hashesNr; j++) {
                MPI_Recv(hashOwner, HASH_SIZE, MPI_CHAR, 0, 9, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                std::string hash = hashOwner;
                segmentOwn[hash].push_back(peers[i]);
                if (std::find(fileToHash[wantFile].begin(), fileToHash[wantFile].end(), hash) == fileToHash[wantFile].end()) {
                    fileToHash[wantFile].push_back(hash);
                }
            }
        }

        /*
        * Choosing a random owner and sending the number of file and then the hashes to it
        * The current client receives ack from the owner if it has those hashes
        * 
        * If the hashes are more than 10 then, the client is sending to the tracker
        * the hashes to inform it about owning them
        */

        int clientSeed = rand() % sizeOwners;
        int nrHashesContor = 0;
        int messageUpload = 0;
        MPI_Send(&messageUpload, 1, MPI_INT, peers[clientSeed], 20, MPI_COMM_WORLD);
        MPI_Send(&wantFile, 1, MPI_INT, peers[clientSeed], 2, MPI_COMM_WORLD);
        int sizeAllHashes = fileToHash[wantFile].size();
        MPI_Send(&sizeAllHashes, 1, MPI_INT, peers[clientSeed], 4, MPI_COMM_WORLD);
        for (auto& hash : fileToHash[wantFile]) {
            MPI_Send(hash.c_str(), HASH_SIZE, MPI_CHAR, peers[clientSeed], 5, MPI_COMM_WORLD);

            char hashRecv[4];
            hashRecv[3] = '\0';
            MPI_Recv(hashRecv, 3, MPI_CHAR, peers[clientSeed], 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            clientsAllData[clientInformations.rank][wantFile].push_back(hash);
            clientsAllFiles[clientInformations.rank][wantFile].second++;
            nrHashesContor++;
            if (nrHashesContor % 10 == 0) {
                msg = 4;
                MPI_Send(&msg, 1, MPI_INT, 0, 7, MPI_COMM_WORLD);
                int nrmsg = 10;
                MPI_Send(&nrmsg, 1, MPI_INT, 0, 11, MPI_COMM_WORLD);
                for (int i = nrHashesContor - 10; i < nrHashesContor; i++) {
                    MPI_Send(fileToHash[wantFile][i].c_str(), HASH_SIZE, MPI_CHAR, 0, 12, MPI_COMM_WORLD);

                }
            }
            
        }

        /*
        * If the total number of hashes % 10 != 0, there are going to be some hashes
        * that the client own and the tracker don't know, so they are sent
        */

        int hashesRemained = sizeAllHashes % 10;
        if (hashesRemained > 0) {
            msg = 4;
            MPI_Send(&msg, 1, MPI_INT, 0, 7, MPI_COMM_WORLD);
            MPI_Send(&hashesRemained, 1, MPI_INT, 0, 11, MPI_COMM_WORLD);
            for (int i = sizeAllHashes - hashesRemained; i < sizeAllHashes; i++) {
                MPI_Send(fileToHash[wantFile][i].c_str(), HASH_SIZE, MPI_CHAR, 0, 12, MPI_COMM_WORLD);
            }   
        }
        
        /*
        * If the client has the whole file, the output file can be created
        * and then it should inform the tracker
        */

        if (clientsAllFiles[clientInformations.rank][wantFile].first ==
                clientsAllFiles[clientInformations.rank][wantFile].second) {
            char outName[MAX_FILENAME];
            memset(outName, 0, MAX_FILENAME);
            strcpy(outName, "client");
            outName[strlen(outName)] = '0' + clientInformations.rank;
            strcat(outName, "_file");
            outName[strlen(outName)] = '0' + wantFile;
            std::cout << outName << '\n';
            std::filebuf fb;
            fb.open (outName,std::ios::out);
            std::ostream os(&fb);
            for (int i = 0; i < clientsAllData[clientInformations.rank][wantFile].size(); i++) {
                os << clientsAllData[clientInformations.rank][wantFile][i] << '\n';
            }
        }
        msg = 2;
        MPI_Send(&msg, 1, MPI_INT, 0, 7, MPI_COMM_WORLD);
        std::string trackerMsg = "Finish";
        MPI_Send(trackerMsg.c_str(), trackerMsg.size(), MPI_CHAR, 0, 10, MPI_COMM_WORLD);
        MPI_Send(&wantFile, 1, MPI_INT, 0, 10, MPI_COMM_WORLD);

    }

    /*
    * This type of message is sent to tracker to inform it that the client has
    * finished the list of wanted files
    */

    msg = 3;
    MPI_Send(&msg, 1, MPI_INT, 0, 7, MPI_COMM_WORLD);

    return NULL;
}

void *upload_thread_func(void *arg)
{
    int rank = *(int*) arg;
    int fileWanted;
    MPI_Status status;
    int numtasksTotal;
    int msg;
    while (1) {
        MPI_Recv(&msg, 1, MPI_INT, MPI_ANY_SOURCE, 20, MPI_COMM_WORLD, &status);
        if (msg == 0) {

            /*
            * The client receives from other that it wants some information
            * about a file and it sends then
            * For the hash, the current client checks if it really has that hash
            */

            MPI_Recv(&fileWanted, 1, MPI_INT, MPI_ANY_SOURCE, 2, MPI_COMM_WORLD, &status);
            int sizeAllHashes;
            MPI_Recv(&sizeAllHashes, 1, MPI_INT, MPI_ANY_SOURCE, 4, MPI_COMM_WORLD, &status);
            char hashRecv[HASH_SIZE + 1];
            hashRecv[HASH_SIZE] = '\0';
            for (int i = 0; i < sizeAllHashes; i++) {
                MPI_Recv(hashRecv, HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, 5, MPI_COMM_WORLD, &status);
                std::string hashSearched = hashRecv;

                if (std::find(clientsAllData[rank][fileWanted].begin(), clientsAllData[rank][fileWanted].end(), hashSearched) !=
                    clientsAllData[rank][fileWanted].end()) {
                        std::string ack = "ack";
                        MPI_Send(ack.c_str(), 3, MPI_CHAR, status.MPI_SOURCE, 3, MPI_COMM_WORLD);
                    }
            }

        } else if (msg == 1) {
            
            /*
            * The client receives a message from tracker that all the clients
            * did their jobs and they are done
            */

            MPI_Recv(&numtasksTotal, 1, MPI_INT, 0, 14, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            char doneMsg[5];
            doneMsg[4] = '\0';
            MPI_Recv(doneMsg, 4, MPI_CHAR, 0, 14, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (strcmp(doneMsg, "done") == 0) {
                break;
            }
        }
    }

    return NULL;
}

/*
* map for each client what files does it has
*/
std::unordered_map<int, std::vector<int>> filesOwnedByClients;

/*
* map for each file what owners does it has
*/
std::unordered_map<int, std::vector<int>> allFilesClients;

/*
* map for each hash what owner does it has
*/
std::unordered_map<std::string, std::vector<int>> segmentsOwners;

/*
* map for clients what segments does they have
*/
std::unordered_map<int, std::vector<std::string>> segmentsOwnedByClients;

/*
* for each client, I associate the file to a number, if is 0, then the file
* is not fully download and he doesn't own it fully, otherwise 1
*/
std::unordered_map<int, std::unordered_map<int, int>> clientsWantedFiles;

void tracker(int numtasks, int rank) {
    int fileNr, nrSegm, nrFiles;
    MPI_Status status;
    std::string hash;
    char hashChar[HASH_SIZE + 1];
    hashChar[HASH_SIZE] = '\0';

    /*
    * The tracker firstly receives all the informations about
    * the clients and their owned files
    */

    for (int i = 1; i < numtasks; i++) {
        MPI_Recv(&nrFiles, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);

        for (int j = 0; j < nrFiles; j++) {
            MPI_Recv(&fileNr, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
            MPI_Recv(&nrSegm, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            allFilesClients[fileNr].push_back(status.MPI_SOURCE);
            filesOwnedByClients[status.MPI_SOURCE].push_back(fileNr);

            for (int k = 0; k < nrSegm; k++) {
                MPI_Recv(hashChar, HASH_SIZE, MPI_CHAR, i, 0, MPI_COMM_WORLD, &status);
                hash = hashChar;
                segmentsOwners[hash].push_back(status.MPI_SOURCE);
                segmentsOwnedByClients[status.MPI_SOURCE].push_back(hash);
                clientsAllData[status.MPI_SOURCE][fileNr].push_back(hash);
            }
        }

    }

    /*
    * Then it is sent the ok for them to start asking for the wanted files
    */

    std::string ackMess = "ok";
    for (int i = 1; i < numtasks; i++) {
        MPI_Send(ackMess.c_str(), ackMess.size(), MPI_CHAR, i, 0, MPI_COMM_WORLD);
    }

    int msg;

    /*
    * keep in a set the clients that want any file
    */
    std::unordered_set<int> clientsDownloads;

    while(1) {
        MPI_Recv(&msg, 1, MPI_INT, MPI_ANY_SOURCE, 7, MPI_COMM_WORLD, &status);

        /*
        * receiving the client's wanted files to be able to populate the set
        */

        if (msg == -1) {
            MPI_Recv(&fileNr, 1, MPI_INT, MPI_ANY_SOURCE, 21, MPI_COMM_WORLD, &status);
            clientsWantedFiles[status.MPI_SOURCE][fileNr] = 0;
            clientsDownloads.insert(status.MPI_SOURCE);
        }
        
        /*
        * receiving from client the file number and send to it the details about the its owners
        */

        else if (msg == 1) {
            MPI_Recv(&fileNr, 1, MPI_INT, MPI_ANY_SOURCE, 8, MPI_COMM_WORLD, &status);

            int sizeOwners = allFilesClients[fileNr].size();
            MPI_Send(&sizeOwners, 1, MPI_INT, status.MPI_SOURCE, 9, MPI_COMM_WORLD);
            
            for (auto& owner : allFilesClients[fileNr]) {
                MPI_Send(&owner, 1, MPI_INT, status.MPI_SOURCE, 9, MPI_COMM_WORLD);
            }
            for (auto& owner : allFilesClients[fileNr]) {
                int hashesOwners = clientsAllData[owner][fileNr].size();
                MPI_Send(&hashesOwners, 1, MPI_INT, status.MPI_SOURCE, 9, MPI_COMM_WORLD);
                for (auto& hash : clientsAllData[owner][fileNr]) {
                    MPI_Send(hash.c_str(), HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, 9, MPI_COMM_WORLD);
                }
            }

        } 
        
        /*
        * receives that a client finished a file and mark it as owned
        */

        else if (msg == 2) {
            char clientFinishMsg[7];
            clientFinishMsg[6] = '\0';
            MPI_Recv(clientFinishMsg, 6, MPI_CHAR, MPI_ANY_SOURCE, 10, MPI_COMM_WORLD, &status);
            int fileRecvFinished;
            MPI_Recv(&fileRecvFinished, 1, MPI_INT, status.MPI_SOURCE, 10, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            filesOwnedByClients[status.MPI_SOURCE].push_back(fileRecvFinished);
            clientsWantedFiles[status.MPI_SOURCE][fileRecvFinished] = 1;
        } 
        
        else if (msg == 3) {
            //the client finished its files
        } 
        
        /*
        * receiving the actualizations about what segements has the client
        */

        else if (msg == 4) {
            int nrHashes;
            MPI_Recv(&nrHashes, 1, MPI_INT, MPI_ANY_SOURCE, 11, MPI_COMM_WORLD, &status);
            char hashes[HASH_SIZE + 1];
            hashes[HASH_SIZE] = '\0';
            for (int i = 0; i < nrHashes; i++) {
                MPI_Recv(hashes, HASH_SIZE, MPI_CHAR, MPI_ANY_SOURCE, 12, MPI_COMM_WORLD, &status);
            }
        }

        /*
        * count how many clients finished their files
        */
        int nrFinished = 0;
        for (auto& client : clientsWantedFiles) {
            bool ok = 1;
            for (auto& file : clientsWantedFiles[client.first]) {
                if (clientsWantedFiles[client.first][file.first] == 0) {
                    ok = 0;
                }
            }
            if (ok == 1) {
               nrFinished++;
            }
        }
        
        /*
        * If all the clients had finished, then the tracker sends to all the clients
        * a message that everything is done
        */

        if (nrFinished == clientsDownloads.size() && nrFinished != 0) {
            int msgUpload = 1;

            std::string doneMsg = "done";
            for (int i = 1; i < numtasks; i++) {
                int nrNumTasks = numtasks - 1;
                MPI_Send(&msgUpload, 1, MPI_INT, i, 20, MPI_COMM_WORLD);

                MPI_Send(&nrNumTasks, 1, MPI_INT, i, 14, MPI_COMM_WORLD);
                MPI_Send(doneMsg.c_str(), doneMsg.size(), MPI_CHAR, i, 14, MPI_COMM_WORLD);

            }
            break;
        }
    }
}

void peer(int numtasks, int rank) {

    FILE * file;
    clientInfo clientsInfo;
    clientsInfo.rank = rank;

    /*
    * Getting input
    */

    char inName[MAX_FILENAME];
    memset(inName, 0, MAX_FILENAME);
    strcpy(inName, "in");
    inName[strlen(inName)] = '0' + rank;
    strcat(inName, ".txt");
    file = freopen(inName, "r", stdin);

    int nrFilesHave, nrFilesWant;
    std::unordered_map<int, std::vector<std::string>> files;
    std::vector<int> filesWanted;
    std::vector<int> filesOwned;
    std::string name;
    int nrSeg;
    std::string charHash;
    std::cin >> nrFilesHave;
    int nrOfFile = 0;
    for (int i = 0; i < nrFilesHave; i++) {
        std::cin >> name >> nrSeg;
        for (char c : name) {
            if (c <= '9' && c >= '0') {
                nrOfFile = (c - '0');
                filesOwned.push_back(nrOfFile);
                clientsAllFiles[rank][nrOfFile].first = nrSeg;
                clientsAllFiles[rank][nrOfFile].second = nrSeg;
            }
        }
        
        for (int j = 0; j < nrSeg; j++) {
            std::cin >> charHash;
            clientsAllData[rank][nrOfFile].push_back(charHash);
            files[nrOfFile].push_back(charHash);
        }
    }

    
    std::cin >> nrFilesWant;
    for (int i = 0; i < nrFilesWant; i++) {
        std::cin >> name;
        for (char c : name) {
            if (c <= '9' && c >= '0') {
                nrOfFile = (c - '0');
                clientsInfo.filesWanted.push_back(nrOfFile);
                clientsAllFiles[rank][nrOfFile].first = 0;
                clientsAllFiles[rank][nrOfFile].second = 0;
            }
        }
    }

    /*
    * Telling tracker the files owned
    */

    int sizefilesOwned = files.size();
    MPI_Send(&sizefilesOwned, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    for (auto& elem : files) {
        MPI_Send(&elem.first, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);

        int size = elem.second.size();
        MPI_Send(&size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);

        for (auto& hash : elem.second) {
            MPI_Send(hash.c_str(), HASH_SIZE, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
        }
    }

    /*
    *   Receives ack from tracker
    */

    char ackChar[3];
    MPI_Recv(ackChar, 2, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    
    std::thread download_thread(download_thread_func, &clientsInfo);
    
    std::thread upload_thread(upload_thread_func, &rank);
    
    download_thread.join();
    upload_thread.join();

}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}
