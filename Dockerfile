FROM ubuntu:20.04

# Install dependencies

RUN apt-get update && \
        DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt-get install -y \
            g++-arm-linux-gnueabi \
            build-essential \
            cmake git

ENTRYPOINT ["/bin/bash"]


