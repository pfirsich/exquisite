#!/bin/bash
# *sigh* https://github.com/moby/moby/issues/25450 it seems there is a race condition in docker
# where the stdout is not a tty YET when the docker command is run.
# I could change exquisite to wait and retry or I simply sleep a bit here.
docker run -v "$(pwd):/pwd" -w "/pwd" -it exq bash -c "sleep 0.05; /root/exquisite"
