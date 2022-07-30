FROM ubuntu:20.04 AS builder
WORKDIR /exq
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/London
RUN apt update && apt install --yes build-essential clang-12 cmake ninja-build python3-pip
RUN python3 -m pip install conan
COPY cmake/ ./cmake/
COPY deps/ ./deps/
COPY src ./src/
COPY CMakeLists.txt conanfile.txt ./
RUN mkdir build && cd build
RUN cd build && conan install --build=missing ..
RUN cd build && \
  cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=clang-12 -DCMAKE_CXX_COMPILER=clang++-12 .. && \
  ninja

FROM ubuntu:20.04
WORKDIR /app
RUN apt update && apt install --yes tini
COPY --from=builder /exq/build/exquisite /root/exquisite
ENTRYPOINT ["tini", "--"]
CMD ["/root/exquisite"]
