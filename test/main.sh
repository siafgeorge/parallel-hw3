#!/bin/bash

# Change to the 3.1 directory
cd "$(dirname "$0")/../3.1" || exit 1

# Create output directories if they don't exist
mkdir -p result/output
mkdir -p result/csv

# Create CSV header if file doesn't exist
CSV_FILE="result/csv/results.csv"
if [ ! -f "$CSV_FILE" ]; then
    echo "numberofprocesses|gradenumber|Broadcast time|Compute time|Reduce Time|Total time" > "$CSV_FILE"
fi

# Build the project first
make clean
make

# Process counts: 2, 12, 22, 32, 42, 52, 62, 72, 82
PROCESS_COUNTS=(2 12 22 32 42 52 62 72 82)

# Grade numbers
GRADE_NUMBERS=(10 100 1000 10000 100000 1000000)

# Run tests
for np in "${PROCESS_COUNTS[@]}"; do
    for grade in "${GRADE_NUMBERS[@]}"; do
        OUTPUT_FILE="result/output/n${np}_g${grade}.txt"
        
        # Check if output file already exists and contains valid timing data
        if [ -f "$OUTPUT_FILE" ]; then
            # Check if the file contains "Total time:" which indicates a successful run
            if grep -q "Total time:" "$OUTPUT_FILE"; then
                echo "Skipping: mpiexec -n $np with grade $grade (already completed)"
                continue
            fi
        fi
        
        echo "Running: mpiexec -n $np with grade $grade"
        
        # Run the program and save output
        mpiexec -n "$np" --mca plm_rsh_agent "ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -x"  -hostfile ./src/hosts.txt ./bin/polyno -n "$grade" > "$OUTPUT_FILE" 2>&1
        
        # Parse the output file and extract timing data using more robust pattern
        # Extract the number after "time:" pattern
        BROADCAST_TIME=$(grep -oP 'Broadcast time: \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
        COMPUTE_TIME=$(grep -oP 'Compute time: \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
        REDUCE_TIME=$(grep -oP 'Reduce time: \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
        TOTAL_TIME=$(grep -oP 'Total time: \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
        
        # Handle empty values (in case parsing failed)
        BROADCAST_TIME=${BROADCAST_TIME:-"N/A"}
        COMPUTE_TIME=${COMPUTE_TIME:-"N/A"}
        REDUCE_TIME=${REDUCE_TIME:-"N/A"}
        TOTAL_TIME=${TOTAL_TIME:-"N/A"}
        
        # Append to CSV
        echo "${np}|${grade}|${BROADCAST_TIME}|${COMPUTE_TIME}|${REDUCE_TIME}|${TOTAL_TIME}" >> "$CSV_FILE"
        
        echo "  -> Saved to $OUTPUT_FILE"
        echo "  -> Broadcast: $BROADCAST_TIME, Compute: $COMPUTE_TIME, Reduce: $REDUCE_TIME, Total: $TOTAL_TIME"
    done
done

echo ""
echo "All tests completed!"
echo "Results saved to: $CSV_FILE"
echo "Output files saved to: result/output/"