FROM gcc:13.2

RUN apt-get update && apt-get install -y cmake libsqlite3-dev
ENV CPM_SOURCE_CACHE=/root/.cache/CPM

WORKDIR /root/todo

COPY cmake/ cmake/
COPY CMakeLists.txt .

RUN mkdir -p src/ && echo "" > src/dummy.cpp
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=23

COPY src/ src/

RUN cmake -B build
RUN cmake --build build

EXPOSE 5000

CMD ["./build/todo", "--host=0.0.0.0:5000"]
