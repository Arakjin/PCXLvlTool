# Wings-kenttäohje

## Kentän koko ja asetukset

Wings-kentän koko voi olla 157 × 90 – 1000 × 1000 pikseliä. Oletuskoko on
400 × 400. Koko sekä tähdet, sade, lumi, pommitus, siviilit ja aseistettujen
siviilien todennäköisyys asetetaan uuden kentän dialogissa. Niitä voi muuttaa
myöhemmin valinnalla **Level > Wings level settings**.

Kentän nimi toimii julkaistavan tiedostonimen ehdotuksena. Muokattava työ
tallennetaan `.pxlp`-projektiksi ja peliin menevä tiedosto tehdään valinnalla
**Publish LEV**.

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

Valitse materiaali toiminnan eikä pelkän näkyvän värin perusteella. Vasen
hiiren painike käyttää ensisijaista ja oikea toissijaista materiaalia.

Wingsin alkuperäinen `COLORS.TXT` vahvistaa, että paletin indeksit 0–47 ovat
kiinteitä eikä niiden RGB-värejä saa muuttaa. Siksi editori estää myös indeksin
16 värin muokkaamisen: indeksi 16 on kenttään piirrettävä **luo vettä** -merkki,
ei vesipyssyn tai veden vapaasti muokattava väri.

Indeksien 48–255 RGB-värejä saa muuttaa. Alkuperäinen suomenkielinen ohje
nimeää indeksin 52 suoraan kuplien väriksi, joten sen muokkaaminen vaikuttaa
pelin piirtämiin kupliin. Indeksi 53 on lumi ja on muokattavalla alueella,
mutta sääefektin lumihiutaleiden saman palettipaikan käyttö pitää vielä
vahvistaa pelitestillä. Indeksit 48–51 ovat veden ja sen virtausten värejä.

## Parallax-tausta

Kun **Parallax background** on valittu, editori luo automaattisesti erillisen
Background-välilehden. Sen koko lasketaan alkuperäisen MAKELEV-ohjeen mukaan:

```text
leveys  = kentän leveys / 2 + 78
korkeus = kentän korkeus / 2 + 45
```

Jakolasku käyttää kokonaislukujakoa. Varsinainen kenttä ja parallax-tausta
käyttävät samaa palettia, tallentuvat samaan `.pxlp`-projektiin ja julkaistaan
samaan Wings-LEV-tiedostoon. Varatut indeksit pysyvät poissa palettivalikosta
myös Background-välilehdellä.

Testaa sekä tavallinen että parallaxia käyttävä julkaisu Wingsissä. Tarkista
kentän rajat, tukikohdat, vesi, materiaalien tuhoutuminen sekä sää- ja
siviiliasetukset.
