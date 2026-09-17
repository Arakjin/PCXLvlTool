# PCX Level Tool – käyttöohje

PCX Level Tool on pikselintarkka kenttäeditori V-Wingille, Wingsille ja
AUTSille. Ohjelma avaa tuettuja `.LEV`-kenttiä ja tallentaa monitasoisen työn
muokattavana `.pxlp`-projektina.

> Säilytä alkuperäisestä `.LEV`-tiedostosta varmuuskopio. Testaa julkaistu
> kenttä pelissä ennen sen jakamista muille.

Vaikka kaikki kolme peliä käyttävät kentilleen `.LEV`-tiedostopäätettä, niiden
tiedostomuodot eivät ole keskenään yhteensopivia. V-Wingille julkaistua kenttää
ei voi käyttää Wingsissä tai AUTSissa eikä päinvastoin. Valitse uutta kenttää
luodessa ja kuvaa tuodessa aina oikea kohdepeli.

## Uuden työn aloittaminen

- **File > New level** (`Ctrl+N`) kysyy pelin ja luo sille oikeankokoisen tyhjän
  kentän sekä oletuspaletin.
- **File > Open** (`Ctrl+O`) avaa `.LEV`-kentän tai `.pxlp`-projektin.
- V-Wingissä yläpalkin **Level name** määrittää pelissä näkyvän kentän nimen.
  Sen enimmäispituus on 20 perusmerkkiä. Ääkkösiä ei hyväksytä ja nimi muutetaan
  tallennettaessa isoiksi kirjaimiksi. Wings ja AUTS eivät tallenna erillistä
  sisäistä kenttänimeä, joten niissä nimeksi tulee julkaistun tiedoston nimi
  eikä **Level name** -kenttää näytetä.

`.LEV` ja `.pxlp` palvelevat eri tarkoituksia:

- **Save project** (`Ctrl+S`) tallentaa työversion `.pxlp`-muodossa. Tasot,
  tasojen järjestys, läpinäkyvyys ja muut editoritiedot säilyvät.
- **Publish LEV** kirjoittaa pelissä käytettävän `.LEV`-tiedoston. Näkyvät
  tasot yhdistetään yhdeksi kuvaksi ja tiedosto tehdään valitun pelin omassa
  muodossa. Jatka myöhempää muokkausta `.pxlp`-projektista.

## Projektien ja LEV-tiedostojen oletushakemistot

Valinnassa **Settings > Preferences** voi asettaa `.pxlp`-projekteille yhteisen
oletushakemiston sekä V-Wingille, Wingsille ja AUTSille omat LEV-
vientihakemistot. Polun voi kirjoittaa käsin tai valita **Browse**-painikkeella.
**Default** palauttaa projektihakemiston oletusarvon ja **Clear** poistaa
kyseisen pelin LEV-asetuksen.

Projektien oletushakemisto on käyttöjärjestelmän Documents-kansion alla
`PCX Level Tool/Projects`. Ohjelma luo sen ensimmäisen projektitallennuksen
yhteydessä. **Save project as** ehdottaa uusille projekteille tätä hakemistoa,
mutta tiedostodialogissa voi aina valita muun sijainnin.

Kun pelille on asetettu vientihakemisto, **Publish LEV** avaa tiedostodialogin
siihen hakemistoon ja säilyttää kentän ehdotetun tiedostonimen. Käyttäjä voi
edelleen valita dialogissa muun sijainnin. Tyhjä asetus käyttää nykyisen
projektin kansiota tai viimeksi käytettyä julkaisukansiota. Asetukset ovat
käyttäjä- ja konekohtaisia, eikä niitä tallenneta `.pxlp`-projektiin.

## Kuvan tuonti

**File > Import** kysyy aina kohdepelin. V-Wing ja Wings hyväksyvät 8-bittisen
indeksoidun PCX-kuvan. AUTS hyväksyy sellaisen PCX-kuvan tai pakkaamattoman
8-bittisen indeksoidun BMP-kuvan. Ohjelma ei arvaa peliä kuvan koon perusteella.

