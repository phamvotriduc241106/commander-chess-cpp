FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    pkg-config \
    libsdl2-dev \
    libsdl2-image-dev \
    libsdl2-ttf-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN g++ -std=c++17 -O2 -I. -o commanderchess-api server.cpp engine.cpp \
    $(sdl2-config --cflags --libs) -lSDL2_image -lSDL2_ttf -lm -lpthread

ENV PORT=8080
EXPOSE 8080

CMD ["./commanderchess-api"]
