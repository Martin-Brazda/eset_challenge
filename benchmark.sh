#!/bin/bash

# benchmark.sh - Comprehensive performance testing for ESET File Searcher

# Configuration
NEEDLE="FIND_ME_NOW"
TARGET="bench_data"
GEN_SCRIPT="/tmp/gen_script.py"
DATA_SIZE=${1:-250}  # default 250MB

case "$DATA_SIZE" in
    (small) NUM_SMALL=50; NUM_LARGE=2; LARGE_MB=10;;
    (medium) NUM_SMALL=100; NUM_LARGE=5; LARGE_MB=50;;
    (large) NUM_SMALL=200; NUM_LARGE=10; LARGE_MB=200;;
esac

echo "===================================================="
echo "              PERFORMANCE BENCHMARK"
echo "===================================================="

if [ ! -d "$TARGET" ]; then
    echo "[!] Benchmarking data missing. Generating ~${DATA_SIZE}MB test set..."
    cat << 'EOF' > "$GEN_SCRIPT"
import os, random, string
def generate_garbage(size):
    return ''.join(random.choices(string.ascii_letters + string.digits + " \n\t", k=size)).encode()
def create_bench_data(base_path, num_small=100, num_large=5, large_size_mb=int($DATA_SIZE/5)):
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

LOW_TIME=0.0
OPTIMAL_THREADS=1
OPTIMAL_BUFFER=4096

thread_range=$(seq 2 2 $(nproc))
buffer_range=(4096 8192 16384 32768 65536 131072 262144 524288 1048576)

for threads in $thread_range; do
    for buf in "${buffer_range[@]}"; do
        sed -i "s/NUM_THREADS=.*/NUM_THREADS=$threads/" .env
        sed -i "s/BUFFER_SIZE=.*/BUFFER_SIZE=$buf/" .env
        
        printf "%-12d | %-12s | " "$threads" "$(($buf/1024))KB"
        
        TIME=$( { time ./main "$TARGET" "$NEEDLE" > /dev/null 2>&1; } 2>&1 | grep real | awk -F'm|s' '{print $1*60 + $2}' )
        
        if (( $(echo "$LOW_TIME == 0" | bc -l) )); then
            LOW_TIME=$TIME
        else
            if (( $(echo "$TIME < $LOW_TIME" | bc -l) )); then
                LOW_TIME=$TIME
                OPTIMAL_THREADS=$threads
                OPTIMAL_BUFFER=$buf
            fi
        fi
        printf "%s\n" "$TIME"
    done
    echo "----------------------------------------------------"
done

echo "[*] Starting adaptive fine sweep around best results..."

# best-1 to best+1, bounded by 1 and nproc
thread_fine_range=()
for t in $((OPTIMAL_THREADS-1)) $OPTIMAL_THREADS $((OPTIMAL_THREADS+1)); do
    if [ $t -ge 1 ] && [ $t -le $(nproc) ]; then
        thread_fine_range+=($t)
    fi
done

buffer_fine_range=()
for expr in "$OPTIMAL_BUFFER / 2" "$OPTIMAL_BUFFER" "$OPTIMAL_BUFFER * 2"; do
    b=$(echo "$expr" | bc)
    if [ $b -ge 4096 ] && [ $b -le 1048576 ]; then
        buffer_fine_range+=($b)
    fi
done

for threads in "${thread_fine_range[@]}"; do
    for buf in "${buffer_fine_range[@]}"; do
        sed -i "s/NUM_THREADS=.*/NUM_THREADS=$threads/" .env
        sed -i "s/BUFFER_SIZE=.*/BUFFER_SIZE=$buf/" .env

        printf "%-12d | %-12s | " "$threads" "$(($buf/1024))KB"

        TIME=$( { time ./main "$TARGET" "$NEEDLE" > /dev/null 2>&1; } 2>&1 | grep real | awk -F'm|s' '{print $1*60 + $2}' )

        if (( $(echo "$TIME < $LOW_TIME" | bc -l) )); then
            LOW_TIME=$TIME
            OPTIMAL_THREADS=$threads
            OPTIMAL_BUFFER=$buf
        fi

        printf "%s\n" "$TIME"
    done
done

echo "[+] Adaptive sweep complete."

printf "Best time: %s\n" "$LOW_TIME"
printf "Optimal threads: %d\n" "$OPTIMAL_THREADS"
printf "Optimal buffer: %dKB\n" "$(($OPTIMAL_BUFFER/1024))"

sed -i "s/NUM_THREADS=.*/NUM_THREADS=$OPTIMAL_THREADS/" .env
sed -i "s/BUFFER_SIZE=.*/BUFFER_SIZE=$OPTIMAL_BUFFER/" .env
echo "[+] Done. Best results usually occur with Threads matching CPU cores and 64KB-1MB buffers."
echo "[+] Dont set thread count too high for small dirs since it adds overhead."
