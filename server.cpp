#include <iostream>
#include <fstream>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <stdio.h>
#include <vector>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "structures.h"

using namespace std;

void parseFile(string fileName) {
  ifstream fin(fileName);

  int N;
  fin >> N;

  for (int i = 0; i < N; i++) {
    string lastName; string firstName;
    fin >> lastName >> firstName;

    int cardNumber; int pin; string pass; double sold;
    fin >> cardNumber >> pin >> pass >> sold;

    User user(lastName, firstName, cardNumber, pin, pass, sold);
    userMap.insert(std::pair<int, User>(cardNumber, user));
  }
  fin.close();
}

void processCommand(char command[BUFLEN], Session &ses, vector<Session> &clientSes) {
  string input = command;
  string firstWord = input.substr(0, input.find(" "));
  char outputBuffer[BUFLEN];
  memset(outputBuffer, 0, BUFLEN);

  if (firstWord == "login") {

    char login[5];
    int cardNo; int pin;
    sscanf (command,"%s %d %d", login, &cardNo, &pin);
    std::map<int, User>::iterator it;
    it = userMap.find(cardNo);
    if (it == userMap.end()) {
      // card number doesn't exist
      strcpy(outputBuffer, "-4: Numar card inexistent\n\n");
      send(ses.cfd, outputBuffer, strlen(outputBuffer) - 1, 0);

    } else {
      // card number exists
      if (it->second.blocked == true) {
        // card blocked
        strcpy(outputBuffer, "-5: Card blocat\n\n");
        send(ses.cfd, outputBuffer, strlen(outputBuffer) - 1, 0);

      } else {
        // search another session
        bool found = false;
        for (int i = 0 ; i < clientSes.size(); i++) {
          if (cardNo == clientSes[i].cardNumber) {
            found = true;
            break;
          }
        }
        if (found) {
          // another session is opened
          strcpy(outputBuffer, "-2: Sesiune deja deschisa\n\n");
          send(ses.cfd, outputBuffer, strlen(outputBuffer) - 1, 0);
        } else {
          // check credentials
          int correctPin = userMap.find(cardNo)->second.pin;
          if (pin == correctPin) {
            // authentification succeeded
            ses.logged = true;
            ses.cardNumber = cardNo;
            // send welcome message
            string lastName  = userMap.find(cardNo)->second.lastName;
            string firstName = userMap.find(cardNo)->second.firstName;
            int n = sprintf (outputBuffer, "Welcome %s %s\n\n",
            lastName.c_str(), firstName.c_str());
            send(ses.cfd, outputBuffer, strlen(outputBuffer) - 1, 0);

          } else {
            // incorrect pin
            ses.attemps --;
            if (ses.attemps == 0) {
              User &user = it->second;
              user.blocked = true;
            }
            strcpy(outputBuffer, "-3: Pin gresit\n\n");
            send(ses.cfd, outputBuffer, strlen(outputBuffer) - 1, 0);
          }
        }
      }
    }

  } else if (firstWord == "logout") {
    // reset ses structure
    ses = Session(ses.cfd);
    strcpy(outputBuffer, "Clientul a fost deconectat\n\n");
    send(ses.cfd, outputBuffer, strlen(outputBuffer) - 1, 0);

  } else if (firstWord == "listsold") {
    // send sold
    double sold = userMap.find(ses.cardNumber)->second.sold;
    // memcpy(outputBuffer,&sold,sizeof(sold));
    send(ses.cfd, to_string(sold).c_str(), sizeof(sold), 0);

  } else if (firstWord == "transfer") {

    char transfer[20];
    int cardNo; double sum;
    sscanf (command,"%s %d %lf", transfer, &cardNo, &sum);

    std::map<int, User>::iterator it;
    it = userMap.find(cardNo);
    if (it == userMap.end()) {
      // card number doesn't exist
      strcpy(outputBuffer, "-4: Numar card inexistent\n\n");
      send(ses.cfd, outputBuffer, strlen(outputBuffer) - 1, 0);
    } else {
      // check if client has enough money
      double sold = userMap.find(cardNo)->second.sold;
      if (sum > sold) {
        strcpy(outputBuffer, "-8: Fonduri insuficiente\n\n");
        send(ses.cfd, outputBuffer, strlen(outputBuffer) - 1, 0);
      } else {
        // request confirmation
        ses.destCardNumber = cardNo;
        ses.sumToTransfer = sum;
        string lastName   = userMap.find(cardNo)->second.lastName;
        string firstName = userMap.find(cardNo)->second.firstName;
        sprintf(outputBuffer, "Transfer %lf catre %s %s ? [y/n]\n",
        sum, lastName.c_str(), firstName.c_str());
        send(ses.cfd, outputBuffer, strlen(outputBuffer) - 1, 0);
      }
    }

  }  else if (firstWord == "quit") {
    // delete session from vector
    for (int i = 0 ; i < clientSes.size(); i++) {
      if (ses.cfd == clientSes[i].cfd) {
        clientSes.erase(clientSes.begin() + i);
        break;
      }
    }
  } else if (firstWord[0] == 'y') {
    User &dest = userMap.find(ses.destCardNumber)->second;
    dest.sold += ses.sumToTransfer;
    User &source = userMap.find(ses.cardNumber)->second;
    source.sold -= ses.sumToTransfer;
    strcpy(outputBuffer, "Succes transfer\n\n");
    send(ses.cfd, outputBuffer,  strlen(outputBuffer) - 1, 0);
  }

}

