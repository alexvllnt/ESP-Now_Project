/* ---- Declaration of librairies ---- */
#include <esp_now.h>
#include <WiFi.h>

/* ---- Declaration of constants ---- */
/*
    In this program, we use two constants :
    - MAC_ADDRESS_LENGTH, the length of a MAC address.
    - CHAR_LENGTH, the maximum length for the char array.
*/
#define MAC_ADDRESS_LENGTH 6
#define CHAR_LENGTH 40

/* ---- Declaration of variables ---- */
/*
   In this program, we use five global variables :
   - receiverAddress, the receiver MAC address. Initially, it sets to the broadcast address. It becomes the master MAC address if the board is a slave.
   - myMacAddress, the MAC address of the board. It will be used during ESP communications to identify the board.
   - ESPstatus, determines whether the board is master (1) or slave (-1) during ESP communications. Initially, it sets to 0 as long as the status is indeterminate.
   - myID, the ID of the board. It is used to identify the board during data recovery. Initially, it sets to -1 as long as the status is indeterminate.
   - master, a bool that represents the presence (or not) of a master on the ESP network.
   - peerInfo, contains all the information about the chanel (cryptage, the address of communication...).
*/
uint8_t receiverAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
String myMacAddress = String(WiFi.macAddress());
int ESPstatus = 0;
int myID = -1;
bool master = false;
esp_now_peer_info_t peerInfo = {};

/* ---- Definition of the message structure ---- */
/*
   An ESP message is composed of four elements :
   - senderID, the ID of the board who sends the message.
   - receiverID, the ID of the board to xhich the message is addressed.
   - typeMessage, it is used to classify the different categories of message send on the ESP network.
   - message, the content of the ESP message. This can be for example a mac address or others.
*/
typedef struct structMessage {
  int senderID;
  int receiverID;
  char typeMessage[CHAR_LENGTH];
  char message[CHAR_LENGTH];
} structMessage;

/* ---- Definition of the buoy structure and the buoy list ---- */
/*
   The master records the infos of all the slaves in a linked list. Initially, the linked list is empty.
   Thanks to this list the master can a link between the MAC address and the ID of every buoy.
   Each element (or buoy) of the list is composed of four fields :
   - buoyID, the ID of the buoy. It has been chosen to assign the ID 0 to the master.
   - buoyName, the name of the buoy if there is a need to differentiate them other than by their ID. Currently, every buoy name has the same structure : "Buoy n°" + its buoyID.
   - buoyMacAddress, the MAC address of the buoy, unique board identifier.
*/
typedef struct structBuoy {
  int buoyID;
  char buoyName [CHAR_LENGTH];
  uint8_t buoyMacAddress[MAC_ADDRESS_LENGTH];
  struct structBuoy *nxt;
} structBuoy;

typedef structBuoy* structBuoyList;
structBuoyList buoyList = NULL;

/* ---- Procedure for counting the number of buoys in the list ---- */
/*
   INPUT : the buoy list (structBuoyList)
   OUTPUT : the number of buoys present in the list (int).
   DESCRITPION : If the buoy list is empty then the procedure return 0, else the program recursively goes through the list to count the buoys.
*/
int nbBuoys(structBuoyList myList) {
  /*if the buoy list is empty...*/
  if (myList == NULL)
    /*...then it returns 0.*/
    return 0;
  /*...else it recursively goes through the list to count the buoys*/
  return nbBuoys(myList->nxt) + 1;
}

