#!/bin/sh

set -ex

_XORG_SCRIPT="/xorg-script"
_FIFO_NAME="/xorg-started"
_PID_FILENAME="/tmp/xorg-script-pid"

start_x() {
    if [ -e "${_PID_FILENAME}" ]; then
        echo "X server is already runnig. Doing nothing ..."
        return
    fi

    cat >"${_XORG_SCRIPT}" <<EOF
mkfifo "$_FIFO_NAME"
echo \$\$ > "$_PID_FILENAME"

while read _SIGNAL; do
    break
done < "$_FIFO_NAME"

rm "$_FIFO_NAME"
rm "$_PID_FILENAME"
rm "$_XORG_SCRIPT"
EOF

    if [ "x$1" != "x" ]; then
       export LD_LIBRARY_PATH="${1}/lib"
       export LIBGL_DRIVERS_PATH="${1}/lib/dri"
    fi
    xinit /bin/sh "${_XORG_SCRIPT}" -- /usr/bin/Xorg vt45 -noreset -dpms -logfile /Xorg.0.log &

    # Wait for xorg to be ready for connections.
    for i in 1 2 3 4 5; do
        if [ -e "${_PID_FILENAME}" ]; then
            cat "${_PID_FILENAME}"
            break
        fi
        sleep 5
    done
}

stop_x() {
    echo "EXIT" > "${_FIFO_NAME}" &

    # Give some time for xorg to shutdown
    for i in 1 2 3 4 5; do
        if [ ! -e "${_PID_FILENAME}" ]; then
            return
        fi
        sleep 5
    done

    kill -9 $(cat "${_PID_FILENAME}")

    rm -f "${_FIFO_NAME}"
    rm -f "${_PID_FILENAME}"
    rm -f "${_XORG_SCRIPT}"
}

case "$1" in
  start)
        echo "Starting X server ..."
        start_x "$2"
        ;;
  stop)
        echo "Stopping X server ..."
        stop_x
        ;;
  *)
        echo "Usage: $0 {start|stop} [<install_path>]" >&2
        exit 1
        ;;
esac

exit 0

