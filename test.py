import time
import subprocess
import sys
import re

def run_timed(cmd: list[str], runs: int = 100):
    real_times = []

    for i in range(runs):
        proc = subprocess.run(
            ["/usr/bin/time", "-p"] + cmd,
            stderr=subprocess.PIPE,
            stdout=subprocess.DEVNULL,
            text=True
        )
        match = re.search(r"real\s+([\d.]+)", proc.stderr)
        if match:
            elapsed = float(match.group(1))
            real_times.append(elapsed)
            print(f"Run {i+1}: {elapsed:.3f}s")
        else:
            print(f"Run {i+1}: could not parse time from: {proc.stderr.strip()}")

    if real_times:
        avg = sum(real_times) / len(real_times)
        mini = min(real_times)
        maxi = max(real_times)
        print(f"\n--- Results over {len(real_times)} runs ---")
        print(f"Average: {avg:.3f}s")
        print(f"Min:     {mini:.3f}s")
        print(f"Max:     {maxi:.3f}s")

def timed(func):
    def timed_run():
        start = time.time()
        func()
        end = time.time()
        print(end - start)
    return timed_run
    
@timed
def main():
    subprocess.run(["./main", "./bench_data/", "FIND_ME_NOW"])
    

if __name__ == "__main__":
    main()