/* ---- Procedure for printing a Mac Address ---- */
/*
   INPUT : the MAC address to print (table of uint8_t(Unsigned Integers of 8 bits)).
   OUTPUT : nothing (void).
   DESCRITPION : The program goes through the table. If the character is smaller than 16 (because a MAC address is in hexadecimal) the program prints "0".
   Every MAC address element is composed of two alphanumeric characters. After printing the character, the program checks that it is not the last character.
   If it is not the case, it print ":" to separate the different characters.
   NB : "HEX" is used to display the character in hexadecimal.
*/
void printMacAddress(uint8_t addressMac[]) {
  for (int i = 0; i < MAC_ADDRESS_LENGTH; i++) {
    /*if the MAC address element is greater smaller than 16...*/
    if (addressMac[i] < 16) {
      /*...then the program prints a "0" to have two characters*/
      Serial.print("0");
    }
    /*it prints the MAC address element in a hexadecimal format*/
    Serial.print(addressMac[i], HEX);
    /*if it is not the last element of the table...*/
    if (i != MAC_ADDRESS_LENGTH - 1)
      /*...then it prints a ":" symbol to the monitor.*/
      Serial.print(":");
    else
      /*...else it does a break line*/
      Serial.println();
  }
}

/* ---- Procedure for modifying a Mac Address ---- */
/*
   INPUT : the MAC address copied (table of uint8_t), the MAC address to copy (String).
   OUTPUT : nothing (void).
   DESCRITPION : The program goes through the table. The ith element of the table corresponds to (3*i)th and (3*i+1)th characters of the String.
   The program checks for each character if the character is between 0 and 9 or A and F. If the character is a letter then it substracts 55 else only 48.
   This allows the correct ASCII code of the character to be found. Finally the program adds the two character with the correct power to have the ith element of the table.
   NB : hex1 (16 pow 1) and hex0 (16 pow 0) represent each alphanumeric of a MAC address character.
   The function .charAt(n) returns the nth character of the string.
*/
void modifMacAddress(uint8_t addressMac[], String stringMacAddress) {
  int hex1, hex0;
  for (int i = 0; i < MAC_ADDRESS_LENGTH; i++) {
    /*if the first alphanumeric element is a letter...*/
    if ((int)stringMacAddress.charAt(3 * i) > 63)
      /*...then it substracts 55*/
      hex1 = (int)stringMacAddress.charAt(3 * i) - 55;
    /*...else it substracts 48*/
    else
      hex1 = (int)stringMacAddress.charAt(3 * i) - 48;
    /*if the second alphanumeric element is a letter...*/
    if ((int)stringMacAddress.charAt(3 * i + 1) > 63)
      /*...then it substracts 55*/
      hex0 = (int)stringMacAddress.charAt(3 * i + 1) - 55;
    else
      /*...else it substracts 48*/
      hex0 = (int)stringMacAddress.charAt(3 * i + 1) - 48;
    /*hex1 and hex0 are used to calculated the ith element of the MAC address*/
    addressMac[i] = 16 * hex1 + hex0;
  }
}


/* ---- Procedure for converting a Mac Address ---- */
/*
   INPUT : a MAC address (table of uint8_t).
   OUTPUT : a MAC address converted (String).
   DESCRITPION : The program goes through the table. For every element of the table, the program calculates hex0 as the rest of the Euclidean division by 16 and hex1 as the quotient.
   The program expresses each number in hexadecimal. Then they are concatnated in a string and if they are not the last MAC character, a ":" symbol is added to the String.
   NB : hex1 (16 pow 1) and hex0 (16 pow 0) represent each alphanumeric of a MAC address character.
   The function .toUpperCase() returns an upper-case version of a String.
*/
String macAddressToString(uint8_t addressMac[]) {
  String stringMacAddress = "";
  String hex1, hex0;
  for (int i = 0; i < MAC_ADDRESS_LENGTH; i++) {
    /*hex0 is calculated as the rest of the Euclidean division by 16, expressed in hexadecimal*/
    hex0 = String(addressMac[i] % 16, HEX);
    /*if the character is a letter...*/
    if (addressMac[i] % 16 > 9) {
      /*... then it is capitalised.*/
      hex0.toUpperCase();
    }
    /* and hex1 quotient, expressed in hexadecimal*/
    hex1 = String((addressMac[i] - addressMac[i] % 16) / 16, HEX);
    /*if the character is a letter...*/
    if ((addressMac[i] - addressMac[i] % 16) / 16 > 9) {
      /*... then it is capitalised.*/
      hex1.toUpperCase();
    }
    /*hex1 and hex0 are concatenated to the result*/
    stringMacAddress += hex1 + hex0;
    /*if it is not the last element of the table...*/
    if (i != MAC_ADDRESS_LENGTH - 1)
      /*... then it adds a ":" symbol to the result.*/
      stringMacAddress += ":";
  }
  return stringMacAddress;
}

