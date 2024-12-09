void setup() {
 // Initialiser la communication série avec l'ordinateur (moniteur série)
  Serial.begin(9600);
  while (!Serial);  // Attendre que le moniteur série soit prêt
//  
  // Initialiser la communication série avec l'Arduino via RX/TX (pins 0 et 1)
  Serial1.begin(9600);  // Serial1 est utilisé sur la Feather M0 pour la communication avec l'Arduino
  Serial.println("Feather M0 prêt à recevoir des données...");
}
//
void loop() {
  // Vérifier si des données sont reçues via Serial1 (RX/TX entre la Feather M0 et l'Arduino)
  if (Serial1.available()) {
    // Lire les données reçues et les afficher sur le moniteur série
    String receivedData = "";
    while (Serial1.available()) {
      receivedData += (char)Serial1.read();  // Lire caractère par caractère
    }
    Serial.println("Données reçues de l'Arduino :");
    Serial.println(receivedData);  // Afficher les données complètes reçues
  }

  delay(500);  // Attendre un peu avant de vérifier à nouveau
}