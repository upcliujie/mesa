from multiprocessing import Process, Manager
import subprocess
import os

NUM_THREADS = 49
NUM_ITERATIONS = 100

def f(worker, iteration,return_dict):
    args = ["stdbuf",
            "-oL",
            "crosvm",
            "run",
            "--gpu", "gles=true,backend=virglrenderer,egl=true,surfaceless=true",
            "-m", "4096",
            "-c", "2",
            "--disable-sandbox",
            "--shared-dir", "/:my_root:type=fs:writeback=true:timeout=60:cache=always",
            "--host_ip=192.168.30.1",
            "--netmask=255.255.255.0",
            "--mac", "AA:BB:CC:00:00:12",
            "-p", "root=my_root rw rootfstype=virtiofs ip=192.168.30.2::192.168.30.1:255.255.255.0:crosvm:eth0 init=foo",
            "/lava-files/bzImage"]
    env = os.environ.copy()
    env["NIR_VALIDATE"] = "0"
    env["LIBGL_ALWAYS_SOFTWARE"] = "true"
    env["GALLIUM_DRIVER"] = "llvmpipe"
    env["LD_LIBRARY_PATH"] = "/install/lib/"
    process = subprocess.Popen(executable="stdbuf", args=args, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = process.communicate()
    triggered = False
    for line in out.decode('ascii', 'ignore').splitlines():
        if "timed out waiting for cap set" in line:
            triggered = True
            print(line)
            break
        elif "virtio_gpu_cmd_get_capset_info" in line:
            #print(line)
            time = int(line.split(' ')[-1])
            if worker not in return_dict:
                return_dict[worker] = 0
            return_dict[worker] = max(return_dict[worker], time)

    if triggered:
        print("%d/%d %d/%d Got it!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" % (worker, NUM_THREADS, iteration, NUM_ITERATIONS))

if __name__ == '__main__':
    for n in range(0, NUM_ITERATIONS):
        manager = Manager()
        return_dict = manager.dict()
        workers = []
        for i in range(0, NUM_THREADS):
            p = Process(target=f, args=(i, n, return_dict))
            p.start()
            workers.append(p)
        for worker in workers:
            worker.join()
        max_time = 0
        for time in return_dict.values():
            max_time = max(max_time, time)
        print("Max time in iteration %d: %d" % (n, max_time))
