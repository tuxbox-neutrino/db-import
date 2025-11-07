# mv2mariadb – MediathekView to MariaDB converter

`mv2mariadb` downloads the MediathekView movie list, converts it into MariaDB
tables and serves as the data source for the Neutrino Mediathek plugin.

## Feature overview

- Automatically maintains the download server list.
- Detects new MediathekView releases (full + diff lists).
- Downloads, unpacks and imports JSON data into MariaDB schemas
  (`mediathek_1`, temporary tables, template DB).
- Cron-friendly operation that only downloads when necessary.

## Quickstart (Docker Compose)

Use the stack in `services/mediathek-backend` of the `neutrino-make` repo:

```bash
make vendor                      # clone mt-api-dev & db-import
docker-compose up -d db          # start MariaDB
docker-compose run --rm importer --update
docker-compose run --rm importer # run full conversion
```

The resulting tables can be consumed by the API container or any local Neutrino
setup.

## Requirements (manual build)

- GCC or Clang, `make`
- MariaDB Connector/C (`libmariadb-dev`)
- libcurl, liblzma, libexpat, libpthread
- rapidjson headers

Example for Debian/Ubuntu:

```bash
sudo apt install build-essential pkg-config git libmariadb-dev \
  libcurl4-openssl-dev liblzma-dev libexpat1-dev rapidjson-dev
```

## Build

```bash
git clone https://github.com/tuxbox-neutrino/db-import.git
cd db-import
cp doc/config.mk.sample config.mk
make -j$(nproc)
```

The binary will be placed under `build/mv2mariadb`. Use `make install
DESTDIR=/opt/importer` if you want a staged directory layout similar to the
Docker image.

## Configuration

- `config/mv2mariadb.conf` – download URLs, target schemas and the new
  `mysqlHost` option so containerised runs can reach the Compose DB host `db`.
- `config/pw_mariadb` – `user:password`, copied to `bin/pw_mariadb` at runtime.
  Use a MariaDB account with CREATE/ALTER privileges on the target schemas.
- Working files and logs are stored in `bin/dl`.

## Operation

Typical cron entry (every 120 minutes, verbose output when updates occur):

```
*/120 * * * * /opt/importer/bin/mv2mariadb --cron-mode 120 --cron-mode-echo >>/var/log/mv2mariadb.log 2>&1
```

Useful CLI flags:

- `--force-convert` – re-import even if the list is up to date.
- `--diff-mode` – use the differential list instead of the full one.
- `--download-only` – skip SQL import.
- `--update` – create template DB + default config and exit.

Run `mv2mariadb --help` for the full option list.

## Development & testing

- `make clean && make` – rebuild
- `make format` – (WIP) formatter target
- `./build/mv2mariadb --debug-print` – verbose importer logs

## Versioning

Release **v0.5.0** introduces the configurable `mysqlHost`, allowing container
setups to talk to non-local MariaDB hosts. We follow Semantic Versioning; all
releases are available via `git tag -l`.

## Support

Issues and pull requests welcome: <https://github.com/tuxbox-neutrino/db-import>.
