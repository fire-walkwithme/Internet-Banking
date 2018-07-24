#include <iostream>
#include <string>
#include <map>

#define MAX_CONNECTIONS 5
#define BUFLEN 200

using namespace std;

struct User {
  string lastName;
  string firstName;

  int cardNumber;
  int pin;
  string pass;
  double sold;
  bool blocked = false;

	User(string _lastName, string _firstName,
    int _cardNumber, int _pin, string _pass, double _sold):
    lastName(_lastName), firstName(_firstName),
    cardNumber(_cardNumber), pin(_pin), pass(_pass), sold(_sold) {}
};

struct Session {
  int cfd;
  bool logged   = false;
  int cardNumber = 0;
  int attemps = 3;
  double sumToTransfer = 0;
  int destCardNumber = 0;
  Session(int _cfd) : cfd(_cfd) {}
};

map<int, User> userMap;
map<int, string> errorMap = { {-1, "Clientul nu este autentificat"},
                              {-2, "Sesiune deja deschisa"},
                              {-3, "Pin gresit"},
                              {-4, "Numar card inexistent"},
                              {-5, "Card blocat"},
                              {-6, "Operatie esuata"},
                              {-7, "Deblocare esuata"},
                              {-8, "Fonduri insuficiente"},
                              {-9, "Operatie anulata"},
                              {-10, "Eroare la apel nume-functie"}};
