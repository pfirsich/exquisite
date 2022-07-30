FROM centos:7

RUN yum install -y centos-release-scl
RUN yum install -y devtoolset-11
RUN yum install -y python3 unzip wget

RUN pip3 install conan meson

RUN wget https://github.com/Kitware/CMake/releases/download/v3.23.3/cmake-3.23.3-linux-x86_64.tar.gz
RUN tar xf cmake-3.23.3-linux-x86_64.tar.gz
RUN ln -s /cmake-3.23.3-linux-x86_64/bin/cmake /usr/bin/cmake

RUN wget https://github.com/ninja-build/ninja/releases/download/v1.11.0/ninja-linux.zip
RUN unzip ninja-linux.zip
RUN ln -s /ninja /usr/bin/ninja

ENV PATH="/opt/rh/devtoolset-11/root/bin/:${PATH}"

RUN conan profile new default --detect
RUN conan profile update settings.compiler.libcxx=libstdc++11 default

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
ENTRYPOINT ["tini", "--"]
CMD ["/root/exquisite"]
