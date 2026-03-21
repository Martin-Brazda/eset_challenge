#!/bin/bash

# benchmark.sh - Comprehensive performance testing for ESET File Searcher

# Configuration
NEEDLE="FIND_ME_NOW"
TARGET="bench_data"
GEN_SCRIPT="/tmp/gen_script.py"

echo "===================================================="
echo "   ESET CHALLENGE PERFORMANCE BENCHMARK"
echo "===================================================="

if [ ! -d "$TARGET" ]; then
    echo "[!] Benchmarking data missing. Generating ~250MB test set..."
    cat << 'EOF' > "$GEN_SCRIPT"
import os, random, string
def generate_garbage(size):
    return ''.join(random.choices(string.ascii_letters + string.digits + " \n\t", k=size)).encode()
def create_bench_data(base_path, num_small=100, num_large=5, large_size_mb=50):
    if not os.path.exists(base_path): os.makedirs(base_path)
    needle = b"FIND_ME_NOW"
    for i in range(num_small):
        with open(os.path.join(base_path, f"small_{i}.txt"), "wb") as f:
            f.write(generate_garbage(random.randint(100, 10000)))
            if i == 42: f.write(b"\nContext in front " + needle + b" context after\n")
    for i in range(num_large):
        file_path = os.path.join(base_path, f"large_{i}.bin")
        with open(file_path, "wb") as f:
            for _ in range(large_size_mb): f.write(generate_garbage(1024 * 1024))
            if i == 0:
                f.seek(random.randint(0, large_size_mb * 1024 * 1024))
                f.write(b"ABC" + needle + b"XYZ")
    nested = os.path.join(base_path, "deep", "nested", "folder")
    os.makedirs(nested)
    with open(os.path.join(nested, "hidden.txt"), "wb") as f: f.write(b"Deeply nested " + needle + b" success!")
if __name__ == "__main__": create_bench_data("bench_data")
EOF
    python3 "$GEN_SCRIPT"
    rm "$GEN_SCRIPT"
    echo "[+] Data generation complete."
fi

echo "[*] Compiling application..."
make > /dev/null

echo "----------------------------------------------------"
printf "%-12s | %-12s | %-10s\n" "Threads" "Buffer" "Time"
echo "----------------------------------------------------"

for threads in 1 2 4 8 12; do
    for buf in 4096 65536 1048576; do
        sed -i "s/NUM_THREADS=.*/NUM_THREADS=$threads/" .env
        sed -i "s/BUFFER_SIZE=.*/BUFFER_SIZE=$buf/" .env
        
        printf "%-12d | %-12s | " "$threads" "$(($buf/1024))KB"
        
        { time ./main "$TARGET" "$NEEDLE" > /dev/null 2>&1; } 2>&1 | grep real | awk '{print $2}'
    done
    echo "----------------------------------------------------"
done

sed -i "s/NUM_THREADS=.*/NUM_THREADS=8/" .env
sed -i "s/BUFFER_SIZE=.*/BUFFER_SIZE=65536/" .env
echo "[+] Done. Best results usually occur with Threads matching CPU cores and 64KB-1MB buffers."
