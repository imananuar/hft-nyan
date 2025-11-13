# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(HFT_MarketMaker)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -Wall")

# Find required packages
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP jsoncpp)

# Fallback: if pkg-config didn't provide library paths (can happen on macOS/Homebrew),
# try to locate jsoncpp manually and populate variables used below.
if(NOT JSONCPP_LIBRARIES)
	find_library(JSONCPP_LIB NAMES jsoncpp jsoncpp_lib PATHS /opt/homebrew/lib /usr/local/lib NO_DEFAULT_PATH)
	if(JSONCPP_LIB)
		set(JSONCPP_LIBRARIES ${JSONCPP_LIB})
	endif()
endif()

if(NOT JSONCPP_INCLUDE_DIRS)
	find_path(JSONCPP_INCLUDE_DIR json/json.h PATHS /opt/homebrew/include /usr/local/include NO_DEFAULT_PATH)
	if(JSONCPP_INCLUDE_DIR)
		set(JSONCPP_INCLUDE_DIRS ${JSONCPP_INCLUDE_DIR})
	endif()
endif()

include_directories(${CURL_INCLUDE_DIR} ${JSONCPP_INCLUDE_DIRS})

add_executable(market_maker main.cpp)
## Some pkg-config backends populate a cache variable named pkgcfg_lib_JSONCPP_jsoncpp
## (see CMakeCache.txt). Prefer that when available, otherwise use JSONCPP_LIBRARIES.
if(DEFINED pkgcfg_lib_JSONCPP_jsoncpp)
	set(JSONCPP_LINK_LIB ${pkgcfg_lib_JSONCPP_jsoncpp})
else()
	set(JSONCPP_LINK_LIB ${JSONCPP_LIBRARIES})
endif()

target_link_libraries(market_maker ${CURL_LIBRARIES} ${JSONCPP_LINK_LIB} pthread)

# ============================================
# build.sh - Quick build script
# Save as: build.sh
# ============================================
# #!/bin/bash
# set -e
# 
# echo "🔨 Building HFT Market Maker..."
# 
# # Create build directory
# mkdir -p build
# cd build
# 
# # Configure and build
# cmake ..
# make -j$(nproc)
# 
# cd ..
# echo ""
# echo "✅ Build complete!"
# echo ""
# echo "Quick start:"
# echo "  1. Get FREE API key: https://www.alphavantage.co/support/#api-key"
# echo "  2. Run: ./build/market_maker AAPL YOUR_API_KEY"
# echo ""
# echo "Or test with demo key (limited):"
# echo "  ./build/market_maker"
# 
# ============================================
# setup_and_run.sh - Complete setup script
# Save as: setup_and_run.sh
# ============================================
# #!/bin/bash
# set -e
# 
# echo "╔════════════════════════════════════════╗"
# echo "║   HFT MARKET MAKER - QUICK SETUP     ║"
# echo "╚════════════════════════════════════════╝"
# echo ""
# 
# # Detect OS
# if [[ "$OSTYPE" == "linux-gnu"* ]]; then
#     echo "📦 Installing dependencies (Ubuntu/Debian)..."
#     sudo apt-get update
#     sudo apt-get install -y build-essential cmake libcurl4-openssl-dev libjsoncpp-dev
# elif [[ "$OSTYPE" == "darwin"* ]]; then
#     echo "📦 Installing dependencies (macOS)..."
#     brew install cmake curl jsoncpp
# else
#     echo "⚠️  Unsupported OS. Please install manually:"
#     echo "   - cmake, gcc/g++, libcurl, jsoncpp"
#     exit 1
# fi
# 
# echo ""
# echo "🔨 Building..."
# mkdir -p build
# cd build
# cmake ..
# make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
# cd ..
# 
# echo ""
# echo "✅ Setup complete!"
# echo ""
# echo "════════════════════════════════════════"
# echo "NEXT STEPS:"
# echo "════════════════════════════════════════"
# echo ""
# echo "1. Get FREE API Key (takes 30 seconds):"
# echo "   https://www.alphavantage.co/support/#api-key"
# echo ""
# echo "2. Run with your key:"
# echo "   ./build/market_maker AAPL YOUR_API_KEY"
# echo ""
# echo "3. Or try demo mode (limited to 25 calls/day):"
# echo "   ./build/market_maker"
# echo ""
# echo "4. Try different stocks:"
# echo "   ./build/market_maker TSLA YOUR_API_KEY"
# echo "   ./build/market_maker MSFT YOUR_API_KEY"
# echo "   ./build/market_maker NVDA YOUR_API_KEY"
# echo ""
# echo "════════════════════════════════════════"

# ============================================
# test_api.sh - Test Alpha Vantage connection
# Save as: test_api.sh
# ============================================
# #!/bin/bash
# 
# API_KEY="${1:-demo}"
# SYMBOL="${2:-AAPL}"
# 
# echo "Testing Alpha Vantage API..."
# echo "Symbol: $SYMBOL"
# echo "API Key: $API_KEY"
# echo ""
# 
# URL="https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=$SYMBOL&apikey=$API_KEY"
# 
# echo "Request:"
# echo "$URL"
# echo ""
# 
# echo "Response:"
# curl -s "$URL" | jq .
# 
# echo ""
# echo "If you see price data above, API is working! ✅"
# echo "If you see 'Note' about rate limit, get a free key at alphavantage.co"