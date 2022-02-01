FROM ubuntu:20.04 AS builder
WORKDIR /exq
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/London
RUN apt update && apt install --yes ninja-build cmake clang-12 build-essential
COPY cmake/ ./cmake/
COPY deps/ ./deps/
COPY src ./src/
COPY CMakeLists.txt ./
RUN mkdir build && cd build && \
  cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=clang-12 -DCMAKE_CXX_COMPILER=clang++-12 .. && \
  ninja

FROM ubuntu:20.04
WORKDIR /app
RUN apt update && apt install --yes tini
COPY --from=builder /exq/build/exquisite /root/exquisite
ENTRYPOINT ["tini", "--"]
CMD ["/root/exquisite"]
