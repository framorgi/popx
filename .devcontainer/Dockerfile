# Use a base image, you can choose an appropriate base image based on your requirements
FROM ubuntu:latest


ARG DEBIAN_FRONTEND=noninteractive


# Update system and install some packets
RUN apt update && apt install -y \
    sudo \
    build-essential \
    bash-completion \
    tcpdump \
    netcat-traditional \
    file \
    git \
    wget \
    curl \
    ca-certificates \
    cmake \
    ninja-build \
    gdb \
    gdb-multiarch \
    doxygen \
    valgrind \
    pkg-config \
    gcovr \
    googletest \
    libgtest-dev \
    libgmock-dev \
    clang \
    clang-tools \
    clang-format \
    clang-tidy \
    gcc \
    g++ \
    libncurses-dev \
 
    zlib1g-dev \
    llvm \
    cppcheck \
    python3 \
    python3-venv \
    libspdlog-dev \
    libfmt-dev \
    libzmq3-dev \
    libmsgpack-cxx-dev \
    pandoc \
    default-jre \
    wget \
    vim \
    htop \
    iputils-ping \
    texlive-latex-base \
    texlive-fonts-recommended \
    texlive-latex-extra \
    lldb \
    python3-pip\
    libusb-1.0-0-dev\
    libsfml-dev


# Install CodeChecker
RUN python3 -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"
RUN pip install codechecker



# Compile CA certificates
RUN update-ca-certificates
 

# Add ubuntu user to the sudoers
RUN echo 'ubuntu:!ubuntu!' | chpasswd && \
    usermod -aG sudo ubuntu && \
    echo 'ubuntu ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

 
 
CMD ["bash"]