void manageConnections(int tcpFd, int udpFd) {
  char inputBuffer[BUFLEN];

  fd_set read_set;
  vector<Session> clientSes;

  int fdmax;

  struct sockaddr_in clientaddr;
  socklen_t clen = sizeof(clientaddr);

  int rc;

  while (1) {

    FD_ZERO(&read_set);
    FD_SET(tcpFd, &read_set);
    FD_SET(udpFd, &read_set);
    FD_SET(STDIN_FILENO, &read_set);

    fdmax = max(tcpFd, udpFd);

    for (int i = 0; i < clientSes.size(); i++) {
      int clientFd = clientSes[i].cfd;

      if (clientFd > 0 ) {
        FD_SET(clientFd, &read_set);
      }

      if (clientFd > fdmax) {
        fdmax = clientFd;
      }
    }

    fd_set exept_set = read_set;

    rc = select(fdmax + 1, &read_set, NULL, &exept_set, NULL);
    if (rc < 0) {
      perror("Failed to select sockets\n");
      exit(-1);
    }

    if (FD_ISSET(tcpFd, &read_set)) {
      if (FD_ISSET(tcpFd, &exept_set)) {
        exit(-1);
      }
      // new TCP connection
      int newFd = accept(tcpFd, NULL, NULL);
      if (newFd < 0) {
        perror("Failed to accept socket from client\n");
        exit(-1);
      }
      clientSes.push_back(Session(newFd));
    }

    if (FD_ISSET(udpFd, &read_set)) {
      if (FD_ISSET(udpFd, &exept_set)) {
        exit(-1);
      }
      // new UDP connection
      int received = recvfrom(udpFd, inputBuffer, sizeof(inputBuffer), 0,
      (struct sockaddr *)&clientaddr, &clen);
      if (received < 0) {
        perror("Failed to receive command from client\n");
        exit(-1);
      }
      // unlock
      char outputBuffer[BUFLEN];
      string input = inputBuffer;
      string firstWord = input.substr(0, input.find(" "));
      if (firstWord == "unlock") {
        char unlock[10];
        int cardNo;
        sscanf (inputBuffer,"%s %d", unlock, &cardNo);
        // check if card number exists
        std::map<int, User>::iterator it;
        it = userMap.find(cardNo);
        if (it == userMap.end()) {
          // card number doesn't exist
          strcpy(outputBuffer, "-4: Numar card inexistent\n\n");
          sendto(udpFd, outputBuffer, strlen(outputBuffer) - 1, 0, (struct sockaddr *)&clientaddr, clen);
        } else {
          // check if card is blocked
          auto it = userMap.find(cardNo);
          if(it != userMap.end()) {
            if (it->second.blocked == true) {
              // ask secret pass
              strcpy(outputBuffer, "Trimite parola secreta\n\n");
            } else {
              strcpy(outputBuffer, "-6: Operatie esuata\n\n");
            }
            sendto(udpFd, outputBuffer, strlen(outputBuffer) - 1, 0, (struct sockaddr *)&clientaddr, clen);
          }
          for (int i = 0 ; i < clientSes.size(); i++) {
            if (clientSes[i].cardNumber == cardNo) {
              sendto(udpFd, outputBuffer, strlen(outputBuffer) - 1, 0, (struct sockaddr *)&clientaddr, clen);
              break;
            }
          }
        }
      } else { // secret pass has been sent
        char pass[10];
        int cardNo;
        sscanf (inputBuffer,"%d %s", &cardNo, pass);
        if (strcmp(userMap.find(cardNo)->second.pass.c_str(), pass) == 0) {
          User &user = userMap.find(cardNo)->second;
          user.blocked = false;
          strcpy(outputBuffer, "Card deblocat\n\n");
        } else {
          strcpy(outputBuffer, "-7: Deblocare esuata\n\n");
        }
        sendto(udpFd, outputBuffer, strlen(outputBuffer) - 1, 0, (struct sockaddr *)&clientaddr, clen);
      }
    }

    if (FD_ISSET(STDIN_FILENO, &read_set)) {

    }

    for(int i = 0; i < clientSes.size(); i++) {
      int clientFd = clientSes[i].cfd;

      if (FD_ISSET(clientFd, &read_set)) {
        if (FD_ISSET(clientFd, &exept_set)) {
          FD_CLR(clientFd, &read_set);
          close(clientFd);
        }
        // manage commands
        rc = recv(clientFd, inputBuffer, BUFLEN, 0);
        if (rc < 0) {
          perror("Failed to receive command from client\n");
          exit(-1);
        }
        processCommand(inputBuffer, clientSes[i], clientSes);
      }
    }
  }
}


int main(int argc, char* argv[]) {

  int portNo = atoi(argv[1]);
  string fileName = argv[2];

  parseFile(fileName);

  int tcpFd = socket(AF_INET, SOCK_STREAM, 0);
  int udpFd = socket(AF_INET, SOCK_DGRAM, 0);

  if (tcpFd < 0 || udpFd < 0) {
    perror("Failed to create socket\n");
    exit(-1);
  }

  struct sockaddr_in serveraddr;
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(portNo);
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

  int rc;
  int enable = 1;
  rc = setsockopt(tcpFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

  if(rc < 0) {
    perror("Failed to set option to TCP socket\n");
    exit(-1);
  }

  rc = bind(tcpFd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
  if(rc < 0) {
    perror("Failed to bind TCP socket\n");
    exit(-1);
  }

  rc = bind(udpFd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
  if(rc < 0) {
    perror("Failed to bind UDP socket\n");
    exit(-1);
  }

  rc = listen(tcpFd, MAX_CONNECTIONS);
  if (rc < 0) {
    perror("Failed to listen to TCP socket\n");
    exit(-1);
  }

  manageConnections(tcpFd, udpFd);

  close(tcpFd);
  close(udpFd);

  return 0;
}
