# DREAMCATCHER – Audio Drama Inner-Voice-Simulator

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/logo-dunkel.png">
  <img src="docs/logo-hell.png" alt="Stromschlag Audio" height="64">
</picture>

VST3-Insert-Effekt für Hörspiel-Produktion. Er lässt eine Stimme klingen, als käme sie nicht aus dem Raum der Szene,
sondern von innen: als **innere Stimme**, als **Erinnerung / Rückblende** oder im **Traum**. Ein Modus-Schalter, ein
Fader, fertig.

**⬇ Download (Windows · VST3 · Stereo):** https://github.com/StromschlagProductions/Dreamcatcher-vst/releases/latest

## Warum es dieses Plugin gibt

Gedanken, Rückblenden und Träume brauchen in fast jedem Hörspiel eine eigene Stimmbehandlung, damit man sofort
merkt: Das ist jetzt nicht die Szene, das ist im Kopf. Bisher habe ich das jedes Mal aus mehreren Effekten
zusammengebaut. DREAMCATCHER legt diese Behandlung mit einem Klick auf die Spur und lässt sich über die Zeit
ein- und ausblenden, zum Beispiel wenn eine alte Stimme aus einer Überblendung in der Rückblende auftaucht.

DREAMCATCHER soll niemandem die Erfahrung abnehmen, solche Szenen selbst zu gestalten. Gerade am Anfang kann es viel
Spaß machen und lehrreich sein, eine Traum- oder Erinnerungsstimme selbst aufzubauen und zu hören, wie Verstimmung,
Echo, Filter und Automation ineinandergreifen. Ich bin dankbar, dass ich diesen Weg selbst gegangen bin, auch wenn er
manchmal umständlich und mühselig war. Gerade dadurch weiß ich, was ich mir von einem Werkzeug wünsche, das all das in
einem vereint. DREAMCATCHER soll das Gestalten nicht ersetzen, sondern die technische Umsetzung im Alltag einfacher machen.

Der Klang jedes Modus ist fest abgestimmt. Es gibt keinen Ausgangsregler, und es werden keine eigenen Geräusche
dazugemischt: Das Plugin verändert nur den Klang der Stimme.

– Lars Eickstädt, Stromschlag Audio

## Oberfläche

| Erinnerung (dunkel) | Traum, Effekt 60 % (hell) |
|---|---|
| ![Erinnerung](docs/oberflaeche-dunkel.png) | ![Traum](docs/oberflaeche-hell.png) |

Oben wählt man den Modus, darunter steht der **Effekt-Fader**. Der **?**-Knopf öffnet die Hilfe, Look Dunkel/Hell und
die Fenstergröße (50–300 %) werden mit dem Projekt gespeichert. Zu jedem Regler gibt es einen Tooltip.

## Modi und Regler (beide automatisierbar)

| Regler | Wirkung |
|---|---|
| **Modus** | **Innere Stimme:** Die Stimme rückt ins Innere des Kopfes: leicht verbreitert, ein Hauch Echo, Höhen weicher, Tiefen weg. **Erinnerung:** Die Stimme stottert an einzelnen Stellen, sinkt minimal ab, klingt dünn und dumpf mit Echo wie aus der Ferne der Zeit (für Rückblenden). **Traum:** körnig verstimmt, in einer weiten Wolke aus Echos, dunkel und schwebend. Beim Wechsel blendet das Plugin in etwa einer zehntel Sekunde über. |
| **Effekt** | 0 % = die Stimme unbearbeitet, 100 % = der volle Klang des Modus, dazwischen fließend (Standard: 100 %). Doppelklick setzt auf 100 % zurück. |

Das ist wie bei LOCATIONS: Ein Fader bewegt die Stimme zwischen zwei Zuständen. Hier sind es die normal klingende
Stimme und der Traum. Einen extra Ein-/Ausschalter gibt es nicht, 0 % ist aus.

## Anwendung

