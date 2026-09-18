# PCX Level Tool – kehittäjän muistilista

Tämä tiedosto kuvaa projektin nykyiset rajat, tärkeimmät tekniset ratkaisut ja
julkaistun 0.2.5-version pohjan jatkokehitykselle. Yksityiskohtaiset käyttäjä-
ja formaattiohjeet pidetään `README.md`- ja `docs/`-tiedostoissa.

## Projektin tavoite

PCX Level Tool on C++17- ja Qt 6 -pohjainen pikselintarkka editori vanhojen
indeksoituja PCX-kuvia käyttäville peleille. Ensimmäiset peliprofiilit ovat
V-Wing, Wings ja AUTS. Rakenteen pitää sallia uusien samankaltaisten pelien
lisääminen ilman editorin yhteisten piirto-, taso- ja projektitoimintojen
kopioimista.

Tuetut alustat ovat Windows x86_64 ja Linux x86_64. Julkaisut tuotetaan
Windows ZIP- ja Linux AppImage -paketteina.

## Nykyinen arkkitehtuuri

- `Level` on muuttuvankokoinen 8-bittinen indeksoitu kuva, jossa on 256 värin
  paletti ja enintään viisi tasoa.
- `.pxlp` on kaikkien pelien yhteinen, versioitu ja pelitunnisteella varustettu
  muokattava projektimuoto. Pelin käyttämä `.LEV` on erillinen julkaisuformaatti.
- `GameProfile` kuvaa pelin tunnisteen, kokorajat ja valinnaiset ominaisuudet.
  Uusi peli lisätään ensisijaisesti profiilina ja omana formaattikirjoittajana.
- Formaatti- ja projektikoodi kuuluu Qt-käyttöliittymästä riippumattomaan
  `vwing_level_format`-kirjastoon.
- Käyttöliittymä saa kysyä vain valitun peliprofiilin tukemia asetuksia.
- Paletti-indeksit ovat pelillistä dataa. Niitä ei saa muuttaa RGB-väreiksi tai
  numeroida käyttöliittymässä eri tavalla kuin tiedostossa.
- Pelille julkaistaessa vain näkyvät tasot tasoitetaan yhdeksi indeksoiduksi
  kuvaksi. Muokattavat tasot säilyvät `.pxlp`-projektissa.
- Pelikohtaiset LEV-vientihakemistot ovat käyttäjän yleisiä `QSettings`-
  asetuksia eivätkä kuulu `.pxlp`-projektiin.
- Uusien `.pxlp`-projektien oletushakemisto on Documents-kansion alla
  `PCX Level Tool/Projects`, ellei käyttäjä muuta sitä yleisissä asetuksissa.
- Yleinen käyttöohje, erilliset pelikohtaiset ohjeet ja lisenssitiedot ovat
  ohjelmaan upotettuja resursseja, jotta Help-valikko toimii itsenäisesti
  kaikissa julkaisupaketeissa.

## Peliprofiilit

### V-Wing

- Kiinteä kenttäkoko 640 x 800.
- Klassisen converter 1.91 -yhteensopivan `.LEV`-muodon luku ja kirjoitus.
- Pelissä näkyvä nimi tallennetaan LEV-tiedostoon, enintään 20 tulostettavaa
  ASCII-merkkiä ja tallennettaessa isot kirjaimet.
- Materiaaliryhmät ja varatut paletti-indeksit perustuvat `CONVERT.TXT`:hen.
- Paletti-indeksejä 46, 48, 51, 52, 56 ja 244–247 käytetään myös pelin
  spritejen tai efektien väreihin. Tykkiväreillä piirtäminen ei sijoita tykkejä.

### Wings

- Kenttäkoko 157 x 90 ... 1000 x 1000, oletuksena 400 x 400.
- Oma Wings `.LEV` -kirjoittaja vastaa MAKELEV-formaatin rakennetta.
- Valinnaisen parallax-taustan koko on `width / 2 + 78` x
  `height / 2 + 45`, käyttäen kokonaislukujakoa.
- Kenttä ja parallax-tausta ovat käyttöliittymässä erilliset välilehdet, mutta
  saman projektin dokumentteja ja käyttävät samaa palettia.
- Wings-asetuksiin kuuluvat tähdet, sade, lumi, pommitus, siviilit ja
  aseistettujen siviilien todennäköisyys.
- Kiinteiden ja varattujen Wings-indeksien RGB-värejä ei saa tarjota
  muokattaviksi.
- Indeksit 0–47 käyttävät kiinteitä RGB-arvoja, mutta dokumentoidut 0, 16 ja
  32–47 ovat piirrettäviä. Indeksien 48–255 värejä voi muuttaa lukuun ottamatta
  varattua aluetta 57–63.
