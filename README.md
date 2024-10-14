Based on info from https://fy.chalmers.se/subatom/subexp-daq/ and
https://fy.chalmers.se/subatom/subexp-daq/minidaq_v2718_mdpp16.txt

# Build and run

  ```bash
  git submodule update --init --recursive
  ./build-daq0.sh && cd scripts && ./free.bash
  ```

# TODO

- figure out why rpath from mvlcc doesn't stick when building outside of docker.
- how to set shm_size when running in devcontainers? this might make ucesb work inside the container