- **Rückblende (Beispiel KADAITCHA):** Spur der alten Stimme auf Erinnerung, Effekt in der Überblendung per Automation von
  0 auf 100 % fahren. Die Stimme wächst aus dem Hier und Jetzt in die Erinnerung hinein.
- **Aufwachen:** Spur auf Traum, Effekt vom Ende der Traumszene an auf 0 % zurückfahren: Die Stimme wird direkter und klarer.
- **Innere Stimme:** Spur auf Innere Stimme, Effekt auf 100 %. Zum Zurückholen ins Gespräch auf 0 % fahren.
- **Auf einem Bus:** DREAMCATCHER lässt sich auch auf einen Gruppenkanal legen. So bekommen die Stimme und die Geräusche
  einer Traum- oder Erinnerungsszene (Atmo, Schritte, Musik) gemeinsam dieselbe Behandlung und wirken wie aus einer
  Welt. Der Effekt arbeitet dann auf der Summe, jede Quelle behält ihren Platz im Stereobild. Ein Fader auf dem Bus
  blendet die ganze Szene zusammen in den Effekt hinein oder zurück ins Hier und Jetzt.
- **Ein Fehler beim Abmischen:** Effekt kurz auf 0 % ziehen, die Stimme ist sofort wieder unbearbeitet.

## Hinweise

- **Latenz:** keine, das Plugin meldet dem Host nichts. Der Effekt setzt mit dem Audio ein. Die bearbeitete Stimme liegt
  durch die Echo- und Stotter-Bausteine einige Millisekunden hinter der unbearbeiteten, das ist Teil des Klangs.
  Zwischen 0 und 100 % mischen sich beide, dabei können leichte Kammfilter-Färbungen entstehen.
- Der Eingang wird als Stereo verarbeitet; eine Mono-Spur bekommt durch die Echo-Bausteine Stereobreite.
- Der Pegel ist fest ausgeglichen: bei 100 % ist die Stimme etwa so laut wie bei 0 % (gemessen mit einem
  Testsignal, bei echten Stimmen kann es um ein paar dB abweichen).
- Nach dem Ende der Stimme klingt die Echo-Wolke noch kurz nach.

## Installation (Windows)

1. Über den Download-Link oben `Dreamcatcher-VST3-Windows.zip` laden.
2. ZIP entpacken und den **ganzen Ordner** `Dreamcatcher.vst3` nach `C:\Program Files\Common Files\VST3` kopieren.
3. DAW neu starten bzw. Plugins neu scannen.

## Versionen

Die Versionsnummer steht in der Datei `VERSION`, im Plugin-Kopf und im Namen der Release. Sie wird bei jeder
Änderung hochgezählt.

- **1.2** – Hinweis zur Nutzung auf einem Bus in README und Plugin-Hilfe ergänzt.
- **1.1** – README um den Absatz „Gestalten nicht ersetzen“ ergänzt.
- **1.0** – Erste Version: Modi Innere Stimme, Erinnerung und Traum, Effekt-Fader 0–100 %, Oberfläche im Stil von
  LOCATIONS und RADIOPHONE mit Hilfe, Hell/Dunkel und skalierbarem Fenster.

## Aufbau

- `Source/DreamcatcherDSP.h`: Klang-Engine (reines C++). Die Klangwerte der drei Modi stehen oben in `kVoicing`.
- `Source/PluginProcessor.*`, `Source/PluginEditor.*`: JUCE-Plugin und Oberfläche
- `LICENSES/`: Lizenztext der übernommenen Airwindows-Bausteine
- `.github/workflows/build.yml`: automatischer Windows-Bau mit Stabilitätstest (pluginval) und Release

## Danke

Gebaut mit [JUCE](https://juce.com) 8. Die Klangbausteine **SweetWide**, **StereoEnsemble**, **GlitchShifter** und
**PitchNasty** sind Portierungen von Airwindows-Code von Chris Johnson
([github.com/airwindows](https://github.com/airwindows/airwindows), MIT-Lizenz). Angepasst wurden nur die Struktur
(freie Abtastrate) und die Dither-Stufe, die weggelassen ist; die Algorithmen selbst sind übernommen.
