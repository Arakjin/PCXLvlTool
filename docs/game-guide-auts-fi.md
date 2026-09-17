# AUTS-kenttäohje

## Kentän perusrakenne

AUTS-kenttä on aina 320 × 400 pikseliä ja käyttää alkuperäisen `BLANK.BMP`-
mallin kiinteää 256 värin palettia. AUTS-LEV ei sisällä kentän nimeä, joten
yläpalkin nimi toimii julkaistavan DOS-tiedostonimen ehdotuksena. Nimi voi olla
enintään kahdeksan DOS-turvallista merkkiä ja muutetaan julkaistaessa isoiksi
kirjaimiksi.

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
108–111. Sama RGB-väri ei siis takaa samaa pelillistä toimintaa.

## Veden rajoitukset

Alkuperäisen converter-ohjeen mukaan yhdessä kentässä saa olla enintään
seitsemän veden pintaa. Yhden pinnan tulee olla alle 100 pikseliä pitkä.
Aaltojen pitää pysyä yli viiden pikselin päässä katosta tai koskettaa kattoa.

## Kuvan tuonti

AUTS-tuonti hyväksyy 320 × 400, 8-bittisen indeksoidun PCX-kuvan tai
pakkaamattoman indeksoidun BMP-kuvan. Pikseli-indeksit säilytetään, mutta
editori ottaa käyttöön kiinteän AUTS-paletin ja lisää suojareunan. Jos
lähdekuvan paletti poikkeaa alkuperäisestä, editori varoittaa siitä. Käytä
materiaalien varmaan vastaavuuteen alkuperäistä `BLANK.BMP`-palettia.

Testaa julkaistu kenttä AUTSissa ja tarkista erityisesti reunus, vesi,
telakointilevyt ja aluksen törmäykset.
