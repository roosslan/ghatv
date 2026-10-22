# Сборка
FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git ca-certificates \
        libcurl4-openssl-dev nlohmann-json3-dev libncursesw5-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY src ./src
# Версия для «О программе»; по умолчанию — версия из CMakeLists.txt
ARG GHATV_VERSION=
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release ${GHATV_VERSION:+-DGHATV_VERSION=$GHATV_VERSION} \
    && cmake --build build -j"$(nproc)" --target ghatv \
    && strip build/ghatv

# Запуск
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
        tini libcurl4 libncursesw6 ca-certificates tzdata curl \
    && rm -rf /var/lib/apt/lists/*
# ttyd: официальный статический бинарник с проверкой контрольной суммы
ARG TTYD_VERSION=1.7.7
ARG TARGETARCH
RUN case "${TARGETARCH:-amd64}" in \
        amd64) arch=x86_64;  sum=8a217c968aba172e0dbf3f34447218dc015bc4d5e59bf51db2f2cd12b7be4f55 ;; \
        arm64) arch=aarch64; sum=b38acadd89d1d396a0f5649aa52c539edbad07f4bc7348b27b4f4b7219dd4165 ;; \
        *) echo "unsupported arch: ${TARGETARCH}" && exit 1 ;; \
    esac \
    && curl -fsSL -o /usr/local/bin/ttyd \
        "https://github.com/tsl0922/ttyd/releases/download/${TTYD_VERSION}/ttyd.${arch}" \
    && echo "${sum}  /usr/local/bin/ttyd" | sha256sum -c - \
    && chmod +x /usr/local/bin/ttyd \
    && apt-get purge -y curl && apt-get autoremove -y
COPY --from=build /src/build/ghatv /usr/local/bin/ghatv
ENV LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    TERM=xterm-256color \
    COLORTERM=truecolor
EXPOSE 7681
ENTRYPOINT ["tini", "--", "ttyd", "--writable", "--port", "7681", \
            "-t", "titleFixed=ghatv", \
            "-t", "fontSize=15", \
            "-t", "disableLeaveAlert=true", \
            "-t", "disableResizeOverlay=true", \
            "ghatv"]