Tuonnissa materiaalinumerot säilyvät, ja editori tarkistaa kuvan koon valitun
pelin rajoitusten mukaan. Katso paletti-, koko- ja materiaalirajoitukset
Help-valikon erillisestä pelikohtaisesta ohjeesta. Tallenna tuotu työ
`.pxlp`-projektiksi ja tee peliin menevä tiedosto valinnalla **Publish LEV**.

## Piirtäminen ja värit

Jokaisella paletin materiaalilla on oma numero eli indeksi. Peli päättelee
numeron perusteella, onko kuvassa esimerkiksi taustaa, vettä tai tuhoutuvaa
maastoa. Siksi kaksi samalta näyttävää väriä eivät välttämättä toimi pelissä
samalla tavalla. Pelikohtaiset ohjeet kertovat materiaalien oikeat numerot ja
käyttötarkoitukset.

Palettipaneeli näyttää valitun materiaalin numeron ja kuvauksen.
Palettipaneelissa vasen napsautus valitsee ensisijaisen materiaalin ja oikea
napsautus toissijaisen materiaalin. Piirtoalueella vasen ja oikea hiiren painike
käyttävät vastaavia materiaaleja. Muotojen ääriviiva käyttää ensisijaista ja
täyttö toissijaista materiaalia.

Paletin yläpuolisella valikolla voi rajata näkyviin esimerkiksi veden,
normaalin maaston tai rikkoutumattoman maaston indeksit. Varatut indeksit on
jätetty valintojen ulkopuolelle. **Edit selected color** muuttaa valitun
materiaalin näkyvää väriä. Paletin voi tallentaa ja avata erillisenä
JASC-PAL-tiedostona.

Työkalut:

- **Pencil** piirtää pikselintarkasti. Kärki voi olla neliö tai ympyrä.
- **Eraser** tekee pyyhityn kohdan ylemmillä tasoilla läpinäkyväksi, jolloin
  alemmat tasot näkyvät sen läpi. Alimmalla Background-tasolla kumi palauttaa
  kohdan pelin taustaksi.
- **Spray** lisää hajanaisia pikseleitä valitulla säteellä.
- **Line** piirtää suoran viivan. `Shift` rajoittaa suunnan vaaka-, pysty- tai
  45 asteen linjaan.
- **Bezier curve** tehdään kolmella vedolla: ensin lähtöviiva ja sitten kaksi
  taivutusta. Taivutuspisteitä voi siirtää uudelleen ennen hyväksymistä.
  `Enter` hyväksyy käyrän, `Esc` peruu sen ja `Shift` rajoittaa vain
  lähtöviivan suunnan.
- **Rectangle** ja **Ellipse** tukevat pelkkää ääriviivaa, ääriviivaa ja
  täyttöä sekä pelkkää täyttöä. `Shift` tekee neliön tai ympyrän.
- **Polygon** luodaan napsauttamalla kulmapisteet. Päätä muoto kaksoisnapsautuksella
  tai `Enter`-näppäimellä; `Esc` peruuttaa keskeneräisen muodon.
- **Flood fill** täyttää yhtenäisen saman indeksin alueen.
- **Eyedropper** poimii materiaalin kuvasta vasempaan tai oikeaan valintaan.
- **Text**: vedä tekstialue, kirjoita suoraan kuvaan ja siirrä laatikkoa
  vetämällä. `Ctrl+Enter` tai napsautus laatikon ulkopuolelle hyväksyy tekstin;
  `Esc` peruuttaa sen.

Paksuus, kynän tai kumin kärki, muodon täyttötapa, suorakulmion kulmien pyöristys
ja tekstin fontti valitaan työkalujen alla olevasta asetuspalkista.

## Editorin yleiset helpotukset

