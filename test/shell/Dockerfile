FROM docker.io/library/alpine@sha256:4bcff63911fcb4448bd4fdacec207030997caf25e9bea4045fa6c8c44de311d1

COPY SHA256SUMS /tmp/apks/SHA256SUMS
COPY *.apk /tmp/apks/

RUN cd /tmp/apks \
    && sha256sum -c SHA256SUMS \
    && : > /etc/apk/repositories \
    && apk add --no-cache --no-network /tmp/apks/*.apk \
    && rm -rf /tmp/apks

WORKDIR /workspace
ENTRYPOINT ["bats"]
