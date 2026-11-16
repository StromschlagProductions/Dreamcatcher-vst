# CI-Nutzung (GitHub Actions)

Das GitHub-Konto hat nur ein begrenztes Monatskontingent an Actions-Minuten. Windows-Läufe zählen bei privaten Repos doppelt.
Deshalb läuft der teure Windows-Bau nicht mehr bei jedem Commit.

## Stand seit der Umstellung

| Vorher | Jetzt |
|---|---|
| Vollständiger Windows-Bau + pluginval bei **jedem Push** auf `main` (außer reine Doku-Änderungen) | Nur auf Anforderung (`workflow_dispatch`) |
| Release **automatisch** nach jedem erfolgreichen Lauf | Release nur, wenn beim Start **„release“ angehakt** wird (Standard: aus) |
| Timeout 45 Minuten | Timeout 20 Minuten (ein Lauf dauert etwa 4 bis 5 Minuten) |
| Überholte Läufe liefen zu Ende | `concurrency` bricht überholte Läufe auf demselben Zweig ab |
| Artefakt ohne Aufbewahrungsfrist-Angabe | Artefakt 14 Tage |

Nicht verändert: Der Lauf baut weiterhin die Windows-VST3 und prüft sie mit pluginval bei Strictness 8. An der Qualität der
abschließenden Prüfung ändert sich nichts. Es gibt weder automatische Releases noch automatische Merges.

## Arbeitsweise

1. **Entwickeln lokal:** Bau mit CMake/JUCE, eigene DSP-Tests (Stabilität, Latenz, Pegel, Knackser) und pluginval (Linux, Strictness 8).
2. **Pushen** darf man jederzeit, das startet keinen Lauf mehr.
3. **Abnahme:** Auf GitHub unter *Actions → Windows-VST3 bauen → Run workflow* starten.
   - „release“ **nicht** angehakt: Bau und pluginval laufen, die VST3 liegt 14 Tage als Artefakt des Laufs bereit.
   - „release“ angehakt: zusätzlich entsteht ein Release mit `Dreamcatcher-VST3-Windows.zip`, auf das der Download-Link in der README zeigt.
4. Pro Version möglichst **einen** Lauf, erst wenn die Version fertig ist.

## Mögliche weitere Einsparung (nicht eingebaut)

Der JUCE-Quelltext wird bei jedem Lauf neu geladen. Ein Cache dafür würde etwa eine halbe Minute sparen; der Hauptteil ist das Kompilieren.
Das ist bewusst nicht eingebaut, weil sich der Cache nur in einem echten Lauf prüfen lässt.
