#!/bin/sh

echo "My PID: $$"

# String to hold child process IDs
child_pids=""
SLEEP_TIME_SEC=300

# Function to start a child process
start_child() {
    # Example child process (sleeps for a long time)
    sleep $SLEEP_TIME_SEC &
    echo "Child $! sleeping for $SLEEP_TIME_SEC seconds"
    # Append the PID of the child process to the string
    child_pids="$child_pids $!"
    echo "Started child process with PID $!"
}

# Function to be executed when SIGTERM is received
handle_sigterm() {
    echo "SIGTERM received, propagating to child processes..." | tee /tmp/sigterm_test.log

    # Loop over child PIDs and send SIGTERM
    for pid in $child_pids; do
        echo "Sending SIGTERM to child process $pid"
        kill -SIGTERM "$pid" 2>/dev/null
    done

    # Exit the script
    exit 0
}

# Trap SIGTERM
trap 'handle_sigterm' TERM

# Start a few child processes
start_child
start_child
start_child

# Wait for child processes to finish
wait

