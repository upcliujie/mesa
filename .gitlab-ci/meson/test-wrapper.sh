#!/bin/sh

if test -f /usr/bin/time; then
    exec /usr/bin/time -v "$@"
fi

exec "$@"
