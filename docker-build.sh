docker build -t pax-buildenv .
docker run --rm -t -u "$(id -u):$(id -g)" -v ${PWD}:/app pax-buildenv -c "cd /app && ./build.sh"