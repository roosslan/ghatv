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
    && chmod +x /usr/local/bin/ttyd
# Своя страница ttyd: встроенная страница ttyd с заменой значка и заголовка вкладки
# и скриптом для Ctrl+Enter. Страница берётся у установленного ttyd, поэтому совпадает с его версией.
# xterm.js отправляет на Ctrl+Enter то же, что на Enter; скрипт вместо этого отправляет код
# Ctrl+Enter в формате протокола kitty (ESC [13;5u), который tvision распознаёт. Скрипт записан без символов
# «\», «&» и «|», чтобы его можно было подставить через sed.
COPY assets/favicon.png assets/ctrl-enter.html /usr/share/ghatv/
RUN set -e; \
    ttyd --port 7682 true >/dev/null 2>&1 & pid=$!; \
    for i in $(seq 1 50); do curl -fsS -o /tmp/index.html http://127.0.0.1:7682/ && break; sleep 0.1; done; \
    kill "$pid"; \
    icon=$(base64 -w0 /usr/share/ghatv/favicon.png); \
    sed -e "s|href=\"data:image/png;base64,[A-Za-z0-9+/=]*\"|href=\"data:image/png;base64,${icon}\"|" \
        -e 's|<title>ttyd - Terminal</title>|<title>ghatv</title>|' \
        -e "s|</body>|$(cat /usr/share/ghatv/ctrl-enter.html)</body>|" \
        /tmp/index.html > /usr/share/ghatv/index.html; \
    grep -q "$icon" /usr/share/ghatv/index.html; \
    grep -q '<title>ghatv</title>' /usr/share/ghatv/index.html; \
    grep -q '13;5u' /usr/share/ghatv/index.html; \
    rm /tmp/index.html /usr/share/ghatv/ctrl-enter.html; \
    apt-get purge -y curl && apt-get autoremove -y
COPY --from=build /src/build/ghatv /usr/local/bin/ghatv
ENV LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    TERM=xterm-256color \
    COLORTERM=truecolor
EXPOSE 7681
# --url-arg: язык можно выбрать адресом http://localhost:7681/?arg=--lang=ru
ENTRYPOINT ["tini", "--", "ttyd", "--writable", "--url-arg", "--port", "7681", \
            "--index", "/usr/share/ghatv/index.html", \
            "-t", "titleFixed=ghatv", \
            "-t", "fontSize=15", \
            "-t", "disableLeaveAlert=true", \
            "-t", "disableResizeOverlay=true", \
            "ghatv"]
