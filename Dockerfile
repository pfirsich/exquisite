FROM cpp-builder as builder

WORKDIR /exq
COPY cmake/ ./cmake/
COPY deps/ ./deps/
COPY src ./src/
COPY CMakeLists.txt conanfile.txt ./

RUN mkdir build && cd build && \
  conan install --build=missing .. && \
  cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && \
  ninja

FROM centos:7

ENV TINI_VERSION v0.19.0
ADD https://github.com/krallin/tini/releases/download/${TINI_VERSION}/tini /tini
RUN chmod +x /tini
ENTRYPOINT ["/tini", "--"]

COPY --from=builder /exq/build/exquisite /root/exquisite
CMD ["/root/exquisite"]