- Wingsin paikallinen referenssiaineisto on `Wings/`-hakemistossa. Hakemisto on
  aina Gitin ja julkaisupakettien ulkopuolella eikä sen tiedostoja muokata.

### AUTS

- Kiinteä kenttäkoko 320 x 400 ja kiinteä 256 värin AUTS-paletti.
- Kaikilla 256 indeksillä voi piirtää, mutta niiden RGB-arvoja ei voi muuttaa.
- Dokumentoidut erikoisindeksit ovat avaruus 0, rikkoutumaton 7, vesi 39 ja
  telakointilevy 92–95.
- Uudessa ja julkaistussa kentässä on alkuperäisen BMP2LEV-converterin tavoin
  kahden pikselin rikkoutumaton reunus indeksillä 7.
- AUTS `.LEV` voidaan lukea ja kirjoittaa suoraan. Muoto on RLE-pakattu eikä
  sisällä palettia tai erillistä kentän nimeä; nimi toimii DOS-tiedostonimenä.
- Pelikohtainen BMP-tuonti hyväksyy vain 320 x 400, 8-bittisen pakkaamattoman
  indeksoidun kuvan. Indeksit säilytetään ja kiinteä AUTS-paletti otetaan
  käyttöön.
- Paikallinen `AUTSCONV/`-aineisto on read-only-referenssiä ja aina Gitin sekä
  julkaisupakettien ulkopuolella.

## Uuden pelin lisäämisen reitti

1. Lisää vakaa `GameId`, profiili ja ominaisuusliput tiedostoihin
   `src/game_profile.h` ja `src/game_profile.cpp`.
2. Lisää pelin tarkka oletuspaletti ja paletti-indeksien säännöt erillään muiden
   pelien säännöistä.
3. Toteuta pelikohtainen reader/writer Qt-riippumattomana. Älä lisää formaatin
   haaroja yleisiin piirto- tai tasoluokkiin.
4. Laajenna `.pxlp`-asetusten tageja taaksepäin luettavasti. Tuntemattomat pelit
   ja virheelliset asetukset hylätään selkeällä virheellä.
5. Näytä uuden pelin asetukset uuden kentän dialogissa vain ominaisuuslippujen
   perusteella.
6. Lisää formaatti-, projekti-, paletti- ja profiilitestit sekä dokumentoi
   alkuperäislähteistä vahvistetut rajoitukset.

Arvauksia ei kirjoiteta tiedostoformaatin säännöiksi. Havainnot erotellaan
vahvistettuihin ja vielä epävarmoihin, ja alkuperäisiä tiedostoja käsitellään
vain read-only-referenssinä.

## Versio 0.2.0

0.2.0 on 16.9.2026 julkaistu ensimmäinen monen pelin versio. Sen pääsisältö on:

- yhteinen `.pxlp`-projektimuoto V-Wingille, Wingsille ja myöhemmille peleille
- peliprofiileihin perustuva laajennettava backend
- uuden kentän pelivalinta sekä pelikohtaiset koko- ja ominaisuusasetukset
- Wingsin muuttuvankokoiset kentät ja tarkka oletuspaletti
- Wingsin peliasetukset, parallax-taustan oma välilehti ja oikea kokolaskenta
- Wings-yhteensopiva `.LEV`-julkaisu ilman alkuperäisen MAKELEV-ohjelman ajoa
- pelin aina käyttäjältä kysyvä indeksoidun kuvan tuonti: PCX kaikille
  tuetuille peleille sekä BMP AUTSille
- AUTS-kenttien luonti ja `.LEV`-julkaisu ilman alkuperäisen BMP2LEV-ohjelman
  ajoa
- V-Wingin nykyisten muokkaus- ja julkaisutoimintojen säilyminen

## Kehityssäännöt

- Muokkaa vain `LEVTOOLS`-repositorion tiedostoja. Ylemmän `VWing`-hakemiston,
  `Wings/`-hakemiston ja `AUTSCONV/`-hakemiston aineisto on
  read-only-referenssiä.
- Pidä riippuvuudet vähäisinä: Qt 6 Widgets/SVG ja C++-standardikirjasto.
- Säilytä Windows- ja Linux-yhteensopivuus; älä kovakoodaa paikallisia polkuja.
- Yksi käyttäjän piirtoele on yksi undo-operaatio. Rasteroidun esikatselun ja
  lopullisen piirron pitää käyttää samaa geometriaa.
- Älä muuta alkuperäisiä `CONV.EXE`, `CONVERT.TXT` tai `FILE_ID.DIZ` -tiedostoja.
  Niiden tarkistussummat ja erilliset jakeluehdot tarkistetaan CI:ssä.
