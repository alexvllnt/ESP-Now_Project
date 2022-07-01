/* ---- Declaration of librairies ---- */
#include "utilities.h"

/* ---- Declaration of constants ---- */
/*
    In this sheet, we use one constant :
    - NUMBER_ATTEMPT_MAX, the number maximum of MASTER_DETECTION message sent to decide if there is a master or not.
*/
#define NUMBER_ATTEMPT_MAX 5

/* ---- Declaration of variables ---- */
/*
    In this sheet, we use one variable :
   - attempt, an integer used to count the number of attempts to detect if there is a master in the network.
*/
int attempt = 0;

void setup() {
  /*calling the InitBoard() function.*/
  initBoard();
  /*when the power is turned on, a delay is required.*/
  delay(1500);

  /*printing informations about the buoy after the initialisation.*/
  Serial.println();
  printBoardInfo();

  /* ---- STEP 1 : MASTER DETECTION ---- */

  /*as long as the board doesn't send 5 times a MASTER_DETECTION message and the boolean master is false then...*/
  while (!master && (attempt != NUMBER_ATTEMPT_MAX)) {
    /*...then it prints the number of attempt and sends a MASTER_DETECTION message.*/
    Serial.println("MASTER_DETECTION, attempt : " + String(attempt + 1));
    /*
      senderID = -1
      receiverID = 0
      typeMessage = MASTER_DETECTION
      message = the MAC address of the board
    */
    myData.senderID = myID;
    myData.receiverID = 0;
    strcpy(myData.typeMessage, "MASTER_DETECTION");
    myMacAddress.toCharArray(myData.message, CHAR_LENGTH);
    Serial.println(F("Sending message..."));
    Serial.println();
    /*the message is sent to the broadcast address.*/
    esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));
    attempt++;
    /*the program waits 1 second between each MASTER_DETECTION message.*/
    delay(1000);
  }

  /* ---- STEP 2 : ID ASSIGNMENT ---- */

  /*if the boolean master is true then...*/
  if (master) {
    /*...then the buoy becomes a slave and its ESP status becomes -1.*/
    ESPstatus = -1;
    Serial.println(F("master detected on the network, ID request in progress"));
    Serial.println();
    /*rndm is used to define the random delay in order to avoid collision during the ID requesto of the slave*/
    long rndm;
    Serial.println(F("permanent modification of receiverAddress"));
    /*printing the old address of communication.*/
    Serial.println(F("old receiverAddress : FF:FF:FF:FF:FF:FF"));
    /*printing the new address of communication.*/
    Serial.println("new receiverAddress : " + macAddressToString(receiverAddress));
    Serial.println();
    /*sending a ID_REQUEST message.*/
    /*
      senderID = -1
      receiverID = 0
      typeMessage = ID_REQUEST
      message = the MAC address of the board
    */
    strcpy(myData.typeMessage, "ID_REQUEST");
    myMacAddress.toCharArray(myData.message, CHAR_LENGTH);
    /*as long as the slave as an undefined ID (equal to -1) then...*/
    while (myID == -1) {
      /*...a random delay is established before the sending to avoid collision between the different ID_REQUEST message sent in the same time.*/
      rndm = random(0, 5);
      /*the procedure of ID assignment takes on average 8,35 ms so the delay is established as a multiple of 9 ms between 0 and 45 ms.*/
      delay(9 * rndm);
      /*the message is sent to the master MAC address.*/
      esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));
      Serial.println(F("ID_REQUEST, message sent!"));
      Serial.println();
      delay(50);
    }
  } else {
    /*...else the network doesn't have a master so the buoy becomes one.*/
    /*its ESP status changes to 1 and it ID to 0*/
    ESPstatus = 1;
    myID = 0;
    /*the program initialises the buoy list by adding the board as the first buoy in the list.*/
    Serial.println(F("no master detected on the network, creation of the IDlist."));
    Serial.println();
    /*and printing the list initialised.*/
    buoyList = addNewBuoy(buoyList, myMacAddress);
    Serial.println(F("IDlist created :"));
    printBuoyList(buoyList);
  }
  /*printing the new informations about the buoy.*/
  printBoardInfo();
}

void loop() {
}
