#!/usr/bin/python3

import requests
import time
import random

# 설정
PROXY_ADDR = "http://localhost:49877"       # 프록시 서버 주소
TINY_SERVER_ADDR = "http://localhost:49876"  # Tiny Web Server 주소
TEST_FILES = [f"a{num:02d}.smi" for num in range(1, 100)]+[f"t{num:02d}.smi" for num in range(1, 100)]  # a01.smi ~ t99.smi
REQUESTS_PER_ITER = 50
NUM_ITERATIONS = 10

def fetch_via_proxy(filename):
    url = f"{TINY_SERVER_ADDR}/{filename}"
    proxies = {"http": PROXY_ADDR, "https": PROXY_ADDR}

    start = time.perf_counter()
    try:
        response = requests.get(url, proxies=proxies)
        elapsed = time.perf_counter() - start
        status = response.status_code
    except Exception as e:
        elapsed = -1
        status = f"ERR({e})"
    return elapsed, status

def run_random_requests():
    print(f"{'Iteration':<10} {'Time (s)':>10}")
    overall_start = time.perf_counter()

    for it in range(1, NUM_ITERATIONS + 1):
        iter_start = time.perf_counter()
        for _ in range(REQUESTS_PER_ITER):
            fname = random.choice(TEST_FILES)
            elapsed, status = fetch_via_proxy(fname)
            if status == 404:
                # 404는 기록하지 않음
                continue
        iter_time = time.perf_counter() - iter_start
        print(f"{it:<10} {iter_time:>10.5f}")

    total_time = time.perf_counter() - overall_start
    print(f"\n{'Total':<10} {total_time:>10.5f}")

if __name__ == "__main__":
    run_random_requests()