- Lisää regressiotesti jokaiselle korjatulle formaatti- tai kaatumisvirheelle.
- Aja ennen committia:

  ```sh
  cmake --build build --parallel
  ctest --test-dir build --output-on-failure
  ```

## Myöhempi jatkokehitys

- Lisää seuraavat indeksoituja kuvia käyttävät pelit yksi profiili ja formaatti
  kerrallaan.
- Lisää pelikohtainen validointi yhteiseen raportointirajapintaan.
- Lisää LEV-tuonti Wingsille vasta, kun reader voidaan toteuttaa ja testata
  deterministisesti oikealla aineistolla.
- Arvioi englanninkieliset käyttöohjeet ja käyttöliittymän lokalisointi erillisinä
  julkaisutavoitteina.

## Piirtotyökalujen suunnitteluideat

Seuraavat ovat jatkokehitysideoita, eivät vielä toteutettavia ominaisuuksia.
Niiden käyttöliittymä ja tiedonkäsittely pitää suunnitella ennen koodaamista.

### Terrain Brush

- Täytetty vapaamuotoinen sivellin suurten maastoalueiden nopeaan piirtämiseen.
- Kirjoittaa aina tarkkoja paletti-indeksejä ilman pehmennystä, läpinäkyvyyttä
  tai automaattista värien sekoittamista.
- Suunniteltavia asetuksia ovat siveltimen koko, kärjen muoto sekä tasainen tai
  hallitusti rosoinen reuna.
- Esikatselun ja lopullisen rasteroinnin pitää vastata toisiaan, ja yhden vedon
  pitää muodostaa yksi undo-operaatio.

### Magic Wand

- Valitsee napsautetun yhtenäisen alueen saman paletti-indeksin perusteella.
- Mahdollinen lisätila valitsee kaikki saman indeksin kohdat aktiivisesta
  dokumentista tai rajatulta valinta-alueelta.
- Ei väritoleranssia: samannäköiset RGB-värit voivat olla eri materiaaleja.
- Toimii nykyisten valintojen yhdistämissääntöjen kanssa: tavallinen valinta,
  `Shift` lisää, `Ctrl` vähentää ja `Shift+Ctrl` leikkaa.
- Ennen toteutusta päätetään, tutkitaanko aktiivista tasoa vai näkyvien tasojen
  yhdistettyä kuvaa. Valinnan muokkaamisen pitää silti kohdistua aktiiviseen
  tasoon yksiselitteisesti.

### Valinnan peilaus ja kierto

- Peilaus vaaka- ja pystysuunnassa sekä kierrot 90, 180 ja 270 astetta.
- Säilyttää paletti-indeksit ja läpinäkyvän maskin täsmälleen; ei interpolointia
  tai reunan pehmennystä.
- Käyttää nykyistä kelluvaa valintaa, jotta tulosta voi siirtää ennen
  hyväksymistä.
- Ennen toteutusta määritellään ei-neliömäisen valinnan ankkurointi sekä
  kentän ulkopuolelle kiertyvien pikselien käsittely.

### Materiaaliryhmään rajattu liukuväri

Liukuväri vaatii vielä lisäsuunnittelua ennen toteutusta.
Tavallinen indeksivälin läpikäynti ei kelpaa, koska peräkkäiset indeksit voivat
vaihtua esimerkiksi vedestä maastoksi tai kulkea varattujen indeksien kautta.

Liukuväri toteutetaan omana **Gradient Fill** -työkaluna tavallisen
**Flood Fill** -työkalun vieressä, ei siveltimenä tai Flood Fillin piilotettuna
tilana. Työkalut jakavat saman yhtenäisen alueen tunnistuksen, mutta niiden
hiiritoiminta ja asetukset pysyvät erillisinä. Yksi täyttö on yksi
undo-operaatio.

#### Kohdealue ja suuntaveto

- Hiiren painallus toimii täytön siemenpisteenä. Täytettävä kohde on siitä
  löytyvä yhtenäinen saman materiaalin alue kuten tavallisessa Flood Fillissä.
- Aktiivinen valinta rajaa kohdealuetta. Vain yhtenäisen alueen ja aktiivisen
  valinnan leikkaukseen saa kirjoittaa.
- Painalluksesta vapautukseen vedetty viiva määrittää vain liukuvärin suunnan,
  pituuden tai säteen. Veto ei muodosta täytettävää suorakulmiota eikä rajaa
  täyttöä vetojäljen ympärille.
- Vedon aikana näytetään esikatselu koko kohdealueella, jotta käyttäjä näkee
  ennen hyväksymistä sekä täytettävän alueen että indeksien lopullisen
  sijoittumisen. Varsinaisia pikseleitä muutetaan vasta vedon hyväksymisen
  yhteydessä.
