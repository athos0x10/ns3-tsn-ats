# Setting up a Reproducible ns-3.40 Environment with Docker

## 1. Motivation
Building ns-3 natively on modern macOS systems (M1/M2) or with cutting-edge Python versions (like 3.14) often leads to compiler mismatch errors, missing standard C++ libraries (`<algorithm>`, `<string>`), and strict parsing errors without forgetting issue with clang's version.

To ensure a stable, reproducible, and cross-platform development environment, I use a Dockerized Ubuntu container. This is particularly crucial to ensure seamless compatibility with the external **TSN module** developed by the IRT Saint Exupéry, known as the [Eden-sim project](https://sahara.irt-saintexupery.com/embedded-systems/eden-sim), alongside our custom ATS (Asynchronous Traffic Shaping) implementations. 

Reference: [ns-3.40 Official Release](https://www.nsnam.org/releases/ns-3-40/)

---

## 2. The Dockerfile
Create a file named `Dockerfile` (no extension) in the root of your `ns3-workspace` directory with the following content:

```dockerfile
# Base image: Ubuntu 22.04 LTS (Standard for ns-3 development)
FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install required system dependencies for C++ and ns-3
RUN apt-get update && apt-get install -y \
    gcc g++ python3 python3-dev cmake ninja-build \
    git qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools \
    libxml2 libxml2-dev libsqlite3-dev \
    && apt-get clean

# Set the working directory inside the container
WORKDIR /ns3
```

---

## 3. Step-by-Step Workflow

### Step 1: Build the Docker Image
Open your terminal on your host machine, navigate to the folder containing the `Dockerfile`, and build the image:
```bash
docker build -t ns3-env .
```

### Step 2: Run the Container (Volume Mounting)
Run the container while mounting your current local directory into the container's `/ns3` folder. This allows you to use your host's IDE (like VS Code) to edit files, while the container handles the compilation.
```bash
docker run -it -v $(pwd):/ns3 ns3-env bash
```

### Step 3: Configure and Build ns-3
Once inside the container (you will see a `root@...:/ns3#` prompt), navigate to the ns-3 directory and configure the project. 
*Note: We disable `NS3_WARNINGS_AS_ERRORS` to prevent the build from failing due to unused variables or functions in the third-party Eden-sim TSN module.*

```bash
cd ns-allinone-3.40/ns-3.40

# Configure the build profile
./ns3 configure --build-profile=debug --enable-examples --enable-tests -- -DNS3_WARNINGS_AS_ERRORS=OFF

# Compile the project
./ns3 build
```

### Step 4: Run Tests
To verify the installation:
```bash
./test.py
```