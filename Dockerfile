FROM ubuntu:20.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends clang make libc6-dev valgrind python3 && rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
COPY . .
RUN make
CMD ["/bin/bash"]
