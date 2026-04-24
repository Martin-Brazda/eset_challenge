#!/usr/bin/env bash

set -euo pipefail

# benchmark.sh - Thread-sweep performance benchmark for file searcher

NEEDLE="FIND_ME_NOW"
TARGET="bench_data"
GEN_SCRIPT="/tmp/eset_bench_gen.py"
DATA_SIZE="${1:-medium}"

case "$DATA_SIZE" in
    small)  NUM_SMALL=50;  NUM_LARGE=2;  LARGE_MB=10 ;;
    medium) NUM_SMALL=100; NUM_LARGE=5;  LARGE_MB=50 ;;
    large)  NUM_SMALL=200; NUM_LARGE=10; LARGE_MB=200 ;;
    *)
        echo "Usage: $0 [small|medium|large]" >&2
        exit 1
        ;;
esac

if [[ ! -f .env ]]; then
    cp .env.example .env
fi

ENV_BACKUP="$(mktemp)"
cp .env "$ENV_BACKUP"
cleanup() {
    cp "$ENV_BACKUP" .env
    rm -f "$ENV_BACKUP"
}
trap cleanup EXIT

echo "===================================================="
echo "              PERFORMANCE BENCHMARK"
echo "===================================================="

if [[ ! -d "$TARGET" ]]; then
    echo "[!] Benchmark data missing. Generating ${DATA_SIZE} dataset..."
    cat > "$GEN_SCRIPT" <<'PY'
import os
import random
import string
import sys

def generate_garbage(size):
    alphabet = string.ascii_letters + string.digits + " \n\t"
    return "".join(random.choices(alphabet, k=size)).encode()

def create_bench_data(base_path, num_small, num_large, large_size_mb):
    os.makedirs(base_path, exist_ok=True)
    needle = b"FIND_ME_NOW"
    for i in range(num_small):
        with open(os.path.join(base_path, f"small_{i}.txt"), "wb") as f:
            f.write(generate_garbage(random.randint(100, 10000)))
            if i == 42:
                f.write(b"\nContext in front " + needle + b" context after\n")

    for i in range(num_large):
        path = os.path.join(base_path, f"large_{i}.bin")
        with open(path, "wb") as f:
            for _ in range(large_size_mb):
                f.write(generate_garbage(1024 * 1024))
            if i == 0:
                f.seek(random.randint(0, large_size_mb * 1024 * 1024))
                f.write(b"ABC" + needle + b"XYZ")

    nested = os.path.join(base_path, "deep", "nested", "folder")
    os.makedirs(nested, exist_ok=True)
    with open(os.path.join(nested, "hidden.txt"), "wb") as f:
        f.write(b"Deeply nested " + needle + b" success!")

if __name__ == "__main__":
    create_bench_data("bench_data", int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]))
PY
    python3 "$GEN_SCRIPT" "$NUM_SMALL" "$NUM_LARGE" "$LARGE_MB"
    rm -f "$GEN_SCRIPT"
    echo "[+] Data generation complete."
fi

echo "[*] Compiling application..."
make >/dev/null

set_env() {
    local key="$1"
    local value="$2"
    if grep -q "^export ${key}=" .env; then
        sed -i "s/^export ${key}=.*/export ${key}=${value}/" .env
    elif grep -q "^${key}=" .env; then
        sed -i "s/^${key}=.*/${key}=${value}/" .env
    else
        printf "export %s=%s\n" "$key" "$value" >> .env
    fi
}

bench_once_us() {
    local raw
    raw=$({ /usr/bin/time -p ./main "$TARGET" "$NEEDLE" >/dev/null; } 2>&1)
    local sec
    sec=$(printf '%s\n' "$raw" | awk '/^real / {print $2}')
    awk -v s="$sec" 'BEGIN { printf "%.0f\n", s*1000000 }'
}

run_case() {
    local threads="$1"
    local chunk_bytes="$2"
    set_env "NUM_THREADS" "$threads"
    set_env "CHUNKING_THRESHOLD" "${chunk_bytes}"
    LAST_US=$(bench_once_us)
    printf "%-9d | %-8s | %.4fs\n" "$threads" "$((chunk_bytes / 1024))KB" "$(awk -v u="$LAST_US" 'BEGIN {print u/1000000}')"
}

echo "--------------------------------------"
printf "%-9s | %-8s | %s\n" "Threads" "Chunk" "Time"

BEST_US=0
BEST_THREADS=1
BEST_CHUNK=65536

THREAD_MAX=$(nproc)
CHUNK_VALUES=(65536 262144 1048576 4194304)

# Note: after a certain point, adding more threads (or changing chunk size) will often stop improving the
# time because of saturating disk I/O and/or CPU. At that stage, small differences between nearby
# settings are frequently dominated by OS scheduling, filesystem cache state, CPU frequency
# scaling, and background activity rather than the algorithm itself.

for threads in $(seq 1 "$THREAD_MAX"); do
    echo "--------------------------------------"
    for chunk in "${CHUNK_VALUES[@]}"; do
        run_case "$threads" "$chunk"
        us="$LAST_US"
        if [[ "$BEST_US" -eq 0 || "$us" -lt "$BEST_US" ]]; then
            BEST_US="$us"
            BEST_THREADS="$threads"
            BEST_CHUNK="$chunk"
        fi
    done
done

set_env "NUM_THREADS" "$BEST_THREADS"
set_env "CHUNKING_THRESHOLD" "$BEST_CHUNK"

echo "--------------------------------------"
printf "Best time: %.6fs\n" "$(awk -v u="$BEST_US" 'BEGIN {print u/1000000}')"
printf "Optimal threads: %d\n" "$BEST_THREADS"
printf "Optimal CHUNKING_THRESHOLD: %dKB\n" "$((BEST_CHUNK / 1024))"
echo "[+] Benchmark complete."