/* ---- Procedure for initialising the buoyList and adding a buoy in the list ---- */
/*
   INPUT : the buoy list (structBuoyList), the MAC address of the buoy (String).
   OUTPUT : the buoy list modified (structBuoyList).
   DESCRITPION : Firstly the program creates a new buoy by fillling all the fields (ID, name...). It knows the ID using the nbBuoys function presented above.
   Then, if the list is empty it returns the new buoy else it goes through the list until the last element.
   Once arrived, the program creates a link between the last element and the new buoy, which is now the new last element of the list.
   NB : temp is a temporary pointer to go through the list.
*/
structBuoyList addNewBuoy(structBuoyList myList, String macAddressBuoy) {
  /*the program creates the new buoy, by allocating memories and filling the field of the new buoy*/
  structBuoy *newBuoy = (structBuoy*)malloc(sizeof(structBuoy));
  newBuoy->buoyID = nbBuoys(myList);
  String nameBuoy = "Buoy n°" + String(nbBuoys(myList));
  nameBuoy.toCharArray(newBuoy->buoyName, CHAR_LENGTH);
  modifMacAddress(newBuoy->buoyMacAddress, macAddressBuoy);
  newBuoy->nxt = NULL;
  /*if the list is empty...*/
  if (myList == NULL) {
    /*... then it returns the new buoy.*/
    return newBuoy;
  }
  else {
    /*...else it creates a temporary pointer to go through the list*/
    structBuoy* temp = myList;
    /*as long as the pointer is not on the last element of the list then...*/
    while (temp->nxt != NULL) {
      /*... it goes to the next element.*/
      temp = temp->nxt;
    }
    /*it adds the new buoy as the new last element of the list.*/
    temp->nxt = newBuoy;
    /*finally, it returns the buoy list modified.*/
    return myList;
  }
}

/* ---- Procedure for printing the buoyList ---- */
/*
   INPUT : the buoy list (structBuoyList).
   OUTPUT : nothing (void).
   DESCRITPION : The program goes through the whole buoy list and prints the information of every buoy while it is not the last element of the list.
   NB : temp is a temporary pointer to go through the list.
*/
void printBuoyList(structBuoyList myList)
{
  /*the program creates a temporary pointer to go through the list.*/
  structBuoy *temp = myList;
  Serial.println(F("------------- ID LIST -------------"));
  /*as long as the pointer is not on the last element of the list then...*/
  while (temp != NULL) {
    /*... it prints the infos of the buoy*/
    Serial.println("buoyID : " + String(temp->buoyID));
    Serial.println("buoyName : " + String(temp->buoyName));
    Serial.print(F("buoyMacAddress : "));
    printMacAddress(temp->buoyMacAddress);
    /*and goes to the next buoy.*/
    temp = temp->nxt;
    /*if it is not the last buoy...*/
    if (temp != NULL)
      /*...then it does a break line.*/
      Serial.println();
  }
  Serial.println(F("-----------------------------------"));
  Serial.println();
}

