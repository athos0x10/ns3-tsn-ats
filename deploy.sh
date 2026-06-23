#!/bin/bash

# --- Colors ---
NC='\033[0m'               # No Color (Reset)
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD_WHITE='\033[1;37m'

# --- Logging Functions ---
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
    exit 1
}

log_title() {
    echo -e "\n${BOLD_WHITE}=== $1 ===${NC}"
}

# --- Script Execution ---
log_title "Starting Deployment and Build Process"

# Copy ATS files
log_info "Copying ATS source files to eden-sim..."
if cp src/ats/* ../eden-sim/contrib/tsn/model/; then
    log_success "ATS source files copied successfully."
else
    log_error "Failed to copy ATS source files."
fi

# Copy Stream ID tag
log_info "Copying stream-id-tag.h..."
if cp src/stream-id-tag.h ../eden-sim/contrib/traffic-generator/model/; then
    log_success "stream-id-tag.h copied successfully."
else
    log_error "Failed to copy stream-id-tag.h."
fi

# Copy ATS tests
log_info "Copying ats-test-suite.cc..."
if cp test/ats-test-suite.cc ../eden-sim/contrib/tsn/test/; then
    log_success "ats-test-suite.cc copied successfully."
else
    log_error "Failed to copy ats-test-suite.cc"
fi

# Copy contributions to ns-3
log_info "Copying eden-sim contributions to ns-3..."
cd ..
if cp -r eden-sim/contrib/* ns-allinone-3.40/ns-3.40/contrib/; then
    log_success "Contributions copied successfully."
else
    log_error "Failed to copy contributions to ns-3."
fi

# Change directory
log_info "Navigating to ns-3.40 directory..."
if cd ns-allinone-3.40/ns-3.40; then
    log_success "Current directory: $(pwd)"
else
    log_error "Could not access directory: ns-allinone-3.40/ns-3.40"
fi

# Configure ns-3
log_title "Configuring ns-3"
log_info "Running ns-3 configuration..."
if ./ns3 configure --build-profile=debug --enable-examples --enable-tests -- -DNS3_WARNINGS_AS_ERRORS=OFF; then
    log_success "ns-3 configured successfully."
else
    log_error "ns-3 configuration failed."
fi

# Build ns-3
log_title "Building ns-3"
log_info "Running ns-3 build (this may take a while)..."
if ./ns3 build; then
    echo -e "\n${GREEN}=========================================${NC}"
    echo -e "${GREEN}[STATUS] Deployment and build completed successfully.${NC}"
    echo -e "${GREEN}=========================================${NC}"
else
    log_error "ns-3 build failed."
fi