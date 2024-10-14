docker build -t pax-buildenv .
docker run --rm -u "$(id -u):$(id -g)" -v ${PWD}:/app pax-buildenv -c "cd /app && ./build.sh"