- Varattuja indeksejä ei voi valita numero- tai palettivalikosta.
- Palettiryhmät näyttävät indeksit niiden pelillisen toiminnan mukaan.
- `Shift` tekee viivoista vaaka-, pysty- tai 45 asteen suuntaisia sekä
  suorakulmioista neliöitä ja ellipseistä ympyröitä.
- Muodon ääriviiva käyttää ensisijaista ja täyttö toissijaista materiaalia.
- Tasot säilyvät projektissa erillisinä. LEV-julkaisu yhdistää vain näkyvät
  tasot.

**Edit selected color** muuttaa valitun materiaalin näkyvää väriä kaikkialla,
missä peli käyttää samaa materiaalia. Muutos voi siksi näkyä kentän lisäksi
myös pelihahmoissa tai efekteissä. Materiaalilla piirtäminen ei itsessään lisää
kenttään esinettä, ellei pelikohtaisessa ohjeessa erikseen sanota materiaalin
toimivan sijoitus- tai luontimerkkinä.

## Valinnat

Käytettävissä ovat suorakulmainen, elliptinen ja vapaamuotoinen valinta.

- tavallinen valinta korvaa aiemman valinnan
- `Shift` lisää alueen aiempaan valintaan
- `Ctrl` vähentää alueen aiemmasta valinnasta
- `Shift+Ctrl` jättää valituksi alueiden leikkauksen

Valittuun alueeseen voi piirtää ennen sen hyväksymistä. **Move selection**
siirtää valintaa, `Delete` poistaa sen, `Ctrl+C` kopioi ja `Ctrl+V` liittää.
`Ctrl+A` valitsee koko kentän. `Enter` hyväksyy kelluvan valinnan ja `Esc`
peruuttaa sen.

## Tasot

Projektissa voi olla enintään viisi tasoa. Tasoluettelon ylin taso piirretään
päällimmäiseksi. Tyhjät pikselit näyttävät alemmat tasot läpi.

- **Add** lisää uuden tason.
- **Duplicate** kopioi aktiivisen tason.
- **Up/Down** muuttaa tasojen järjestystä.
- **Rename** nimeää tason.
- **Lock** estää tason muokkaamisen.
- Näkyvyysruutu näyttää tai piilottaa tason kokonaan.

Background on aina alin ja näkyvä taso. Sitä ei voi siirtää, nimetä uudelleen
eikä poistaa.

## Näkymä ja pikanäppäimet

- `Ctrl` + hiiren rulla: zoomaus osoittimen ympärille
- hiiren keskipainikkeen veto: näkymän siirto
- `Ctrl+Z`: kumoa
- `Ctrl+Shift+Z`: tee uudelleen
- `Ctrl+N`: uusi kenttä
- `Ctrl+O`: avaa
- `Ctrl+S`: tallenna projekti
- `Ctrl+Shift+S`: tallenna projekti nimellä
- `Ctrl+A`, `Ctrl+C`, `Ctrl+V`, `Delete`: valintatoiminnot

Osoittimen sijainti ja sen alla olevan materiaalin numero näkyvät alapalkissa.

Pelikohtaiset materiaalit, kokorajoitukset ja piirto-ohjeet löytyvät
Help-valikon erillisistä V-Wing-, Wings- ja AUTS-ohjeista.

## Kentän julkaiseminen peliin

1. Jos haluat säilyttää tasot ja jatkaa kentän muokkaamista myöhemmin, tallenna
   työ ensin `.pxlp`-projektiksi. Tämä vaihe on valinnainen.
2. Valitse **File > Publish LEV** ja anna julkaistavalle kentälle `.LEV`-nimi.
3. Jos korvaat olemassa olevan kentän, ota siitä halutessasi varmuuskopio.
4. Tallenna `.LEV` suoraan pelin kenttäkansioon tai kopioi se sinne
   julkaisemisen jälkeen. V-Wing ja AUTS käyttävät pelin omaa kansiota;
   Wingsin kentät kuuluvat pelin `LEV`-alikansioon.
5. Avaa kenttä pelissä ja tarkista materiaalit, törmäykset sekä erikoisosat.
