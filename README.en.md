# mv2mariadb – MediathekView to MariaDB converter

`mv2mariadb` downloads the MediathekView movie list, converts it into MariaDB
tables and serves as the data source for the Neutrino Mediathek plugin.

> Need a turnkey setup? Run the [quickstart script](https://github.com/tuxbox-neutrino/mt-api-dev/blob/master/scripts/quickstart.sh)
> from `mt-api-dev` (bundled in the [mediathek-backend](https://github.com/tuxbox-neutrino/mediathek-backend)
> repo under `vendor/mt-api-dev/scripts/quickstart.sh`).
> It asks for the MariaDB credentials (or spins up its own `mariadb` container),
> generates the importer/API configs and launches both containers automatically.

## Table of Contents

- [Feature overview](#feature-overview)
- [Quickstart (Docker Compose)](#quickstart-docker-compose)
- [Requirements (manual build)](#requirements-manual-build)
- [Build](#build)
- [Configuration](#configuration)
- [Operation](#operation)
- [Container usage](#container-usage)
- [Development & testing](#development--testing)
- [Versioning](#versioning)
- [Support](#support)

## Feature overview

Core capabilities include:

- Automatically maintains the download server list.
- Detects new MediathekView releases (full + diff lists).
- Downloads, unpacks and imports JSON data into MariaDB schemas
  (`mediathek_1`, temporary tables, template DB).
- Cron-friendly operation that only downloads when necessary.

## Quickstart (Docker Compose)

When working inside `neutrino-make`, the bundled compose stack wires MariaDB,
importer and API exactly like the real deployment.

Use the compose stack from the [mediathek-backend](https://github.com/tuxbox-neutrino/mediathek-backend)
repository:

```bash
make vendor                      # clone mt-api-dev & db-import
docker-compose up -d db          # start MariaDB
docker-compose run --rm importer --update
docker-compose run --rm importer # run full conversion
```

The resulting tables can be consumed by the API container or any local Neutrino
setup.

## Requirements (manual build)

Build-from-source prerequisites:

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

Compile the importer locally with:

```bash
git clone https://github.com/tuxbox-neutrino/db-import.git
cd db-import
cp doc/config.mk.sample config.mk
make -j$(nproc)
```

The binary will be placed under `build/mv2mariadb`. Use `make install
DESTDIR=/opt/importer` if you want a staged directory layout similar to the
Docker image.

> **Version embedding**  
> `git describe --tags` is automatically embedded into the binary and written to
> the database tables (visible via the API). To force a specific number (e.g.
> when building from a tarball) run `IMPORTER_VERSION=0.5.0 make`.

## Configuration

The following files control runtime behaviour:

- `config/mv2mariadb.conf` – download URLs, target schemas and the new
  `mysqlHost` option so containerised runs can reach the Compose DB host `db`.
- `config/pw_mariadb` – `user:password`, copied to `bin/pw_mariadb` at runtime.
  Use a MariaDB account with CREATE/ALTER privileges on the target schemas.
- Working files and logs are stored in `bin/dl`.

## Operation

Schedule the importer or run it manually with these common options:

Typical cron entry (every 120 minutes, verbose output when updates occur):

```
*/120 * * * * /opt/importer/bin/mv2mariadb --cron-mode 120 --cron-mode-echo >>/var/log/mv2mariadb.log 2>&1
```

Useful CLI flags:

- `--force-convert` – re-import even if the list is up to date.
- `--diff-mode` – use the differential list instead of the full one.
- `--download-only` – skip SQL import.
- `--update` – create template DB + default config and exit.
- `--debug-channels <pattern>` – dump channel ↔ channelinfo mappings that match
  the pattern (e.g. `--debug-channels ard`). Use this when investigating
  reported mismatches between list entries and their assigned channel; the
  output makes visible which `channelinfo` entry a sender currently maps to.

Run `mv2mariadb --help` for the full option list.

## Container usage

Use the prebuilt Docker image when you prefer a containerised workflow:

Multi-arch images are available via Docker Hub:

```bash
docker pull dbt1/mediathek-importer:latest

# bootstrap templates + config (once)
docker run --rm \
  -v $PWD/config/importer:/opt/importer/config \
  -v $PWD/data/importer:/opt/importer/bin/dl \
  --network mediathek-net \
  dbt1/mediathek-importer --update

# periodic import (e.g. cron/systemd)
docker run --rm \
  -v $PWD/config/importer:/opt/importer/config \
  -v $PWD/data/importer:/opt/importer/bin/dl \
  --network mediathek-net \
  dbt1/mediathek-importer --cron-mode 120 --cron-mode-echo
```

`config/importer` must contain `mv2mariadb.conf` and `pw_mariadb`. The data volume
holds downloads/cache (~2 GB recommended). Works on amd64 PCs and arm64 Raspberry Pis.

Set `mysqlHost=<hostname>` in `mv2mariadb.conf` to match your MariaDB server.
If you rely on `--network host`, this is usually `localhost` or the actual IP.
Within custom Docker networks (compose) the host name should match the service
name (e.g. `mysqlHost=db`).

## Development & testing

Developer helpers:

- `make clean && make` – rebuild
- `make format` – (WIP) formatter target
- `./build/mv2mariadb --debug-print` – verbose importer logs

## Versioning

We cut SemVer releases so the API and plugins can reason about the data source
version.

Release **v0.2.5** introduces the configurable `mysqlHost`, allowing container
setups to talk to non-local MariaDB hosts, and adds the optional
`--debug-channels` helper. We follow Semantic Versioning; all releases are
available via `git tag -l`.

Version numbers are kept in the `VERSION` file. Update it manually (format
`VERSION="x.y.z"`) before tagging/publishing. The previous automated workflow is
temporarily disabled; reactivate or reintroduce it once the tagging process is
stable.

## Support

Issues and pull requests welcome: <https://github.com/tuxbox-neutrino/db-import>.