/* ---- Procedure for checking if a buoy is in the buoy list ---- */
/*
   INPUT : the buoy list (structBuoyList), the MAC address of the buoy sought (String).
   OUTPUT : the place of the buoy (int).
   DESCRITPION : The program goes through the whole buoy list and searches by comparing the mac addresses if a buoy is present.
   If it the case it returns its place else it returns -1.
   NB : The function compareTo() compares character by character two strings. It returns 0 if they are equal.
*/
int isBuoyExists(structBuoyList structBuoyList, String addressMac) {
  /*place is used to find the place of the buoy in the list.*/
  int place = 0;
  /*the program creates a temporary pointer to go through the list.*/
  structBuoy* temp = structBuoyList;
  /*as long as the pointer is not on the last element of the list then...*/
  while (temp != NULL) {
    /*...it checks if the MAC address of the buoy is the one required...*/
    if (macAddressToString(temp->buoyMacAddress).compareTo(addressMac) == 0)
      /*...then it returns the place of the buoy in the list (which is its ID).*/
      return place;
    /*...else it goes to the next buoy and increments place.*/
    temp = temp->nxt;
    place ++;
  }
  /*if the buoy has not been found in the list then it is not known and the value -1 is returned.*/
  return -1;
}

/* ---- Procedure for printing the board informations ---- */
/*
   INPUT : nothing(void).
   OUTPUT : nothing(void).
   DESCRITPION : The programs print on the serial monitor the information about the board as its ID, its ESP status in the ESP-NOW communications and its MAC address.
*/
void printBoardInfo() {
  Serial.println(F("------ new board informations ------"));
  Serial.print(F("myID : "));
  if (myID != -1)
    Serial.println(String(myID));
  else Serial.println(F("Unattributed"));
  Serial.print(F("ESPstatus : "));
  if (ESPstatus == -1)
    Serial.println(F("Slave"));
  else if (ESPstatus == 0)
    Serial.println(F("Unattributed"));
  else Serial.println(F("Master"));
  Serial.println("MAC address : " + myMacAddress);
  Serial.println(F("------------------------------------"));
  Serial.println();
}

/* Defintition of the type of received data (dataRcv) and data sent (myData) */
structMessage dataRcv;
structMessage myData;

/* ----- Define callbacks for sending data ----- */
/*
   DESCRITPION : define the behaviour when the card sends a message.
*/
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  /*if the data is not correctly sent...*/
  if (status != ESP_NOW_SEND_SUCCESS) {
    /*...then it prints an error.*/
    Serial.println(F("error sending"));
  }
  /*...else it does nothing.*/
}

