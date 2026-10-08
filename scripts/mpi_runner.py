"""Launch local MPI checks with deadlines and cleanup of their entire process group."""
import os
import shlex
import signal
import subprocess


def launch(ranks, executable, arguments=(), timeout=60):
    command = (shlex.split(os.environ.get("MPIEXEC", "mpiexec"))
               + shlex.split(os.environ.get("MPIEXEC_FLAGS", ""))
               + ["-n", str(ranks), str(executable), *map(str, arguments)])
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               text=True, start_new_session=True)
    try:
        stdout, stderr = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired as error:
        # Kill only this launch's private group, including MPI workers/proxies.
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.communicate()
        raise RuntimeError(f"MPI check exceeded {timeout}s at {ranks} ranks: {executable}") from error
    return subprocess.CompletedProcess(command, process.returncode, stdout, stderr)
