#include <SPI.h>
#include <MFRC522.h>
#include <SoftwareSerial.h>

// Pins pour le module RFID
#define SS_PIN 10   // SDA
#define RST_PIN 9   // RST

// Broches pour la communication série SoftwareSerial
#define RX_PIN 2
#define TX_PIN 3

MFRC522 rfid(SS_PIN, RST_PIN); // Instance de l'objet RFID
SoftwareSerial mySerial(RX_PIN, TX_PIN); // Communication série sur D2 (RX) et D3 (TX)

// Structure pour stocker l'UID et l'heure de détection
struct CardData {
  String uid;
  //unsigned long timestamp; // Temps en millisecondes
};

// Tableau pour stocker les cartes détectées
CardData detectedCards[10]; // Stocke jusqu'à 10 cartes
int cardCount = 0;

// Dernière fois où les cartes ont été affichées
unsigned long lastDisplayTime = 0;

void setup() {
  // Démarre la communication série SoftwareSerial
  mySerial.begin(9600);

  // Démarre la communication série avec le PC pour le moniteur série
  Serial.begin(9600); // Assurez-vous que le moniteur série utilise le même débit
  while (!Serial);      // Attend que le moniteur série soit prêt
  Serial.println("Moniteur série initialisé.");

  // Initialisation SPI et RFID
  SPI.begin();
  rfid.PCD_Init();

  mySerial.println("Lecteur RFID initialisé.");
  Serial.println("Lecteur RFID initialisé.");
}

void loop() {
  // Vérifie si une carte est présente et lisible
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    // Récupère l'UID de la carte
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      uid += String(rfid.uid.uidByte[i], HEX); // Convertit chaque octet en HEX
      if (i < rfid.uid.size - 1) uid += ":";  // Ajoute ":" entre les octets
    }
    detectedCards[cardCount].uid = uid;
    //detectedCards[cardCount].timestamp = millis();
    cardCount++;

    // Arrête la communication avec la carte pour permettre une nouvelle lecture
    rfid.PICC_HaltA();
  }

  // Affiche les cartes détectées toutes les 12 secondes
  if (millis() - lastDisplayTime >= 12000) {
    Serial.println("=== Liste des cartes détectées ===");
    for (int i = 0; i < cardCount; i++) {
      // Convertit le temps en secondes
      //unsigned long detectionTime = detectedCards[i].timestamp / 1000;
      Serial.print("Carte UID: ");
      Serial.println(detectedCards[i].uid);

      mySerial.print("Carte UID: ");
      mySerial.println(detectedCards[i].uid);
//      Serial.print(" - Détectée à: ");
//      Serial.print(detectionTime);
//      Serial.println(" secondes");
    }

    // Réinitialise la liste pour la prochaine période
    cardCount = 0;
    lastDisplayTime = millis();
  }
}
