import time

def main(arg):
    start = time.time()
    time.sleep(int(arg) / 1000.0)
    end = time.time()
    return str((end - start) * 1000000)
