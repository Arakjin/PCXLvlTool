# AUTS-kenttäohje

## Kentän perusrakenne

AUTS-kenttä on aina 320 × 400 pikseliä. Pelin paletti on kiinteä, eikä sen
värejä voi muuttaa editorissa. AUTS-kentässä ei ole erillistä pelin sisäistä
nimeä. Kenttä tunnistetaan `.LEV`-tiedostonimestä, jonka tulee olla enintään
kahdeksan merkkiä pitkä. Turvallisia merkkejä ovat kirjaimet, numerot, alaviiva
ja yhdysmerkki.

Uusi kenttä saa kahden pikselin paksuisen rikkoutumattoman reunuksen indeksillä
7. **Publish LEV** palauttaa tämän reunan automaattisesti, vaikka sitä olisi
muokattu editorissa.

## Materiaalit

Dokumentoidut tärkeät indeksit ovat:

- 0: avaruus
- 7: rikkoutumaton materiaali
- 39: vesi
- 92–95: telakointilevy

Paletin ryhmävalikko kokoaa nämä suoraan valittaviksi. Telakointilevyn värejä
92–95 ei pidä sekoittaa saman harmaan sävyisiin tavallisen maaston väreihin
108–111. Sama näkyvä väri ei siis takaa samaa pelillistä toimintaa.

## Veden rajoitukset

Yhdessä kentässä saa olla enintään seitsemän veden pintaa. Yhden pinnan tulee
olla alle 100 pikseliä pitkä. Aaltojen pitää pysyä yli viiden pikselin päässä
katosta tai koskettaa kattoa.

## Kuvan tuonti

AUTS-tuonti hyväksyy 320 × 400, 8-bittisen indeksoidun PCX-kuvan tai
pakkaamattoman indeksoidun BMP-kuvan. Pikseli-indeksit säilytetään, mutta
editori ottaa käyttöön kiinteän AUTS-paletin ja lisää suojareunan.

Testaa julkaistu kenttä AUTSissa ja tarkista erityisesti reunus, vesi,
telakointilevyt ja aluksen törmäykset.
