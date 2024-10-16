FROM gcc:13.2 AS builder

RUN apt-get update && apt-get install -y cmake libsqlite3-dev

WORKDIR /root/todo

COPY cmake/ cmake/
COPY CMakeLists.txt .

RUN mkdir -p src/ && echo "" > src/dummy.cpp

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=23
RUN cmake --build build -t delameta && \
    cmake --build build -t fmt && \
    cmake --build build -t Catch2

COPY src/ src/

RUN cmake -B build -DCMAKE_EXE_LINKER_FLAGS="-static" -DCMAKE_FIND_STATIC_PREFER=ON && \
    cmake --build build -t todo

FROM alpine:3.20.3

COPY --from=builder /root/todo/build/todo /usr/bin/todo
COPY assets/ /usr/share/todo/assets/
COPY static/ /usr/share/todo/static/

WORKDIR /root/todo
RUN todo --test

EXPOSE 5000
CMD ["todo", "--host=0.0.0.0:5000"]
