#!/usr/bin/env bash
# Monitor fuzzing progress and display statistics

set -euo pipefail

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Default values
FUZZER_TYPE="auto"
FUZZING_DIR=""
REFRESH_INTERVAL=5
ONCE=0
ALERT_ON_CRASH=0
ALERT_COMMAND=""

# Usage
usage() {
    cat <<EOF
Usage: $0 [OPTIONS] FUZZING_DIR

Monitor fuzzing progress and display statistics

OPTIONS:
    -t, --type TYPE         Fuzzer type: libfuzzer, afl, auto (default: auto)
    -r, --refresh SECS      Refresh interval in seconds (default: 5)
    -1, --once              Display stats once and exit
    -a, --alert             Alert on new crashes (beep)
    -c, --command CMD       Run command on new crash (e.g., "notify-send")
    -h, --help              Show this help

ARGUMENTS:
    FUZZING_DIR             Directory containing fuzzing output
                            (AFL: output directory, LibFuzzer: corpus directory)

EXAMPLES:
    # Monitor AFL++ fuzzing
    $0 Fuzzing/Corpus/FuzzIPCMessages-afl-findings/

    # Monitor LibFuzzer corpus (shows growth over time)
    $0 -t libfuzzer Fuzzing/Corpus/FuzzIPCMessages/

    # Show stats once
    $0 -1 Fuzzing/Corpus/FuzzIPCMessages-afl-findings/

    # Alert on crashes
    $0 -a Fuzzing/Corpus/FuzzIPCMessages-afl-findings/

    # Run custom command on crash
    $0 -c "notify-send 'Crash found!'" Fuzzing/Corpus/FuzzIPCMessages-afl-findings/

EOF
    exit 1
}

log_info() {
    echo -e "${BLUE}==>${NC} $1"
}

log_success() {
    echo -e "${GREEN}✓${NC} $1"
}

log_error() {
    echo -e "${RED}✗${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}⚠${NC} $1"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--type)
            FUZZER_TYPE="$2"
            shift 2
            ;;
        -r|--refresh)
            REFRESH_INTERVAL="$2"
            shift 2
            ;;
        -1|--once)
            ONCE=1
            shift
            ;;
        -a|--alert)
            ALERT_ON_CRASH=1
            shift
            ;;
        -c|--command)
            ALERT_COMMAND="$2"
            ALERT_ON_CRASH=1
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        -*)
            log_error "Unknown option: $1"
            usage
            ;;
        *)
            FUZZING_DIR="$1"
            shift
            ;;
    esac
done

# Validate arguments
if [[ -z "$FUZZING_DIR" ]]; then
    log_error "Missing fuzzing directory"
    usage
fi

if [[ ! -d "$FUZZING_DIR" ]]; then
    log_error "Fuzzing directory not found: $FUZZING_DIR"
    exit 1
fi

# Auto-detect fuzzer type
if [[ "$FUZZER_TYPE" == "auto" ]]; then
    if [[ -f "$FUZZING_DIR/fuzzer_stats" ]] || [[ -d "$FUZZING_DIR/default" ]]; then
        FUZZER_TYPE="afl"
    else
        FUZZER_TYPE="libfuzzer"
    fi
fi

FUZZER_TYPE=$(echo "$FUZZER_TYPE" | tr '[:upper:]' '[:lower:]')

# Global variables for crash tracking
LAST_CRASH_COUNT=0

# Function to format time duration
format_duration() {
    local seconds=$1
    local days=$((seconds / 86400))
    local hours=$(((seconds % 86400) / 3600))
    local minutes=$(((seconds % 3600) / 60))
    local secs=$((seconds % 60))

    if [[ $days -gt 0 ]]; then
        printf "%dd %02dh %02dm" $days $hours $minutes
    elif [[ $hours -gt 0 ]]; then
        printf "%dh %02dm %02ds" $hours $minutes $secs
    elif [[ $minutes -gt 0 ]]; then
        printf "%dm %02ds" $minutes $secs
    else
        printf "%ds" $secs
    fi
}

# Function to format number with commas
format_number() {
    printf "%'d" "$1" 2>/dev/null || echo "$1"
}

