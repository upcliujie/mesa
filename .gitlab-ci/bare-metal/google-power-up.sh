#!/bin/bash

relay=$1

if [ -z "$relay" ]; then
    echo "Must supply a relay arg"
    exit 1
fi

/install/bare-metal/google-power-relay.py off $relay
sleep 5
/install/bare-metal/google-power-relay.py on $relay
