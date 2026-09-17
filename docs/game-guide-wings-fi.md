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
tausta, rikkoutumaton, pehmeä, palava ja tavallinen maasto. Kiinteät ja
varatut indeksit on poistettu valittavista vaihtoehdoista.

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

## Editorin helpotukset

- Kentän ja parallax-taustan oikeat mitat lasketaan ja validoidaan
  automaattisesti.
- Varattuja indeksejä ei voi valita numero- tai palettivalikosta.
- Palettiryhmät näyttävät materiaalit niiden toiminnan mukaan.
- `Shift` tekee viivoista vaaka-, pysty- tai 45 asteen suuntaisia sekä
  suorakulmioista neliöitä ja ellipseistä ympyröitä.
- Tasot säilyvät projektissa erillisinä. Julkaisu yhdistää vain näkyvät tasot.

Testaa sekä tavallinen että parallaxia käyttävä julkaisu Wingsissä. Tarkista
kentän rajat, tukikohdat, vesi, materiaalien tuhoutuminen sekä sää- ja
siviiliasetukset.

