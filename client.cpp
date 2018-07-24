#include <iostream>
#include <fstream>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
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

enum State {
   unlogged,
   logging,
   logged,
   listsold,
   transferInitial,
   transferFinal,
   unlocking,
   unlockPass,
   logout,
   readState
};

int main(int argc, char* argv[]) {
  string ipServer = argv[1];
  int portServer = atoi(argv[2]);

  int rc;

  int tcpFd = socket(AF_INET, SOCK_STREAM, 0);
  int udpFd = socket(AF_INET, SOCK_DGRAM, 0);

  if (tcpFd < 0 || udpFd < 0) {
    perror("Failed to create socket\n");
    exit(-1);
  }

  struct sockaddr_in serveraddr;
  socklen_t slen = sizeof(serveraddr);
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons((unsigned short)portServer);
  rc = inet_aton(ipServer.c_str(), &serveraddr.sin_addr);
  if(rc < 0) {
    perror("INET_ATON error\n");
    exit(-1);
  }

  rc = connect(tcpFd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
  if (rc < 0) {
    perror("Failed to connect to server\n");
    exit(-1);
  }


  char fileName[20];
  int n = sprintf(fileName, "client-%d.log", getpid());
  ofstream fout(fileName);

  State state = unlogged;
  int cardNo = 0;

  char inputBuffer[BUFLEN], outputBuffer[BUFLEN];
  int f = 0;
  while(1) {

    fd_set read_set;
    int fdmax;

    FD_ZERO(&read_set);
    FD_SET(tcpFd, &read_set);
    FD_SET(udpFd, &read_set);
    FD_SET(STDIN_FILENO, &read_set);

    fdmax = max(tcpFd, udpFd);

    fd_set exept_set = read_set;
    memset(inputBuffer, 0, BUFLEN);
    rc = select(fdmax + 1, &read_set, NULL, &exept_set, NULL);
    if (rc < 0) {
      perror("Failed to select sockets\n");
      exit(-1);
    }
    if (FD_ISSET(tcpFd, &read_set)) {
      // process server responses
      memset(inputBuffer, 0, BUFLEN);
      rc = recv(tcpFd, inputBuffer, BUFLEN, 0);
      if(f == 1) {
        cout << "IBANK> " << inputBuffer;
        fout << "IBANK> " << inputBuffer;
        f = 0;
      }
      if (rc < 0) {
        perror("Failed to receive message from server\n");
        exit(-1);
      }

      switch(state) {
        case logging:
          // check if server sends error
          if (inputBuffer[0] == '-') {
            state  = unlogged;
          } else {
            state = logged;
          }
          cout << "IBANK> " << inputBuffer;
          fout << "IBANK> " << inputBuffer;
          break;

        case logout:
          if (inputBuffer[0] != '-') {
            state = unlogged;
            cardNo = 0;
          }

          cout << "IBANK> " << inputBuffer;
          fout << "IBANK> " << inputBuffer;
          break;

        case listsold:
          cout << "IBANK> " << inputBuffer << "\n";
          fout << "IBANK> " << inputBuffer << "\n";
          break;

        case transferInitial:
          cout << "IBANK> " << inputBuffer;
          fout << "IBANK> " << inputBuffer;
          if(strncmp(inputBuffer, "Transfer", 8) == 0) {
            cin.getline(outputBuffer, BUFLEN);
            if(outputBuffer[0] != 'y') {
              cout << "IBANK> " << "-9 : Operatie anulata\n\n";
              fout << "IBANK> " << "-9 : Operatie anulata\n\n";
            } else {
              f = 1;
              state = transferFinal;
              send(tcpFd, outputBuffer, 1, 0);
            }
          }
          break;
        default:
          break;

      }
    }

    if (FD_ISSET(udpFd, &read_set)) {
      memset(inputBuffer, 0, BUFLEN);
      rc = recvfrom(udpFd, inputBuffer, sizeof(inputBuffer), 0,
      (struct sockaddr *)&serveraddr, &slen);
      if (rc < 0) {
        perror("Failed to receive command from client\n");
        exit(-1);
      }

      switch(state) {
        case unlocking:
        cout << "UNLOCK> " << inputBuffer << "\n";
        fout << "UNLOCK> " << inputBuffer << "\n";

        if(strncmp(inputBuffer, "Trimite", 7) == 0) {
        char pass[10];
        cout << "Enter secret pass:";
        cin.getline(pass, 10);
        strcpy(outputBuffer, (to_string(cardNo) + " ").c_str());
        strcat(outputBuffer, pass);
        rc = sendto(udpFd, outputBuffer, strlen(outputBuffer) + 1, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
        if (rc < 0) {
          perror("Failed to send unlock to server\n");
          exit(-1);
        }
      }
        break;
      default:
        break;
    }
  }

    if (FD_ISSET(STDIN_FILENO, &read_set)) {
      memset(outputBuffer, 0, BUFLEN);
      cin.getline(outputBuffer, BUFLEN);

      std::string input = outputBuffer;
      std::string firstWord = input.substr(0, input.find(" "));

      if (firstWord == "quit") {
        // inform server
        rc = send(tcpFd, outputBuffer, strlen(outputBuffer) + 1, 0);
        if (rc < 0) {
          perror("Failed to send quit to server\n");
          exit(-1);
        }
        // stop
        break;
      } else if (firstWord == "login") {

        state = logging;
        // get the card number, needed by udp for unlock
        char login[5]; int pin; // just for pasing
        sscanf(outputBuffer,"%s %d %d", login, &cardNo, &pin);
        // request login
        rc = send(tcpFd, outputBuffer, strlen(outputBuffer) + 1, 0);
        if (rc < 0) {
          perror("Failed to send quit to server\n");
          exit(-1);
        }

      } else if (firstWord == "logout") {

        if (state == unlogged || state == logging) {
          // unlogged
          fout << "-1: Clientul nu este autentificat\n\n";
        } else {
          // request logout
          rc = send(tcpFd, outputBuffer, strlen(outputBuffer) + 1, 0);
          state = logout;
          if (rc < 0) {
            perror("Failed to send logout to server\n");
            exit(-1);
          }
        }

      } else if (firstWord == "transfer") {
        if (state == unlogged || state == logging) {
          // unlogged
          fout << "-1: Clientul nu este autentificat\n\n";
          cout << " -1: Clientul nu este autentificat\n\n";
        } else {
          state = transferInitial;
          rc = send(tcpFd, outputBuffer, strlen(outputBuffer) + 1, 0);
          if (rc < 0) {
            perror("Failed to send transfer to server\n");
            exit(-1);
          }
        }
      } else if (firstWord == "listsold") {
        // //cout state;
        if (state == unlogged || state == logging) {
          // unlogged
          fout << "-1: Clientul nu este autentificat\n\n";
          cout << " -1: Clientul nu este autentificat\n\n";
        } else {
          // request listsold
          state = listsold;
          rc = send(tcpFd, outputBuffer, strlen(outputBuffer) + 1, 0);
          if (rc < 0) {
            perror("Failed to send listsold to server\n");
            exit(-1);
          }
        }
      } else if (firstWord == "unlock") {
          state = unlocking;
          strcat(outputBuffer, (" " + to_string(cardNo)).c_str());
          rc = sendto(udpFd, outputBuffer, strlen(outputBuffer) + 1, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
          if (rc < 0) {
            perror("Failed to send unlock to server\n");
            exit(-1);
          }
      }

    }
  }

  close(tcpFd);
  close(udpFd);

  return 0;
}
