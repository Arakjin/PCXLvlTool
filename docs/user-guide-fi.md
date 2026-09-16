# PCX Level Tool – käyttöohje

PCX Level Tool on pikselintarkka kenttäeditori. Ensimmäinen tuettu peli on
V-Wing. Ohjelma avaa pelin `.LEV`-kenttiä ja tallentaa monitasoisen työn
muokattavana `.pxlp`-projektina.

> Säilytä alkuperäisestä `.LEV`-tiedostosta varmuuskopio. Testaa julkaistu
> kenttä pelissä ennen sen jakamista muille.

## Uuden työn aloittaminen

- **File > New level** (`Ctrl+N`) luo tyhjän kentän ja oletuspaletin.
- **File > Open** (`Ctrl+O`) avaa `.LEV`-kentän tai `.pxlp`-projektin.
- Yläpalkin **Level name** määrittää pelissä näkyvän kentän nimen. Nimen
  enimmäispituus on 20 tulostettavaa ASCII-merkkiä. Ääkkösiä ei hyväksytä ja
  nimi muutetaan tallennettaessa isoiksi kirjaimiksi.

`.LEV` ja `.pxlp` palvelevat eri tarkoituksia:

- **Save project** (`Ctrl+S`) tallentaa työversion `.pxlp`-muodossa. Tasot,
  tasojen järjestys, läpinäkyvyys ja muut editoritiedot säilyvät.
- **Publish LEV** kirjoittaa pelissä käytettävän `.LEV`-tiedoston. Näkyvät
  tasot yhdistetään yhdeksi kuvaksi, joten jatka muokkausta `.pxlp`-projektista.

## Piirtäminen ja värit

Palettipaneelissa vasen napsautus valitsee ensisijaisen materiaalin ja oikea
napsautus toissijaisen materiaalin. Piirtoalueella vasen ja oikea hiiren painike
käyttävät vastaavia materiaaleja. Muotojen ääriviiva käyttää ensisijaista ja
täyttö toissijaista materiaalia.

Paletin yläpuolisella valikolla voi rajata näkyviin esimerkiksi veden,
normaalin maaston tai rikkoutumattoman maaston indeksit. Varatut indeksit on
jätetty valintojen ulkopuolelle. **Edit selected color** muuttaa yksittäisen
indeksin RGB-väriä. Paletin voi tallentaa ja avata erillisenä JASC-PAL-
tiedostona.

Työkalut:

- **Pencil** piirtää pikselintarkasti. Kärki voi olla neliö tai ympyrä.
- **Eraser** pyyhkii ylemmällä tasolla läpinäkyväksi. Background-tasolla se
  kirjoittaa indeksin 0.
- **Spray** lisää hajanaisia pikseleitä valitulla säteellä.
- **Line** piirtää suoran viivan. `Shift` rajoittaa suunnan vaaka-, pysty- tai
  45 asteen linjaan.
- **Bezier curve** tehdään kolmella vedolla: ensin lähtöviiva ja sitten kaksi
  taivutusta. `Shift` rajoittaa vain lähtöviivan suunnan.
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

Osoittimen koordinaatit ja sen alla oleva tiedostoindeksi näkyvät alapalkissa.

## Kentän vieminen V-Wingiin

1. Tallenna ensin muokattava `.pxlp`-projekti.
2. Valitse **File > Publish LEV** ja anna julkaistavalle kentälle `.LEV`-nimi.
3. Ota alkuperäisestä pelikentästä varmuuskopio.
4. Kopioi julkaistu tiedosto V-Wingin kenttähakemistoon pelin käyttämällä
   tiedostonimellä.
5. Avaa kenttä pelissä ja tarkista materiaalit, törmäykset sekä erikoisosat.

Ohjelma ei asenna tai siirrä tiedostoja V-Wingin hakemistoon automaattisesti.
