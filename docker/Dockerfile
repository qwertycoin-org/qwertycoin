# syntax=docker/dockerfile:1.7@sha256:a57df69d0ea827fb7266491f2813635de6f17269be881f696fbfdf2d83dda33e

ARG BASE_IMAGE=docker.io/library/debian:bookworm-slim@sha256:88200866dfff7ea7f5cbcb6ec7c8a701889efe6fe859fe64d6990e4b07ea4171
ARG CA_CERTIFICATES_VERSION=20250419~deb12u1
FROM ${BASE_IMAGE} AS certificates

ARG CA_CERTIFICATES_VERSION
RUN set -eu; \
    apt-get update; \
    DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends --yes \
      "ca-certificates=${CA_CERTIFICATES_VERSION}"; \
    rm -rf /var/lib/apt/lists/*

FROM ${BASE_IMAGE}

ARG CA_CERTIFICATES_VERSION
ARG QWC_RELEASE_TAG
ARG QWC_RELEASE_VERSION
ARG QWC_SOURCE_REVISION
ARG QWC_RELEASE_ARCHIVE_SHA256
ARG QWC_BASE_IMAGE_DIGEST
ARG QWC_PACKAGING_REVISION
ARG QWC_IMAGE_REVISION=1

LABEL org.opencontainers.image.title="Qwertycoin Core" \
      org.opencontainers.image.description="Qwertycoin mainnet daemon, CLI wallet and wallet RPC" \
      org.opencontainers.image.source="https://github.com/qwertycoin-org/qwertycoin" \
      org.opencontainers.image.url="https://github.com/qwertycoin-org/qwertycoin" \
      org.opencontainers.image.licenses="BSD-3-Clause" \
      org.opencontainers.image.version="${QWC_RELEASE_VERSION}" \
      org.opencontainers.image.revision="${QWC_SOURCE_REVISION}" \
      org.qwertycoin.release.tag="${QWC_RELEASE_TAG}" \
      org.qwertycoin.release.archive.sha256="${QWC_RELEASE_ARCHIVE_SHA256}" \
      org.qwertycoin.runtime.base.digest="${QWC_BASE_IMAGE_DIGEST}" \
      org.qwertycoin.runtime.ca-certificates.version="${CA_CERTIFICATES_VERSION}" \
      org.qwertycoin.packaging.revision="${QWC_PACKAGING_REVISION}" \
      org.qwertycoin.image.revision="${QWC_IMAGE_REVISION}"

COPY --from=certificates /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/ca-certificates.crt

RUN set -eu; \
    groupadd --gid 10001 qwertycoin; \
    useradd --uid 10001 --gid 10001 --home-dir /home/qwertycoin --create-home \
      --shell /usr/sbin/nologin qwertycoin; \
    install -d -o 10001 -g 10001 -m 0700 /data /wallet /service-node

COPY --chown=0:0 release/qwertycoind release/qwertycoin-wallet-cli release/qwertycoin-wallet-rpc /opt/qwertycoin/
COPY --chown=0:0 release/lib/ /opt/qwertycoin/lib/
COPY --chown=0:0 release/LICENSE release/THIRD-PARTY-NOTICES.md /usr/share/doc/qwertycoin/
COPY --chown=0:0 entrypoint.sh /usr/local/bin/qwertycoin-entrypoint

RUN set -eu; \
    chmod 0755 \
      /opt/qwertycoin/qwertycoind \
      /opt/qwertycoin/qwertycoin-wallet-cli \
      /opt/qwertycoin/qwertycoin-wallet-rpc \
      /usr/local/bin/qwertycoin-entrypoint; \
    find /opt/qwertycoin/lib -type f -exec chmod 0644 {} +

ENV HOME=/home/qwertycoin \
    SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt \
    PATH=/opt/qwertycoin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

EXPOSE 8196 8197 8198 8199

USER 10001:10001
STOPSIGNAL SIGINT
ENTRYPOINT ["/usr/local/bin/qwertycoin-entrypoint"]
CMD ["--data-dir=/data", "--p2p-bind-ip=0.0.0.0", "--p2p-bind-port=8196", "--rpc-bind-ip=127.0.0.1", "--rpc-bind-port=8197", "--no-zmq", "--non-interactive", "--no-igd"]
