# V-Wing-kenttäohje

## Kentän perusrakenne

V-Wing-kenttä on aina 640 × 800 pikselin indeksoitu kuva. Uuden kentän
Background-taso täytetään tiedostoindeksillä 0, joka on pelissä aina musta.
Kentän nimi näkyy pelin kenttävalikossa. Nimi voi sisältää enintään 20
tulostettavaa ASCII-merkkiä, eikä ääkkösiä hyväksytä. Editorissa nimi muutetaan
julkaistaessa isoiksi kirjaimiksi.

Tallenna muokattava työ `.pxlp`-projektiksi. **Publish LEV** yhdistää näkyvät
tasot ja kirjoittaa V-Wingin ymmärtämän `.LEV`-tiedoston.

## Materiaalien valitseminen

Paletin ryhmävalikko kokoaa indeksit niiden pelillisen toiminnan mukaan.
Käytä esimerkiksi ryhmiä **Water**, **Fly through**, **Normal terrain**,
**Burnable**, **Underwater** ja **Indestructible** sen sijaan, että valitsisit
materiaalin pelkän RGB-värin perusteella.

Tärkeimpiä materiaaliryhmiä ovat:

- 0: musta tausta
- 16–19: vesi ja veden virtaussuunnat
- 20–30: läpilennettävä alue
- 57–149: tavallinen maasto
- 151–174: pois palava maasto
- 176–199: tuhkaksi palava maasto
- 201–219: vedenalaiset materiaalit ja vedeksi tuhoutuva maasto
- 221–243 ja 248–255: rikkoutumaton maasto
- 244–247: tykkien spritejen värit

Palettipaneeli näyttää valitun materiaalin tiedostoindeksin ja kuvauksen.
Vasen hiiren painike käyttää ensisijaista ja oikea toissijaista materiaalia.
Kumi palauttaa alimman Background-tason indeksiksi 0.

## Kentän ja pelihahmojen yhteiset palettivärit

V-Wing käyttää osaa kentän paletista myös spriteihin ja efekteihin. Indeksin
RGB-värin muuttaminen vaikuttaa siis kaikkiin pelin saman paletti-indeksin
käyttökohteisiin, ei vain kenttään piirrettyihin pikseleihin. Näihin kuuluvat
ainakin:

- 39 ja 203: jää
- 46: lentävien lintujen väri
- 48: veri, mukaan lukien pilotin veri
- 51: tuhka
- 52: lumi
- 56: kuplat
- 244–247: satunnaisesti syntyvien tykkien osien värit

Indekseillä 244–247 piirtäminen ei sijoita kenttään valmiita tykkejä. Peli
luo tykit omien sääntöjensä mukaan, ja nämä neljä palettipaikkaa määräävät
niiden ulkoasun. Sama periaate koskee muiden yhteisten palettipaikkojen
värinmuutosta: esimerkiksi indeksin 46 muuttaminen punaiseksi tekee lentävistä
linnuista punaisia ja indeksin 48 muuttaminen vihreäksi tekee myös pilotin
verestä vihreää.

## Alkuperäisen converterin piirto-ohjeet

- Älä käytä suuria yhtenäisiä räjähtäviä tai palavia alueita.
- Käytä kahta palavaa materiaaliryhmää yhdessä, jotta rakennelmista jää
  tarkoituksenmukaisia raunioita.
- Veden alla olevan maaston kannattaa olla rikkoutumatonta tai tuhoutua
  vedeksi. Muuten kenttään voi syntyä epäluonnollisia tyhjiä kohtia.
- Tykkejä ei synny palavaan maastoon, tukikohtiin eikä vedenalaisiin
  materiaaleihin.
- Alkuperäinen ohje suosittelee tekemään rakennelmista hieman tavallista
  korkeampia, jos kenttää pelataan 320 × 400 -näyttötilassa.

Testaa julkaistu kenttä aina V-Wingissä ja tarkista erityisesti törmäykset,
vesi, palaminen, räjähdykset ja tykkien toiminta.
