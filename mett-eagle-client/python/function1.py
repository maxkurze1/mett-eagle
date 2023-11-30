import time

def main(arg):
    sleep_time = int(arg) / 1000.0
    start = time.time()
    time.sleep(sleep_time)
    end = time.time()
    return str((end - start) * 1000000)
