#!/bin/bash

# Change to the 3.1 directory
cd "$(dirname "$0")/../3.1" || exit 1

# Create output directories if they don't exist
mkdir -p result4/output
mkdir -p result4/csv

# Create CSV header if file doesn't exist
CSV_FILE="result4/csv/results.csv"
if [ ! -f "$CSV_FILE" ]; then
    echo "numberofprocesses|gradenumber|Broadcast time|Compute time|Reduce Time|Total time" > "$CSV_FILE"
fi

# Build the project first
make clean
make

# Process counts: 2, 12, 22, 32, 42, 52
PROCESS_COUNTS=(2 12 22 32 42 52)

# Grade numbers
GRADE_NUMBERS=(10 100 1000 10000 100000)

# Maximum number of retry attempts
MAX_RETRIES=3

# Run tests
for np in "${PROCESS_COUNTS[@]}"; do
    for grade in "${GRADE_NUMBERS[@]}"; do
        OUTPUT_FILE="result4/output/n${np}_g${grade}.txt"
        
        # Check if output file already exists and contains valid timing data
        if [ -f "$OUTPUT_FILE" ]; then
            # Check if the file contains "Total time:" which indicates a successful run
            if grep -q "Total time:" "$OUTPUT_FILE"; then
                # Parse to verify all values are valid
                BROADCAST_TIME=$(grep -oP 'Broadcast time: \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                COMPUTE_TIME=$(grep -oP 'Compute time: \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                REDUCE_TIME=$(grep -oP 'Reduce time: \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                TOTAL_TIME=$(grep -oP 'Total time: \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                
                # If all values are present, skip this test
                if [ -n "$BROADCAST_TIME" ] && [ -n "$COMPUTE_TIME" ] && [ -n "$REDUCE_TIME" ] && [ -n "$TOTAL_TIME" ]; then
                    echo "Skipping: n=$np grade=$grade (already completed successfully)"
                    continue
                fi
            fi
        fi
        
        # Run the test with retries
        RETRY_COUNT=0
        SUCCESS=0
        
        while [ $RETRY_COUNT -lt $MAX_RETRIES ] && [ $SUCCESS -eq 0 ]; do
            if [ $RETRY_COUNT -gt 0 ]; then
                echo "  Retry attempt $RETRY_COUNT for: n=$np grade=$grade"
            else
                echo "Running: mpiexec -n $np with grade $grade"
            fi
            
            # Run the program and save output
            mpiexec -n "$np" --mca plm_rsh_agent "ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -x" -hostfile ./src/hosts.txt ./bin/polyno -n "$grade" > "$OUTPUT_FILE" 2>&1
            
            # Parse the output file and extract timing data
            BROADCAST_TIME=$(grep -oP 'Broadcast time: \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
            COMPUTE_TIME=$(grep -oP 'Compute time: \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
            REDUCE_TIME=$(grep -oP 'Reduce time: \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
            TOTAL_TIME=$(grep -oP 'Total time: \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
            
            # Check if all values were successfully parsed
            if [ -n "$BROADCAST_TIME" ] && [ -n "$COMPUTE_TIME" ] && [ -n "$REDUCE_TIME" ] && [ -n "$TOTAL_TIME" ]; then
                SUCCESS=1
                echo "  -> Saved to $OUTPUT_FILE"
                echo "  -> Broadcast: $BROADCAST_TIME, Compute: $COMPUTE_TIME, Reduce: $REDUCE_TIME, Total: $TOTAL_TIME"
                
                # Append to CSV
                echo "${np}|${grade}|${BROADCAST_TIME}|${COMPUTE_TIME}|${REDUCE_TIME}|${TOTAL_TIME}" >> "$CSV_FILE"
            else
                RETRY_COUNT=$((RETRY_COUNT + 1))
                
                if [ $RETRY_COUNT -lt $MAX_RETRIES ]; then
                    echo "  -> Test failed (N/A values detected). Retrying..."
                    sleep 2  # Wait 2 seconds before retry
                else
                    echo "  -> Test failed after $MAX_RETRIES attempts. Saving as N/A."
                    
                    # Handle empty values
                    BROADCAST_TIME=${BROADCAST_TIME:-"N/A"}
                    COMPUTE_TIME=${COMPUTE_TIME:-"N/A"}
                    REDUCE_TIME=${REDUCE_TIME:-"N/A"}
                    TOTAL_TIME=${TOTAL_TIME:-"N/A"}
                    
                    # Append to CSV
                    echo "${np}|${grade}|${BROADCAST_TIME}|${COMPUTE_TIME}|${REDUCE_TIME}|${TOTAL_TIME}" >> "$CSV_FILE"
                    echo "  -> Saved to $OUTPUT_FILE (with N/A values)"
                fi
            fi
        done
    done
done

echo ""
echo "All tests completed!"
echo "Results saved to: $CSV_FILE"
echo "Output files saved to: result3/output/"

