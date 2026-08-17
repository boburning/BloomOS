FROM docker.io/library/alpine@sha256:4bcff63911fcb4448bd4fdacec207030997caf25e9bea4045fa6c8c44de311d1

RUN apk add --no-cache bats-core=1.12.0-r0 coreutils=9.7-r1 jq=1.8.1-r0 openssl=3.5.7-r0 python3=3.12.14-r0

WORKDIR /workspace
ENTRYPOINT ["bats"]
