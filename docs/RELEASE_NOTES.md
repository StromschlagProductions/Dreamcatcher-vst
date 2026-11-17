# Dreamcatcher 2.2 – Echtzeitsicherheit und abgesicherter Traum-Modus

Der in Version 2.1 überarbeitete Traum-Modus bleibt erhalten: weiche Zwei-Kopf-Verstimmung statt der früheren harten PitchNasty-Verarbeitung. Markus' Verdacht auf Ducking war damit nicht die Ursache der damals beschriebenen Artefakte.

Zusätzliche technische Verbesserungen:

- Reset verwendet die vorhandenen GlitchShifter-Puffer weiter, statt zwei neue Speicherreservierungen auszuführen.
- Die Meldung geänderter Geist-Latenz erfolgt über einen Message-Thread-Timer; der Audio-Callback stellt keine Betriebssystem-Nachricht mehr zu.
- Ringpufferpositionen bleiben begrenzt und können bei langen Sitzungen nicht mehr als vorzeichenbehaftete Zähler überlaufen.
- Ungültige Werte werden am Eingang und an den Steuerparametern abgefangen. Eingangspegel jenseits ±4 werden zum Schutz des ganzzahligen Glitch-Bausteins begrenzt; normale Sprachpegel bleiben unverändert.
- Neue Tests für alle Modi und 22.050–192.000 Hz, Blockaufteilung, Reset-/Audio-Allokationen und den Traum-Ausklang mit Ducking aus/an. Im synthetischen Test klingt der Nachhall ab; das ist kein Ersatz für einen Hörtest mit eigenen Sprachaufnahmen.

Der Veröffentlichungs-Lauf prüft Regressionstests und pluginval Level 8. Geist meldet weiterhin etwa 0,6 s Latenz. Die Latenzänderung wird außerhalb des Audio-Threads weitergegeben; den Modus vor der Wiedergabe einstellen. Alte Versionen bleiben verfügbar.

Installation: DAW schließen, ZIP entpacken und den ganzen Ordner `Dreamcatcher.vst3` nach `C:\Program Files\Common Files\VST3` kopieren.
