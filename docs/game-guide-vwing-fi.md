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
**Burnable**, **Underwater**, **Indestructible** ja **Turrets** sen sijaan,
että valitsisit materiaalin pelkän RGB-värin perusteella. Varatut indeksit on
poistettu valittavista vaihtoehdoista.

Tärkeimpiä materiaaliryhmiä ovat:

- 0: musta tausta
- 16–19: vesi ja veden virtaussuunnat
- 20–30: läpilennettävä alue
- 57–149: tavallinen maasto
- 151–174: pois palava maasto
- 176–199: tuhkaksi palava maasto
- 201–219: vedenalaiset materiaalit ja vedeksi tuhoutuva maasto
- 221–243 ja 248–255: rikkoutumaton maasto
- 244–247: tykkien osat

Palettipaneeli näyttää valitun materiaalin tiedostoindeksin ja kuvauksen.
Vasen hiiren painike käyttää ensisijaista ja oikea toissijaista materiaalia.
Kumi palauttaa alimman Background-tason indeksiksi 0.

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

## Editorin helpotukset

- Varattuja indeksejä ei voi valita numero- tai palettivalikosta.
- Materiaaliryhmät vähentävät indeksinumeroiden muistamista.
- `Shift` tekee viivoista vaaka-, pysty- tai 45 asteen suuntaisia sekä
  suorakulmioista neliöitä ja ellipseistä ympyröitä.
- Muodon ääriviiva käyttää ensisijaista ja täyttö toissijaista materiaalia.
- Tasot voi pitää erillään muokkauksen aikana; vain näkyvät tasot yhdistetään
  LEV-julkaisussa.

Testaa julkaistu kenttä aina V-Wingissä ja tarkista erityisesti törmäykset,
vesi, palaminen, räjähdykset ja tykkien toiminta.

