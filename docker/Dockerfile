FROM debian:12-slim AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      build-essential \
      pkg-config \
      git \
      ca-certificates \
      python3 \
      libmariadb-dev \
      libmariadb-dev-compat \
      libcurl4-openssl-dev \
      liblzma-dev \
      libexpat1-dev && \
    rm -rf /var/lib/apt/lists/*

RUN git clone --depth=1 --branch v1.0.2 https://github.com/Tencent/rapidjson.git /tmp/rapidjson && \
    cp -r /tmp/rapidjson/include/rapidjson /usr/local/include/ && \
    rm -rf /tmp/rapidjson

WORKDIR /opt/src/db-import

COPY . /opt/src/db-import

RUN python3 docker/patch_sources.py && \
    make -C /opt/src/db-import -j"$(nproc)"

FROM debian:12-slim

ENV DEBIAN_FRONTEND=noninteractive \
    IMPORTER_HOME=/opt/importer

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      ca-certificates \
      libmariadb3 \
      libcurl4 \
      liblzma5 \
      libexpat1 && \
    rm -rf /var/lib/apt/lists/*

COPY --from=build /opt/src/db-import/build/mv2mariadb /opt/importer/bin/mv2mariadb
COPY docker/entrypoint.sh /usr/local/bin/importer-entrypoint

RUN chmod +x /usr/local/bin/importer-entrypoint

ENTRYPOINT ["/usr/local/bin/importer-entrypoint"]
