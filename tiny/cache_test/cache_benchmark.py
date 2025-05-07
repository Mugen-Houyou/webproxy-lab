#!/usr/bin/python3
# -*- coding: utf-8 -*-

import requests
import time

# 설정
PROXY_ADDR = "http://localhost:49877"       # 프록시 서버 주소
TINY_SERVER_ADDR = "http://localhost:49876"  # Tiny Web Server 주소
TEST_FILES = [f"a{str(i).zfill(2)}.smi" for i in range(1, 100)] + \
             [f"t{str(i).zfill(2)}.smi" for i in range(1, 100)]

def fetch_via_proxy(filename):
    url = f"{TINY_SERVER_ADDR}/{filename}"
    proxies = { "http": PROXY_ADDR, "https": PROXY_ADDR }

    start = time.perf_counter()
    try:
        response = requests.get(url, proxies=proxies)
        elapsed = time.perf_counter() - start
        status = response.status_code
    except Exception as e:
        elapsed = -1
        status = f"ERR({e})"
    return elapsed, status

def run_benchmark():
    print(f"{'파일':<12} {'1차요청(캐시없음)':>20} {'2차요청(캐시있음)':>20}")

    for fname in TEST_FILES:
        miss_time, miss_status = fetch_via_proxy(fname)
        hit_time, hit_status = fetch_via_proxy(fname)

        print(f"{fname:<12} {miss_time:>18.5f}s ({miss_status}) {hit_time:>18.5f}s ({hit_status})")

if __name__ == "__main__":
    run_benchmark()

