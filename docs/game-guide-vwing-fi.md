# V-Wing-kenttäohje

## Kentän perusrakenne

V-Wing-kenttä on aina 640 × 800 pikselin kuva. Uuden kentän alin
Background-taso on musta.

Kentän nimi näkyy pelin kenttävalikossa. Nimi voi sisältää enintään 20
perusmerkkiä, eikä ääkkösiä hyväksytä. Editorissa nimi muutetaan julkaistaessa
isoiksi kirjaimiksi.

## Materiaalien valitseminen

Paletin ryhmävalikko kokoaa indeksit niiden pelillisen toiminnan mukaan.
Käytä esimerkiksi ryhmiä **Water**, **Fly through**, **Normal terrain**,
**Burnable**, **Underwater** ja **Indestructible** sen sijaan, että valitsisit
materiaalin pelkän näkyvän värin perusteella.

Tärkeimpiä materiaaliryhmiä ovat:

- 0: musta tausta
- 16–19: vesi ja veden virtaussuunnat
- 20–30: läpilennettävä alue
- 57–149: tavallinen maasto
- 151–174: pois palava maasto
- 176–199: tuhkaksi palava maasto
- 201–219: vedenalaiset materiaalit ja vedeksi tuhoutuva maasto
- 221–243 ja 248–255: rikkoutumaton maasto
- 244–247: pelin luomien tykkien värit

## Kentän ja pelihahmojen yhteiset palettivärit

V-Wing käyttää joitakin samoja palettivärejä sekä kentässä että pelin
hahmoissa ja efekteissä. Kun muutat tällaisen materiaalin väriä toiminnolla
**Edit selected color**, muutos näkyy kaikissa sitä käyttävissä kohteissa.
Luettelo kertoo värien yhteiskäytöstä, ei siitä, että materiaalilla piirtäminen
lisäisi kyseisen hahmon tai efektin kenttään:

- 39 ja 203: jää
- 46: lentävien lintujen väri
- 48: veri, mukaan lukien pilotin veri
- 51: tuhka
- 52: lumi
- 56: kuplat
- 244–247: satunnaisesti syntyvien tykkien osien värit

Esimerkiksi indeksin 46 muuttaminen punaiseksi tekee pelin lentävistä linnuista
punaisia. Indeksin 48 muuttaminen vihreäksi muuttaa myös pilotin veren
vihreäksi. Samalla tavalla indeksit 52 ja 56 vaikuttavat pelin lumeen ja
kupliin. Indekseillä 244–247 piirtäminen ei sijoita kenttään tykkejä, vaan peli
luo tykit omien sääntöjensä mukaan ja käyttää näitä värejä niiden ulkoasuun.

## Piirto-ohjeita

- Älä käytä suuria yhtenäisiä räjähtäviä tai palavia alueita, sillä ne voivat
  estää ampumisen.
- Käytä kahta palavaa materiaaliryhmää yhdessä, jotta rakennelmista jää
  tarkoituksenmukaisia raunioita.
- Veden alla olevan maaston kannattaa olla rikkoutumatonta tai tuhoutua
  vedeksi. Muuten kenttään voi syntyä epäluonnollisia tyhjiä kohtia.
- Tykkejä ei synny palavaan maastoon, tukikohtiin eikä vedenalaisiin
  materiaaleihin.
- Tee rakennelmista hieman tavallista korkeampia, jos kenttää pelataan
  320 × 400 -näyttötilassa.

Testaa julkaistu kenttä aina V-Wingissä ja tarkista erityisesti törmäykset,
vesi, palaminen ja räjähdykset.
