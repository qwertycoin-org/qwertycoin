# builder stage
FROM ubuntu:18.04 as builder

RUN set -ex && \
    apt-get update && \
    apt-get install --no-install-recommends --yes \
        ca-certificates \
        cmake \
        make \
        git \
        g++ \
        curl \
        libssl-dev

WORKDIR /usr/local

ENV CFLAGS='-fPIC'
ENV CXXFLAGS='-fPIC'

#Cmake
ARG CMAKE_VERSION=3.14.0-rc4
ARG CMAKE_VERSION_DOT=v3.14
ARG CMAKE_HASH=8ec90cfaef2b45f70ec04c87ba9f364f90470ee49a14a5af161fead620a81b05
RUN set -ex \
    && curl -s -O https://cmake.org/files/${CMAKE_VERSION_DOT}/cmake-${CMAKE_VERSION}.tar.gz \
    && echo "${CMAKE_HASH}  cmake-${CMAKE_VERSION}.tar.gz" | sha256sum -c \
    && tar -xzf cmake-${CMAKE_VERSION}.tar.gz \
    && cd cmake-${CMAKE_VERSION} \
    && ./configure \
    && make \
    && make install

# build install curl with ssl
ARG CURL_VERSION=7.74.0
RUN set -ex && \
    apt-get update && \
    apt-get install -y wget && \
    wget https://curl.se/download/curl-${CURL_VERSION}.tar.gz && \
    tar -xvf curl-${CURL_VERSION}.tar.gz && \
    cd curl-${CURL_VERSION} && \
    ./configure --with-ssl && \
    make && \
    make install


ENV OPENSSL_ROOT_DIR=/usr/local/openssl-${OPENSSL_VERSION}

WORKDIR /src
COPY . .

RUN set -ex && \
    mkdir ./build
#    cd /src/cmake && \
#    git clone https://github.com/ruslo/polly.git polly

#RUN set -ex && \
#    git submodule init && git submodule update && \

WORKDIR /src/build

ENV USE_SINGLE_BUILDDIR=1
ARG NPROC
RUN set -ex && \
    if [ -z "$NPROC" ] ; \
    then cmake -DBUILD_WITH_TOOLS:BOOL=TRUE -DBUILD_WITH_TESTS:BOOL=FALSE .. && cmake --build . --config Release ; \
    else cmake -DBUILD_WITH_TOOLS:BOOL=TRUE -DBUILD_WITH_TESTS:BOOL=FALSE .. && cmake --build --parallel $NPROC . --config Release ; \
    fi


# runtime stage
FROM ubuntu:18.04

# cleanup
RUN set -ex && \
    apt-get clean && \
    rm -rf /var/lib/apt
COPY --from=builder /src/build/src/ /usr/local/bin/

# create directories
RUN mkdir -p /app /blockchain /logs


COPY ./utils/docker/entrypoint.sh /app/entrypoint.sh
RUN chmod +x /app/entrypoint.sh

VOLUME /blockchain
VOLUME /logs

EXPOSE 8196
EXPOSE 8197

ENTRYPOINT ["/app/entrypoint.sh"]