- Lineaarisessa tilassa vedon alku vastaa liu'un alkua ja loppu liu'un loppua.
  Säteittäisessä tilassa alku on keskipiste ja vedon pituus määrää säteen.
- Ilman toistoa ennen alkua ja lopun jälkeen käytetään lähintä päätyindeksiä.
  Toistotilassa valittu indeksisarja jatkuu asetetun jakson mukaisesti koko
  kohdealueelle.

#### Asetusten avaaminen

- Liukuvärityökalun painikkeen oikea napsautus avaa liukuväriasetukset.
- Kun liukuvärityökalu on aktiivinen, normaalin palettipaneelin alle ilmestyy
  erillinen **Gradient settings** -painike samaan ikkunaan.
- Asetuspainiketta ei näytetä muiden työkalujen aikana.

#### Materiaalin ja indeksien valinta

- Asetusikkunassa on normaalin palettipaneelin kaltainen palettinäkymä ja
  pelikohtaisten materiaaliryhmien pudotusvalikko.
- Liukuvärissä saa valita vain pelillisesti yhtenäisen materiaaliryhmän, kuten
  **Fly through** tai **Normal terrain**. Laajat sekaryhmät, kuten
  **All usable**, eivät kelpaa liukuväriryhmäksi.
- Käyttäjä valitsee alku- ja loppuindeksin valitusta ryhmästä. Molempien sekä
  kaikkien käytettävien väli-indeksien pitää kuulua samaan materiaalityyppiin.
- Varattuja indeksejä tai toisen toiminnon materiaaleja ei saa tulla sarjaan,
  vaikka niiden numerot sijaitsisivat alku- ja loppuindeksin välissä.
- Asetusikkuna näyttää alku- ja loppuvärin sekä kaikki mukaan tulevat
  väli-indeksit numeroineen ja väreineen. Näin käyttäjä näkee etukäteen myös
  monivärisen liu'un, esimerkiksi vihreästä keltaisen kautta punaiseen.
- Gradient Fill sijoittaa edelleen vain olemassa olevia paletti-indeksejä.
  Asetusikkunaan lisätään erillinen palettirampin laskin, jolla näiden indeksien
  RGB-värit voidaan valmistella ennen täyttämistä.

#### Palettirampin laskin

- Käyttäjä valitsee alkuindeksin ja loppuindeksin värit. Laskin muodostaa
  ehdotetut RGB-värit niiden välissä oleville sallituille indekseille.
- Käyttäjä voi lisätä väli-indeksin väripisteeksi. Tällöin laskenta tehdään
  osissa, esimerkiksi vihreästä keltaiseen ja keltaisesta punaiseen.
- Kaikki indeksit ja lasketut värit näytetään yhtenä esikatseltavana rampina
  ennen paletin muuttamista.
- **Calculate** päivittää vain esikatselun. Erillinen hyväksyntä kirjoittaa
  lasketut värit projektin palettiin yhtenä undo-operaationa; peruutus ei muuta
  palettia.
- Laskin saa muuttaa vain valitun yhtenäisen materiaalityypin muokattavia
  indeksejä. Varatut ja pelin kiinteät värit ovat aina poissa laskennasta.
- AUTSin paletti on kokonaan kiinteä, joten laskinta ei tarjota AUTSille.
  Gradient Fill voi silti käyttää AUTSin olemassa olevia saman
  materiaalityypin indeksejä muuttamatta niiden värejä.
- V-Wingissä laskimen pitää näyttää varoitus, jos valittu indeksi vaikuttaa
  myös pelihahmojen tai efektien väriin.
- Ennen toteutusta päätetään värien laskentatapa, esimerkiksi suora RGB-liuku
  tai sävyn kautta kulkeva laskenta. Valittu tapa pitää näyttää esikatselussa,
  eikä laskin saa päätellä materiaalityyppiä pelkän näkyvän värin perusteella.

#### Täyttötavat

- **Linear** muodostaa suoran liu'un valittuun suuntaan.
- **Radial** muodostaa liu'un keskipisteestä ulospäin.
- **Repeat** toistaa valitun indeksisarjan täytettävällä alueella.
- Ennen toteutusta määritellään, onko toisto tavallinen, peilattu vai molemmat
  valittavina, sekä kuinka suunta, keskipiste ja jakson pituus asetetaan
  hiirellä.
- Myös yhden napsautuksen oletustoiminta ja vedolla tehtävä esikatselu pitää
  ratkaista ennen koodaamista.

Vältä tarpeettomia service/controller/plugin-kerroksia. Laajennettavuus tässä
projektissa tarkoittaa selkeitä peliprofiileja ja formaattirajoja, ei dynaamista
plugin-järjestelmää.
