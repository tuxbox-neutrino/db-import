# mv2mariadb – MediathekView to MariaDB converter

`mv2mariadb` pulls the MediathekView movie list and writes it into MariaDB. It
feeds the Neutrino Mediathek plugin.

Quickest path: run the [quickstart script](https://github.com/tuxbox-neutrino/mt-api-dev/blob/master/scripts/quickstart.sh)
from the [mediathek-backend](https://github.com/tuxbox-neutrino/mediathek-backend)
repo (`vendor/mt-api-dev/scripts/quickstart.sh`). It asks for DB credentials (or
starts its own MariaDB), writes the configs and starts importer + API.

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

## Quickstart (Docker Compose)

Use the compose stack from the
[mediathek-backend](https://github.com/tuxbox-neutrino/mediathek-backend)
repository (imports everything into MariaDB and exposes the API):

```bash
make vendor                      # clone mt-api-dev & db-import
docker-compose up -d db          # start MariaDB
docker-compose run --rm importer --update
docker-compose run --rm importer # run full conversion
```

Afterwards the API (port 18080) and any Neutrino client can read the database.

## Build it yourself (Debian/Ubuntu)

```bash
sudo apt-get update
sudo apt-get install build-essential pkg-config git \
  libmariadb-dev libcurl4-openssl-dev liblzma-dev libexpat1-dev rapidjson-dev

git clone https://github.com/tuxbox-neutrino/db-import.git
cd db-import
cp doc/config.mk.sample config.mk
make -j$(nproc)
```

Binary: `build/mv2mariadb`. Optional: `make install DESTDIR=/opt/importer`.

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

## Runtime config (must be present)

- `config/mv2mariadb.conf`: download URLs, target schemas, `mysqlHost`.
- `config/pw_mariadb`: `user:password` with CREATE/ALTER rights.
- Working files/logs: `bin/dl`.

## How to run

Common options:

- `--update` – create template DB + default config and exit.
- `--force-convert` – re-import even if the list is up to date.
- `--diff-mode` – use the diff list instead of the full list.
- `--download-only` – just download, no SQL import.
- `--debug-channels <pattern>` – print channel ↔ channelinfo mappings for a
  pattern (e.g. `--debug-channels ard`) when troubleshooting mislabelled
  senders.

Example cron (every 2h, log only on change):
```
*/120 * * * * /opt/importer/bin/mv2mariadb --cron-mode 120 --cron-mode-echo >>/var/log/mv2mariadb.log 2>&1
```

`mv2mariadb --help` lists all flags.

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