# Function to monitor AFL++
monitor_afl() {
    # Find AFL stats file
    if [[ -f "$FUZZING_DIR/fuzzer_stats" ]]; then
        STATS_FILE="$FUZZING_DIR/fuzzer_stats"
    elif [[ -f "$FUZZING_DIR/default/fuzzer_stats" ]]; then
        STATS_FILE="$FUZZING_DIR/default/fuzzer_stats"
    else
        log_error "AFL++ stats file not found"
        exit 1
    fi

    # Parse stats file
    declare -A STATS
    while IFS=: read -r key value; do
        STATS["$key"]=$(echo "$value" | xargs)
    done < "$STATS_FILE"

    # Get crash/hang directories
    if [[ -d "$FUZZING_DIR/default/crashes" ]]; then
        CRASHES_DIR="$FUZZING_DIR/default/crashes"
        HANGS_DIR="$FUZZING_DIR/default/hangs"
        QUEUE_DIR="$FUZZING_DIR/default/queue"
    else
        CRASHES_DIR="$FUZZING_DIR/crashes"
        HANGS_DIR="$FUZZING_DIR/hangs"
        QUEUE_DIR="$FUZZING_DIR/queue"
    fi

    # Count crashes and hangs
    CRASH_COUNT=$(find "$CRASHES_DIR" -type f ! -name "README.txt" 2>/dev/null | wc -l)
    HANG_COUNT=$(find "$HANGS_DIR" -type f ! -name "README.txt" 2>/dev/null | wc -l)
    QUEUE_COUNT=$(find "$QUEUE_DIR" -type f ! -name "README.txt" 2>/dev/null | wc -l)

    # Calculate runtime
    START_TIME=${STATS["start_time"]:-0}
    LAST_UPDATE=${STATS["last_update"]:-0}
    RUNTIME=$((LAST_UPDATE - START_TIME))

    # Display stats
    clear
    echo "============================================"
    echo -e "${CYAN}AFL++ Fuzzing Monitor${NC}"
    echo "============================================"
    echo ""
    echo -e "${BLUE}Campaign Information:${NC}"
    echo "  Fuzzer:          ${STATS["afl_banner"]:-N/A}"
    echo "  Start time:      $(date -d "@$START_TIME" 2>/dev/null || echo "N/A")"
    echo "  Runtime:         $(format_duration $RUNTIME)"
    echo "  Last update:     $(date -d "@$LAST_UPDATE" 2>/dev/null || echo "N/A")"
    echo ""

    echo -e "${BLUE}Execution Statistics:${NC}"
    echo "  Exec speed:      $(format_number ${STATS["execs_per_sec"]:-0}) exec/s"
    echo "  Total execs:     $(format_number ${STATS["execs_done"]:-0})"
    echo "  Cycles done:     ${STATS["cycles_done"]:-0}"
    echo ""

    echo -e "${BLUE}Coverage Information:${NC}"
    echo "  Coverage:        ${STATS["bitmap_cvg"]:-0}"
    echo "  Corpus count:    $(format_number $QUEUE_COUNT)"
    echo "  Pending favs:    ${STATS["pending_favs"]:-0}"
    echo "  Pending total:   ${STATS["pending_total"]:-0}"
    echo ""

    echo -e "${BLUE}Findings:${NC}"
    if [[ $CRASH_COUNT -gt 0 ]] || [[ $HANG_COUNT -gt 0 ]]; then
        [[ $CRASH_COUNT -gt 0 ]] && echo -e "  ${RED}Crashes:         $CRASH_COUNT${NC}" || echo "  Crashes:         0"
        [[ $HANG_COUNT -gt 0 ]] && echo -e "  ${YELLOW}Hangs:           $HANG_COUNT${NC}" || echo "  Hangs:           0"
    else
        echo -e "  ${GREEN}Crashes:         0${NC}"
        echo "  Hangs:           0"
    fi
    echo "  Unique crashes:  ${STATS["unique_crashes"]:-0}"
    echo "  Unique hangs:    ${STATS["unique_hangs"]:-0}"
    echo ""

    echo -e "${BLUE}Stage Progress:${NC}"
    echo "  Current stage:   ${STATS["cur_stage"]:-N/A}"
    echo "  Stage progress:  ${STATS["stage_cur"]:-0}/${STATS["stage_max"]:-0}"
    echo ""

    # Alert on new crashes
    if [[ $ALERT_ON_CRASH -eq 1 ]] && [[ $CRASH_COUNT -gt $LAST_CRASH_COUNT ]]; then
        echo -e "${RED}🚨 NEW CRASH DETECTED! 🚨${NC}" $'\a'
        [[ -n "$ALERT_COMMAND" ]] && eval "$ALERT_COMMAND" &
    fi
    LAST_CRASH_COUNT=$CRASH_COUNT

    echo "Press Ctrl+C to exit"
}

