# Multi-stage build for WaitingTheLongest C++ Application
# Stage 1: Builder

FROM ubuntu:22.04 AS builder

LABEL maintainer="WaitingTheLongest Development Team"
LABEL description="Builder stage for WaitingTheLongest C++ Application"

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build tools and development dependencies (single layer for caching)
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    g++ \
    make \
    git \
    curl \
    ca-certificates \
    libssl-dev \
    uuid-dev \
    zlib1g-dev \
    libc-ares-dev \
    libpq-dev \
    libpqxx-dev \
    libjsoncpp-dev \
    libbrotli-dev \
    libsqlite3-dev \
    libhiredis-dev \
    libcrypt-dev \
    libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Install header-only libraries (rarely changes - good cache layer)
RUN git clone --depth 1 https://github.com/kazuho/picojson.git /tmp/picojson && \
    mkdir -p /usr/local/include/picojson && \
    cp /tmp/picojson/picojson.h /usr/local/include/picojson/ && \
    rm -rf /tmp/picojson

WORKDIR /build/jwt-cpp
RUN git clone --depth 1 https://github.com/Thalhammer/jwt-cpp.git . && \
    cp -r include/jwt-cpp /usr/local/include/ && \
    rm -rf /build/jwt-cpp/.git

# Build and install Drogon framework (rarely changes - good cache layer)
WORKDIR /build/drogon
RUN git clone --depth 1 --branch v1.9.1 --recursive https://github.com/drogonframework/drogon.git . && \
    mkdir -p build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    make install && \
    ldconfig

# Set working directory for application build
WORKDIR /build/app

# Copy CMakeLists.txt first for cmake configure caching
COPY CMakeLists.txt .

# Copy source directories (split for better cache invalidation)
COPY src/ ./src/

# Configure the application build
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build the application
RUN cmake --build build -j$(nproc)

# Stage 2: Runtime
FROM ubuntu:22.04 AS runtime

LABEL maintainer="WaitingTheLongest Development Team"
LABEL version="1.0"
LABEL description="WaitingTheLongest C++ Application - HTTP Server with PostgreSQL"

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies only
RUN apt-get update && apt-get install -y --no-install-recommends \
    libpq5 \
    libpqxx-6.4 \
    libjsoncpp25 \
    libuuid1 \
    libssl3 \
    libsqlite3-0 \
    libcurl4 \
    libcrypt1 \
    zlib1g \
    libc-ares2 \
    libbrotli1 \
    libhiredis0.14 \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -m -u 1000 -s /sbin/nologin wtl

# Set working directory
WORKDIR /app

# Copy built binary from builder stage
COPY --from=builder /build/app/build/WaitingTheLongest ./

# Copy Drogon shared libraries
COPY --from=builder /usr/local/lib/libdrogon* /usr/local/lib/
COPY --from=builder /usr/local/lib/libtrantor* /usr/local/lib/
RUN ldconfig

# Copy static files, configuration, and migrations (separate layers for caching)
COPY --chown=wtl:wtl config/ ./config/
COPY --chown=wtl:wtl static/ ./static/
COPY --chown=wtl:wtl sql/ ./sql/
COPY --chown=wtl:wtl content_templates/ ./content_templates/

# Ensure proper permissions for application and data directories
RUN chown -R wtl:wtl /app && \
    chmod +x /app/WaitingTheLongest && \
    mkdir -p /app/logs /app/uploads && \
    chown -R wtl:wtl /app/logs /app/uploads && \
    chmod 755 /app/logs /app/uploads

# Switch to non-root user
USER wtl

# Expose application port
EXPOSE 8080

# Healthcheck
HEALTHCHECK --interval=30s --timeout=3s --start-period=15s --retries=3 \
    CMD curl -f http://localhost:8080/api/health || exit 1

# Run the application
CMD ["./WaitingTheLongest"]