/* ---- Define callbacks for received data ---- */
/*
   DESCRITPION : define the behaviour when the card receives a message.
   NB: The function toInt() converts a String into an integer.
   The function strcmp() returns 0 if two Strings are equal.
   The function indexOf() locates a String in antoher String. It returns -1 if not found.
   The functioin toCharArray() transforms a String into a char array.
   The function memcpy() copies a memory bloc.
*/
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  /*the program copies the message in a variable to manipulate it.*/
  memcpy(&dataRcv, incomingData, sizeof(dataRcv));
  /*if the receiver ID in the message is the same than the ID of the board...*/
  if (dataRcv.receiverID == myID) {
    /*...then it reads the message in function of the ESP status of the board.*/
    switch (ESPstatus) {
      /* case 1 : The buoy is a slave.*/
      case -1:
        /*if the type message received is a ID_REPLY and the message contains the MAC address of the board...*/
        if ((!strcmp(dataRcv.typeMessage, "ID_REPLY")) && (String(dataRcv.message).indexOf(myMacAddress) != -1)) {
          /*...then it removes the MAC address int the message to keep the new ID of the board.*/
          myID = String(dataRcv.message).substring(20, CHAR_LENGTH).toInt();
          /*and the program prints it in the monitor.*/
          Serial.println("my new ID is : " + String(myID));
          Serial.println();
        }
        break;
      /* case 2 : The buoy doesn't has a role yet.*/
      case 0:
        /*if the type message received is a MASTER_REPLY...*/
        if (!strcmp(dataRcv.typeMessage, "MASTER_REPLY")) {
          /*...then the boolean master becomes true.*/
          master = true;
          /*the board modifies the address of ESP communication to the master MAC address*/
          modifMacAddress(receiverAddress, dataRcv.message);
          /*if the new peer is not correctly added...*/
          memcpy(peerInfo.peer_addr, receiverAddress, 6);
          if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            /*...then it prints an error and does a return.*/
            Serial.println(F("Failed to add peer"));
            return;
          }
        }
        break;
      /* case 3 : The buoy is the master.*/
      case 1:
        /*IDslave is used to found a specific ID in the buoy list.*/
        int IDslave;
        /*reply is used for the ID_REPLY message.*/
        String reply = "";
        /*if the type message received is a MASTER_DETECTION...*/
        if (!strcmp(dataRcv.typeMessage, "MASTER_DETECTION")) {
          /*...then the program sends a MASTER_REPLY message.*/
          /*
            senderID = 0
            receiverID = -1
            typeMessage = MASTER_REPLY
            message = the MAC address of the board
          */
          myData.senderID = myID;
          myData.receiverID = -1;
          strcpy(myData.typeMessage, "MASTER_REPLY");
          myMacAddress.toCharArray(myData.message, CHAR_LENGTH);
          /*the message is sent to the broadcast address.*/
          esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));
          /*elsif the type message received is a ID_REQUEST...*/
        } else if (!strcmp(dataRcv.typeMessage, "ID_REQUEST")) {
          /*...then the program sends a ID_REPLY message.*/
          /*
            senderID = 0
            receiverID = -1
            typeMessage = ID_REPLY
            message = the MAC address of the slave + its ID
          */
          myData.senderID = myID;
          myData.receiverID = -1;
          strcpy(myData.typeMessage, "ID_REPLY");
          /*the programs verifies is the slave is in the buoy list thanks to its MAC address.*/
          IDslave = isBuoyExists(buoyList, dataRcv.message);
          /*if IDslave is different from -1, the slave is known...*/
          if (IDslave != -1) {
            /*...then IDslave is adding to the message.*/
            reply = String(dataRcv.message) + " : " + String(IDslave);
            /*the String is converted to a char array.*/
            reply.toCharArray(myData.message, CHAR_LENGTH);
            /*and the program prints the slave ID in the monitor.*/
            Serial.println("buoy known, buoyID = " + String(IDslave));
            Serial.println();

          } else {
            /*...else IDslave is -1, the slave is unknown...*/
            /*the slave is added to buoy list.*/
            buoyList = addNewBuoy(buoyList, dataRcv.message);
            /*the slave ID is calculated from the number of buoys in the list and added to the message.*/
            reply = String(dataRcv.message) + " : " + String(nbBuoys(buoyList) - 1);
            /*the String is converted to a char array.*/
            reply.toCharArray(myData.message, CHAR_LENGTH);
            /*and the program prints the new buoy list in the monitor.*/
            Serial.println(F("new buoy created, structBuoyList :"));
            printBuoyList(buoyList);
          }
          /*the message is sent to the broadcast MAC address.*/
          esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));
          break;
        }
    }
  }
  /*...else it does nothing.*/
}

/* ---- Init the ESP board ---- */
/*
   INPUT : nothing(void).
   OUTPUT : nothing(void).
   DESCRITPION : initialise the ESP board as the serial monitor and esthablish the ESP-NOW communication (callback functions and peer).
*/
void initBoard() {
  /* Init serial monitor */
  Serial.begin(115200);

  /* Set device as a Wi-Fi Station */
  WiFi.mode(WIFI_STA);

  /* Init ESP-NOW */
  /*if the init is not correctly set...*/
  if (esp_now_init() != ESP_OK) {
    /*...then it prints an error and does a return.*/
    Serial.println(F("Error initializing ESP-NOW"));
    return;
  }
  /*...else it does nothing.*/

  /* Init callback functions */
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  /* Register peer... */
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  /*
    the programs defines the parameters of the chanel :
    - the channel number used to send and receive the data.
    - the encryption of the data in the channel (false, unencrypted).
  */
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  /* Add peer */
  /*if the peer is not correctly added...*/
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    /*...then it prints an error and does a return.*/
    Serial.println(F("Failed to add peer"));
    return;
  }
}