# Function to monitor LibFuzzer
monitor_libfuzzer() {
    # Count corpus files
    CORPUS_COUNT=$(find "$FUZZING_DIR" -type f ! -name "*.log" ! -name "README.txt" 2>/dev/null | wc -l)
    CORPUS_SIZE=$(du -sh "$FUZZING_DIR" 2>/dev/null | cut -f1)

    # Look for recent crashes
    CRASH_COUNT=$(find . -maxdepth 1 -name "crash-*" -o -name "timeout-*" 2>/dev/null | wc -l)

    # Try to find recent fuzzing log
    LOG_FILE=$(ls -t fuzzing_*_*.log 2>/dev/null | head -1)

    # Display stats
    clear
    echo "============================================"
    echo -e "${CYAN}LibFuzzer Corpus Monitor${NC}"
    echo "============================================"
    echo ""
    echo -e "${BLUE}Corpus Information:${NC}"
    echo "  Directory:       $FUZZING_DIR"
    echo "  Corpus count:    $(format_number $CORPUS_COUNT)"
    echo "  Corpus size:     $CORPUS_SIZE"
    echo "  Last updated:    $(date)"
    echo ""

    if [[ -n "$LOG_FILE" ]]; then
        echo -e "${BLUE}Recent Activity (from $LOG_FILE):${NC}"

        # Parse last stats from log
        if grep -q "#.*cov:" "$LOG_FILE" 2>/dev/null; then
            echo "  Last stats:"
            tail -20 "$LOG_FILE" | grep "#.*cov:" | tail -1 | sed 's/^/    /'
        fi

        # Count total runs
        TOTAL_RUNS=$(grep -c "^#" "$LOG_FILE" 2>/dev/null || echo "N/A")
        echo "  Total runs:      $TOTAL_RUNS"
        echo ""
    fi

    echo -e "${BLUE}Findings:${NC}"
    if [[ $CRASH_COUNT -gt 0 ]]; then
        echo -e "  ${RED}Crashes/Timeouts: $CRASH_COUNT${NC}"
        echo ""
        echo "  Recent crashes:"
        find . -maxdepth 1 \( -name "crash-*" -o -name "timeout-*" \) -printf "    %f\n" 2>/dev/null | head -10
    else
        echo -e "  ${GREEN}Crashes:          0${NC}"
    fi
    echo ""

    # File size distribution
    echo -e "${BLUE}Corpus Size Distribution:${NC}"
    find "$FUZZING_DIR" -type f ! -name "*.log" -exec wc -c {} \; 2>/dev/null | \
        awk '{
            if ($1 < 100) small++;
            else if ($1 < 1024) medium++;
            else if ($1 < 10240) large++;
            else huge++;
            total++;
        }
        END {
            printf "  < 100 bytes:     %d (%.1f%%)\n", small, small*100.0/total;
            printf "  100-1KB:         %d (%.1f%%)\n", medium, medium*100.0/total;
            printf "  1-10KB:          %d (%.1f%%)\n", large, large*100.0/total;
            printf "  > 10KB:          %d (%.1f%%)\n", huge, huge*100.0/total;
        }' 2>/dev/null || echo "  N/A"

    echo ""

    # Alert on new crashes
    if [[ $ALERT_ON_CRASH -eq 1 ]] && [[ $CRASH_COUNT -gt $LAST_CRASH_COUNT ]]; then
        echo -e "${RED}🚨 NEW CRASH DETECTED! 🚨${NC}" $'\a'
        [[ -n "$ALERT_COMMAND" ]] && eval "$ALERT_COMMAND" &
    fi
    LAST_CRASH_COUNT=$CRASH_COUNT

    echo "Press Ctrl+C to exit"
}

# Main monitoring loop
echo "Starting fuzzing monitor..."
echo "Fuzzer type: $FUZZER_TYPE"
echo "Refresh interval: ${REFRESH_INTERVAL}s"
echo ""

if [[ $ONCE -eq 1 ]]; then
    # Display once and exit
    case "$FUZZER_TYPE" in
        afl|afl++)
            monitor_afl
            ;;
        libfuzzer|lf)
            monitor_libfuzzer
            ;;
        *)
            log_error "Unknown fuzzer type: $FUZZER_TYPE"
            exit 1
            ;;
    esac
else
    # Continuous monitoring
    while true; do
        case "$FUZZER_TYPE" in
            afl|afl++)
                monitor_afl
                ;;
            libfuzzer|lf)
                monitor_libfuzzer
                ;;
            *)
                log_error "Unknown fuzzer type: $FUZZER_TYPE"
                exit 1
                ;;
        esac

        sleep "$REFRESH_INTERVAL"
    done
fi
