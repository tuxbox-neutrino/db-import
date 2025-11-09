# mv2mariadb – MediathekView nach MariaDB

`mv2mariadb` ist ein Kommandozeilenwerkzeug, das die Filmliste von
[MediathekView](https://mediathekview.de) herunterlädt, nach MariaDB importiert
und damit die Datenbasis für das Neutrino-Mediathek-Plugin bereitstellt.

> Du möchtest alles per Skript einrichten? Nutze das
> [Quickstart-Skript](https://github.com/tuxbox-neutrino/mt-api-dev/blob/master/scripts/quickstart.sh)
> aus `mt-api-dev` (lokal: `services/mediathek-backend/vendor/mt-api-dev/scripts/quickstart.sh`).
> Es fragt die MariaDB-Zugangsdaten ab (oder startet eine eigene `mariadb`-Instanz),
> erzeugt die nötigen Konfigurationsdateien und startet Importer + API automatisch.

## Was erledigt das Tool?

- Lädt und pflegt automatisch die Liste der Download-Server.
- Erkennt, ob eine neue MediathekView-Filmliste vorliegt (inkl. Diff-Listen).
- Lädt, entpackt und importiert die JSON-Daten in eine MariaDB-Instanz.
- Erstellt Indexe und hält mehrere Ziel-Schemas aktuell (`mediathek_1`,
  temporäre Tabellen, Template-Datenbank).
- Kann im Cron-Modus laufen und ruft nur dann neue Daten ab, wenn es nötig ist.

## Schnellstart (Docker Compose)

Im Repository `neutrino-make` liegt unter `services/mediathek-backend` ein
fertiger Stack:

```bash
make vendor                      # mt-api-dev & db-import klonen
docker-compose up -d db          # MariaDB starten
docker-compose run --rm importer --update
docker-compose run --rm importer # kompletter Import
```

Die erzeugten Tabellen stehen danach dem API-Container oder einer lokalen
Neutrino-Installation zur Verfügung.

## Voraussetzungen (manueller Build)

- GCC oder Clang, `make`
- MariaDB Connector/C (`libmariadb-dev`)
- libcurl, liblzma, libexpat, libpthread
- rapidjson (Header-only)

Beispiel Debian/Ubuntu:

```bash
sudo apt install build-essential pkg-config git libmariadb-dev \
  libcurl4-openssl-dev liblzma-dev libexpat1-dev rapidjson-dev
```

## Kompilieren

```bash
git clone https://github.com/tuxbox-neutrino/db-import.git
cd db-import
cp doc/config.mk.sample config.mk
make -j$(nproc)
```

Das Binary liegt danach unter `build/mv2mariadb`. Optional kannst du mit
`make install DESTDIR=/opt/importer` eine Zielstruktur erzeugen.

> **Version hinterlegen**  
> Wie beim API-Projekt wird automatisch `git describe --tags` eingebettet und
> später über die Datenbank bzw. das API sichtbar. Beim Bauen aus einem Tarball
> kannst du per `IMPORTER_VERSION=0.5.0 make` eine feste Nummer setzen.

## Konfiguration

- `config/mv2mariadb.conf` – enthält Download-URLs, Ziel-Datenbank und neue
  Option `mysqlHost`, damit Container z. B. die Compose-DB unter `db` erreichen.
- `config/pw_mariadb` – `user:pass`, wird beim Start nach `bin/pw_mariadb`
  kopiert. Verwende einen Benutzer mit CREATE/ALTER-Rechten auf den relevanten
  Schemas.
- Log-Dateien sowie Zwischenablagen landen unter `bin/dl`.

## Betrieb

Typische Cron-Zeile (Import alle 2 Stunden, Ausgaben nur bei Updates):

```
*/120 * * * * /opt/importer/bin/mv2mariadb --cron-mode 120 --cron-mode-echo >>/var/log/mv2mariadb.log 2>&1
```

Zusätzliche Optionen:

- `--force-convert` – erzwingt einen Re-Import, auch wenn die Liste aktuell ist.
- `--diff-mode` – verarbeitet die Differenz-Datei statt der Vollversion.
- `--download-only` – nur herunterladen, nicht importieren.
- `--update` – erstellt Template-Datenbank und Standardkonfiguration.

Eine vollständige Auflistung liefert `mv2mariadb --help`.

## Container-Nutzung

Das Docker-Image `dbt1/mediathek-importer:latest` steht für amd64 und arm64
bereit:

```bash
docker pull dbt1/mediathek-importer:latest

# Templates/Config initial anlegen
docker run --rm \
  -v $PWD/config/importer:/opt/importer/config \
  -v $PWD/data/importer:/opt/importer/bin/dl \
  --network mediathek-net \
  dbt1/mediathek-importer --update

# Regelmäßiger Import (z. B. via Cron)
docker run --rm \
  -v $PWD/config/importer:/opt/importer/config \
  -v $PWD/data/importer:/opt/importer/bin/dl \
  --network mediathek-net \
  dbt1/mediathek-importer --cron-mode 120 --cron-mode-echo
```

Im `config/importer`-Ordner müssen `mv2mariadb.conf` und `pw_mariadb` liegen.
Das Daten-Volume `data/importer` enthält Downloads und Zwischendateien (ca. 2 GB
Freiraum einplanen). Gleiches Vorgehen funktioniert auf Raspberry Pi und PC.

Wichtig: In `mv2mariadb.conf` muss `mysqlHost=<Hostname>` auf deinen MariaDB-
Server zeigen. Bei `--network host` entspricht das normalerweise `localhost`
oder deiner realen IP. Nutzt du ein Compose-Netz, trage dort den Service-Namen
ein (z. B. `mysqlHost=db`).

> Hinweis: `mediathek-net` steht exemplarisch für ein vorhandenes Docker-Netz
> (z. B. Compose). Läuft MariaDB auf dem Host, verwende `--network host` oder
> setze in `mv2mariadb.conf` den Parameter `mysqlHost=<IP/Hostname>` passend.
> Für dauerhafte Container `--name mediathek-importer` setzen und `--rm` weglassen.
## Entwicklung & Tests

- `make clean && make` – Neubau
- `make format` – (WIP) Code-Formatierung
- `./build/mv2mariadb --debug-print` – Debug-Ausgabe aktivieren

## Versionierung

Dieses Release ist als **v0.5.0** getaggt. Es enthält insbesondere die neue
`mysqlHost`-Option, damit Container nicht zwingend `127.0.0.1` verwenden müssen.
Wir folgen SemVer; Releases werden über `git tag -l` ersichtlich.

Die Versionsnummer liegt zusätzlich in der Datei `VERSION`. Aktuell muss sie
manuell gepflegt werden (`VERSION="x.y.z"` setzen, danach taggen). Der zuvor
geplante automatische Workflow ist vorübergehend deaktiviert und kann später
reaktiviert werden, sobald der Prozess stabil läuft.

## Support

Fragen oder Beiträge bitte als Issue/Pull-Request einreichen:
<https://github.com/tuxbox-neutrino/db-import>.
