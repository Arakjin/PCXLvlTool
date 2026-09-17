# Wings-kenttäohje

## Kentän koko ja asetukset

Wings-kentän koko voi olla 157 × 90 – 1000 × 1000 pikseliä. Oletuskoko on
400 × 400. Koko sekä tähdet, sade, lumi, pommitus, siviilit ja aseistettujen
siviilien todennäköisyys asetetaan uuden kentän ikkunassa. Niitä voi muuttaa
myöhemmin valinnalla **Level > Wings level settings**.

Wings-kentässä ei ole erillistä pelin sisäistä nimeä. Kenttä tunnistetaan
nimellä, jonka annat `.LEV`-tiedostolle julkaisemisen yhteydessä.

## Materiaalit

Wings käyttää paletti-indeksejä myös materiaalien toiminnan määrittämiseen.
Paletin ryhmävalikossa ovat esimerkiksi tukikohdat, vesi, läpilennettävä
tausta, rikkoutumaton, pehmeä, palava ja tavallinen maasto. Pelin varaamat
alueet 1–15, 17–31 ja 57–63 jätetään pois piirrettävistä vaihtoehdoista.

Tärkeimpiä dokumentoituja indeksejä ovat:

- 0: tausta
- 16: veden lähde
- 32–47: tukikohtien materiaalit
- 48–53: vesi, virtaukset, kuplat ja lumi
- 54: vahingoittava tulitausta
- 55–56: räjähtävä maasto
- 64–79: läpilennettävä tausta
- 80–95: rikkoutumaton maasto
- 96–111: pehmeä maasto
- 112–127: palava maasto
- 128–255: tavallinen maasto

Valitse materiaali toiminnan eikä pelkän näkyvän värin perusteella.

Paletin indeksien 0–47 värit ovat kiinteitä. Siksi värinmuokkauspainike ei ole
käytettävissä näille materiaaleille. Materiaali 16 on kenttään piirrettävä
**luo vettä** -merkki.

Indeksien 48–255 värejä saa muuttaa. Indeksi 52 määrää pelin piirtämien kuplien
värin ja indeksi 53 satavan lumen värin. Indeksit 48–51 ovat veden ja sen
virtausten värejä.

## Parallax-tausta

Kun **Parallax background** on valittu, editori luo automaattisesti erillisen
Background-välilehden ja laskee sille Wingsin vaatiman koon.
Varsinainen kenttä ja parallax-tausta käyttävät samaa palettia, tallentuvat
samaan `.pxlp`-projektiin ja julkaistaan samaan Wings-LEV-tiedostoon. Varatut
indeksit pysyvät poissa palettivalikosta myös Background-välilehdellä.

Testaa sekä tavallinen että parallaxia käyttävä julkaisu Wingsissä. Tarkista
kentän rajat, tukikohdat, vesi, materiaalien tuhoutuminen sekä sää- ja
siviiliasetukset.
