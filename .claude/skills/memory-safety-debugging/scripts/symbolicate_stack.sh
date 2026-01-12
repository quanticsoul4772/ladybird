#!/usr/bin/env bash
# Symbolicate sanitizer stack traces using llvm-symbolizer

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Usage
usage() {
    cat <<EOF
Usage: $0 [OPTIONS] BINARY [LOG_FILE]

Symbolicate sanitizer stack traces

OPTIONS:
    -b, --binary PATH      Path to binary (for symbolication)
    -i, --inline           Show inlined functions
    -h, --help             Show this help

ARGUMENTS:
    BINARY                 Binary that produced the log
    LOG_FILE               Sanitizer log file (or stdin if not provided)

EXAMPLES:
    # Symbolicate log file
    $0 Build/sanitizers/bin/Ladybird asan.log

    # Symbolicate from stdin
    cat asan.log | $0 Build/sanitizers/bin/Ladybird

    # With inline functions
    $0 -i Build/sanitizers/bin/Ladybird asan.log

EOF
    exit 1
}

# Default values
SHOW_INLINE=0
BINARY=""
LOG_FILE=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -b|--binary)
            BINARY="$2"
            shift 2
            ;;
        -i|--inline)
            SHOW_INLINE=1
            shift
            ;;
        -h|--help)
            usage
            ;;
        -*)
            echo -e "${RED}Unknown option: $1${NC}"
            usage
            ;;
        *)
            if [[ -z "$BINARY" ]]; then
                BINARY="$1"
            elif [[ -z "$LOG_FILE" ]]; then
                LOG_FILE="$1"
            else
                echo -e "${RED}Too many arguments${NC}"
                usage
            fi
            shift
            ;;
    esac
done

# Check if binary provided
if [[ -z "$BINARY" ]]; then
    echo -e "${RED}Error: No binary specified${NC}"
    usage
fi

# Check if binary exists
if [[ ! -f "$BINARY" ]]; then
    echo -e "${RED}Error: Binary not found: $BINARY${NC}"
    exit 1
fi

# Find symbolizer
SYMBOLIZER=""
for tool in llvm-symbolizer-18 llvm-symbolizer-17 llvm-symbolizer-16 llvm-symbolizer; do
    if command -v "$tool" &> /dev/null; then
        SYMBOLIZER="$tool"
        break
    fi
done

if [[ -z "$SYMBOLIZER" ]]; then
    echo -e "${RED}Error: llvm-symbolizer not found${NC}"
    echo -e "${YELLOW}Install: sudo apt-get install llvm${NC}"
    exit 1
fi

echo -e "${GREEN}Using symbolizer: $SYMBOLIZER${NC}" >&2
echo -e "${GREEN}Binary: $BINARY${NC}" >&2
echo "" >&2

# Symbolization function
symbolicate_address() {
    local addr="$1"
    local opts=()

    if [[ $SHOW_INLINE -eq 1 ]]; then
        opts+=(--inlining=true)
    else
        opts+=(--inlining=false)
    fi

    opts+=(--demangle)
    opts+=(--obj="$BINARY")

    echo "$addr" | "$SYMBOLIZER" "${opts[@]}"
}

# Process input
process_log() {
    local input="$1"

    while IFS= read -r line; do
        # Check if line contains address
        if [[ "$line" =~ \#[0-9]+[[:space:]]+0x[0-9a-f]+ ]]; then
            # Extract address
            addr=$(echo "$line" | grep -oP '0x[0-9a-f]+' | head -1)

            # Try to symbolicate
            if [[ -n "$addr" ]]; then
                symbolicated=$(symbolicate_address "$addr" 2>/dev/null || echo "")

                if [[ -n "$symbolicated" ]]; then
                    # Replace address with symbolicated info
                    echo -e "${YELLOW}$line${NC}"
                    echo -e "${GREEN}  -> $symbolicated${NC}"
                else
                    echo "$line"
                fi
            else
                echo "$line"
            fi
        else
            echo "$line"
        fi
    done < "$input"
}

# Process input from file or stdin
if [[ -n "$LOG_FILE" ]]; then
    if [[ ! -f "$LOG_FILE" ]]; then
        echo -e "${RED}Error: Log file not found: $LOG_FILE${NC}"
        exit 1
    fi
    process_log "$LOG_FILE"
else
    process_log /dev/stdin
fi
