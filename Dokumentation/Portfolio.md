* **Hashing af MAC-adresser**
    - Vi hasher MAC-adresser, inden de sendes til MQTT-serveren, for at undgå at sende persondata videre.
* **Opbevaring af MAC-adresser**
    - Formålet med indsamlingen er at kunne registrere antallet af unikke enheder i et område.
* **GDPR-regler**
    - Vi skal være opmærksomme på, om vi modtager persondata, samt sikre at data ikke bruges eller opbevares længere end nødvendigt.
    - Derfor indsamler vi kun den nødvendige data, som så vidt muligt beskyttes gennem hashing eller kryptering, og vi anvender en TLS-beskyttet MQTT-server.
* **Afgrænsning af MAC-adresser (sniffing)**
    - Vi har hardcodet de MAC-adresser, vi ønsker at registrere, da der ellers ville komme et meget stort antal forespørgsler, hvilket ville gøre det vanskeligt at identificere relevante matches.
    - På den måde undgår vi også at indsamle og gemme unødvendig data om andre enheder.
* **Hardcoding af master og slaver**
    - Da vi udfører trilaterale beregninger, skal noderne altid befinde sig på faste positioner. Derfor giver det mening at hardcode dem.
    - Det gør det samtidig lettere at identificere, hvilken node der eventuelt skaber fejl.
* **Oprettelse af secret-/CA-certifikatfil**
    - Vi har oprettet en separat fil til beskyttelse af MQTT-serverens certifikat samt loginoplysninger til både Wi-Fi og MQTT.
    - Filen beskytter også de hardcodede MAC-adresser.
    - Derudover er der tilføjet eksempel-filer, så andre brugere kan opsætte lignende miljøer.
* **Fjernelse af main**
    - I forbindelse med refactoring blev funktionaliteten opdelt i separate moduler. Da setup og loop ikke længere behøvede at ligge i main, endte main med at være tom og blev derfor fjernet.
* **Brug af én channel**
    - Vi valgte at benytte én channel frem for flere, så vi undgår at skulle synkronisere noderne gennem channel hopping.