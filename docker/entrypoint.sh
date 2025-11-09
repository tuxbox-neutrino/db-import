#!/usr/bin/env bash
set -euo pipefail

IMPORTER_HOME=${IMPORTER_HOME:-/opt/importer}
CONFIG_DIR="${IMPORTER_HOME}/config"
BIN_DIR="${IMPORTER_HOME}/bin"
LOG_DIR="${IMPORTER_HOME}/log"

mkdir -p "${CONFIG_DIR}" "${BIN_DIR}" "${LOG_DIR}"

CONFIG_SOURCE="${CONFIG_SOURCE:-/opt/importer.dist/config}"
if [[ -d "${CONFIG_SOURCE}" ]]; then
  cp -a "${CONFIG_SOURCE}/." "${CONFIG_DIR}/"
fi

if [[ ! -f "${CONFIG_DIR}/mv2mariadb.conf" ]]; then
  echo "[entrypoint] Missing importer config mv2mariadb.conf" >&2
  exit 1
fi

cp "${CONFIG_DIR}/mv2mariadb.conf" "${BIN_DIR}/mv2mariadb.conf"

if [[ -f "${CONFIG_DIR}/pw_mariadb" ]]; then
  cp "${CONFIG_DIR}/pw_mariadb" "${BIN_DIR}/pw_mariadb"
else
  echo "[entrypoint] Warning: pw_mariadb not found in ${CONFIG_DIR} – proceeding without it." >&2
fi

exec "${BIN_DIR}/mv2mariadb" "$@"
