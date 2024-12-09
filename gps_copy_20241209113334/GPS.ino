#include <TinyGPS++.h>  // Inclure la bibliothèque TinyGPS++
#include <wiring_private.h>

// Créer un objet GPS
TinyGPSPlus gps;
unsigned long previousMillis = 0;  // Variable pour stocker le dernier temps d'envoi
const long interval = 12000;       // Intervalle en millisecondes (12 secondes)

Uart Serial2(&sercom1, 12, 10, SERCOM_RX_PAD_3, UART_TX_PAD_2); // RX = D11, TX = D10

void SERCOM1_Handler() {
  Serial2.IrqHandler(); // Gérer les interruptions pour SERCOM2
}

void setup() {
  // Initialiser la communication série avec le GPS
  Serial2.begin(9600);  // Utiliser Serial2 pour le GPS
  Serial.begin(9600);  // Moniteur série pour voir les données
  pinPeripheral(12, PIO_SERCOM); // RX (D11)
  pinPeripheral(10, PIO_SERCOM); // TX (D10)

  Serial.println("Démarrage du GPS...");
}

void loop() {
  // Vérifier si 12 secondes se sont écoulées
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;  // Enregistrer le temps actuel

    // Lire toutes les données disponibles du GPS
    while (Serial2.available() > 0) {
      char c = Serial2.read();
      gps.encode(c);  // Décoder les trames NMEA
    }

    // Si on a des coordonnées valides, les afficher
    if (gps.location.isValid()) {
      Serial.println("\nCoordonnées GPS trouvées !");
      Serial.print("Latitude: ");
      Serial.println(gps.location.lat(), 6);  // 6 décimales pour précision
      Serial.print("Longitude: ");
      Serial.println(gps.location.lng(), 6);
    } else {
      // Afficher le message d'attente et le nombre de satellites détectés
      Serial.print("En cours, discute avec satellite... ");
      Serial.print("Satellites visibles : ");
      Serial.println(gps.satellites.value());  // Nombre de satellites détectés
    }
  }
}