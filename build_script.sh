#!/bin/bash
# BUDC Controller Cross-Platform Build Script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/builds"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

usage() {
    echo "BUDC Controller Cross-Platform Build Script"
    echo "Author Sebastien Dudek (@FlUxIuS) at @Penthertz"
    echo ""
    echo "Usage: $0 [OPTIONS] [TARGETS]"
    echo ""
    echo "OPTIONS:"
    echo "  -h, --help     Show this help message"
    echo "  -c, --clean    Clean build directory before building"
    echo "  --no-cache     Build Docker image(s) without cache"
    echo ""
    echo "TARGETS (default: all):"
    echo "  linux          Build static Linux binaries"
    echo "  windows        Build Windows binaries"
    echo "  macos          Build macOS binaries (CLI only)"
    echo "  deb            Build a .deb package for Debian/Ubuntu"
    echo "  all            Build all cross-platform targets (linux, windows, macos)"
    echo ""
}

check_prerequisites() {
    print_header "Checking Prerequisites"
    if ! command -v docker &> /dev/null; then
        print_error "Docker is required but not installed."
        exit 1
    fi
    if ! docker info &> /dev/null; then
        print_error "Docker daemon is not running."
        exit 1
    fi
    print_success "Docker is available"
    if [[ ! -f "CMakeLists.txt" || ! -d "src" ]]; then
        print_error "Run script from the project root directory."
        exit 1
    fi
    print_success "Source files found"
}

build_deb_package() {
    print_header "Building Debian (.deb) Package"
    local deb_dockerfile="Dockerfile.deb"
    if [[ ! -f "$deb_dockerfile" ]]; then
        print_error "$deb_dockerfile not found."
        exit 1
    fi

    mkdir -p "${BUILD_DIR}/deb"
    echo "Building and exporting .deb package..."

    if ! docker build --output type=local,dest="${BUILD_DIR}/deb" -f "$deb_dockerfile" .; then
        print_error "Failed to build .deb package."
        exit 1
    fi
    
    local deb_file=$(find "${BUILD_DIR}/deb" -name "*.deb" -print -quit)
    if [[ -n "$deb_file" ]]; then
        print_success "Package extracted to: ${BUILD_DIR}/deb/$(basename "$deb_file")"
    else
        print_warning "No .deb file found in output"
    fi
}

build_cross_platform_binaries() {
    print_header "Building Cross-Platform Binaries"
    local cross_dockerfile="Dockerfile"
    if [[ ! -f "$cross_dockerfile" ]]; then
        print_error "$cross_dockerfile not found."
        exit 1
    fi

    mkdir -p "${BUILD_DIR}"
    echo "Building and exporting cross-platform binaries..."

    # Add --no-cache flag if requested
    local build_args=""
    if [[ "$NO_CACHE" == "true" ]]; then
        build_args="--no-cache"
        print_warning "Building without cache (this will take longer)"
    fi

    # Builds and exports the 'output' stage directly
    if ! docker build $build_args --target output --output type=local,dest="${BUILD_DIR}" -f "$cross_dockerfile" .; then
        print_error "Failed to build cross-platform binaries."
        echo "Debug: Checking if Docker build produced any intermediate results..."
        
        # Try to get more information about the failure
        docker build $build_args --target output -f "$cross_dockerfile" . || {
            print_error "Docker build failed completely"
            exit 1
        }
        
        print_warning "Build succeeded but file extraction failed. Check Dockerfile output stage."
        exit 1
    fi
    
    # Verify the extraction worked
    if [[ ! -d "${BUILD_DIR}" ]] || [[ -z "$(ls -A "${BUILD_DIR}" 2>/dev/null)" ]]; then
        print_error "Build directory is empty after extraction"
        echo "Attempting to debug the issue..."
        
        # Try building without output to see if the build itself works
        print_warning "Testing build without extraction..."
        if docker build $build_args --target output -f "$cross_dockerfile" .; then
            print_error "Build works but extraction fails. This suggests an issue with the output stage in Dockerfile."
        else
            print_error "Build itself is failing."
        fi
        exit 1
    fi
    
    print_success "Binaries extracted to: ${BUILD_DIR}"
    show_build_results
}

show_build_results() {
    print_header "Build Results (Binaries)"
    if [[ ! -d "$BUILD_DIR" ]]; then
        print_warning "Build directory not found"
        return
    fi
    
    local found_results=false
    find "${BUILD_DIR}" -maxdepth 2 -type d ! -name "builds" | while read -r dir; do
        if [[ -n "$(ls -A "$dir" 2>/dev/null)" && "$(basename "$dir")" != "builds" ]]; then
            echo -e "\n${YELLOW}Target: $(basename "$dir")${NC}"
            found_results=true
            find "$dir" -type f \( -perm /u+x -o -name "*.exe" -o -name "*.deb" \) -print0 | while IFS= read -r -d $'\0' file; do
                local size
                size=$(stat -f%z "$file" 2>/dev/null || stat -c%s "$file" 2>/dev/null || echo "unknown")
                printf "  %-30s %10s bytes\n" "$(basename "$file")" "$size"
            done
        fi
    done
    
    if [[ "$found_results" == "false" ]]; then
        print_warning "No build results found"
    fi
}

clean_builds() {
    print_header "Cleaning Build Directory"
    if [[ -d "$BUILD_DIR" ]]; then
        rm -rf "$BUILD_DIR"
        print_success "Build directory cleaned"
    else
        print_warning "Build directory doesn't exist"
    fi
}

# --- Main Execution Logic ---
TARGETS=()
CLEAN=false
NO_CACHE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        --no-cache)
            NO_CACHE=true
            print_warning "--no-cache is active for Docker builds"
            shift
            ;;
        linux|windows|macos|all|deb)
            TARGETS+=("$1")
            shift
            ;;
        *)
            print_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

if [[ ${#TARGETS[@]} -eq 0 ]]; then
    TARGETS=("all")
fi

main() {
    check_prerequisites
    
    if [[ "$CLEAN" == "true" ]]; then
        clean_builds
    fi
    
    local build_cross=false
    local build_deb=false
    
    for target in "${TARGETS[@]}"; do
        case "$target" in
            linux|windows|macos|all) 
                build_cross=true 
                ;;
            deb) 
                build_deb=true 
                ;;
        esac
    done

    # Build .deb package if requested
    if [[ "$build_deb" == "true" ]]; then
        build_deb_package
    fi
    
    # Build cross-platform binaries if requested
    if [[ "$build_cross" == "true" ]]; then
        build_cross_platform_binaries
    fi
    
    print_success "All requested builds complete!"
    
    # Show summary
    if [[ -d "$BUILD_DIR" ]]; then
        echo ""
        echo -e "${BLUE}Build Summary:${NC}"
        echo "Output directory: $BUILD_DIR"
        echo "Available targets:"
        find "${BUILD_DIR}" -maxdepth 1 -type d ! -name "builds" -exec basename {} \; | sort
    fi
}

main "$@"