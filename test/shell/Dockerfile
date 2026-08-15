FROM docker.io/library/alpine@sha256:4bcff63911fcb4448bd4fdacec207030997caf25e9bea4045fa6c8c44de311d1

RUN apk add --no-cache bats-core=1.12.0-r0

WORKDIR /workspace
ENTRYPOINT ["bats"]
