#!/bin/bash

# Script: run_proof_droid.sh
# Description: Runs proof_droid on set1.thm through set47.thm in non-interactive mode.
#              Halts on first failure with an informative message.

# Function to display usage instructions
usage() {
    echo "Usage: $0"
    echo "Ensure that proof_droid executable and set1.thm through set47.thm are present in the current directory."
    exit 1
}

# Optional: Uncomment the following line to enable strict error handling
# set -e

# Verify that proof_droid executable exists
if ! [ -x "./proof_droid" ]; then
    echo "Error: proof_droid executable not found in the current directory."
    echo "Please ensure that proof_droid is compiled and present."
    exit 1
fi

# Loop through set1.thm to set47.thm
for i in {1..47}; do
    filename="set${i}.thm"

    # Check if the file exists
    if ! [ -f "$filename" ]; then
        echo "Error: File '$filename' does not exist."
        echo "Halting the script."
        exit 1
    fi

    echo "Running proof_droid on '$filename'..."

    # Execute proof_droid in non-interactive mode
    ./proof_droid "$filename"
    exit_status=$?

    # Check the exit status
    if [ $exit_status -ne 0 ]; then
        echo "Failure: proof_droid failed on '$filename' with exit code $exit_status."
        echo "Halting further executions."
        exit $exit_status
    else
        echo "Success: '$filename' processed successfully."
        echo "-------------------------------------------"
    fi
done

echo "All proof_droid runs completed successfully."
exit 0

