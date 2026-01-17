#!/bin/bash
# Change to the 3.2 directory
cd "$(dirname "$0")/../3.2" || exit 1

# Create output directories if they don't exist
mkdir -p result/output
mkdir -p result/csv

# Create CSV header if file doesn't exist
CSV_FILE="result/csv/results.csv"
if [ ! -f "$CSV_FILE" ]; then
    echo "numberofprocesses|size|zeros|multiplications|CSR construction|Broadcast time|Compute time (CSR)|Total time (CSR)|Total time (Dense)" > "$CSV_FILE"
fi

# Build the project first
make clean
make

# Process counts: 2, 12, 22, 32, 42, 52
PROCESS_COUNTS=(2 12 22 32 42 52)
# Matrix sizes
SIZES=(10 100 1000 10000)
# Zero percentages
ZEROS=(20 40 60 80)
# Multiplications
MULTIPLICATIONS=(5 10 15)

# Maximum number of retry attempts
MAX_RETRIES=3

# Run tests
for np in "${PROCESS_COUNTS[@]}"; do
    for size in "${SIZES[@]}"; do
        for zeros in "${ZEROS[@]}"; do
            for mult in "${MULTIPLICATIONS[@]}"; do
                OUTPUT_FILE="result/output/n${np}_s${size}_z${zeros}_m${mult}.txt"
                
                # Check if output file already exists and contains valid timing data
                if [ -f "$OUTPUT_FILE" ]; then
                    # Check if the file contains "Total time (Dense):" which indicates a successful run
                    if grep -q "Total time (Dense):" "$OUTPUT_FILE"; then
                        # Parse to verify all values are valid
                        CSR_CONSTRUCTION=$(grep -oP 'CSR construction time: \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                        BROADCAST_TIME=$(grep -oP 'Broadcast time \(CSR\): \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                        COMPUTE_TIME=$(grep -oP 'Compute time \(CSR\): \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                        TOTAL_TIME_CSR=$(grep -oP 'Total time \(CSR\): \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                        TOTAL_TIME_DENSE=$(grep -oP 'Total time \(Dense\): \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                        
                        # If all values are present, skip this test
                        if [ -n "$CSR_CONSTRUCTION" ] && [ -n "$BROADCAST_TIME" ] && [ -n "$COMPUTE_TIME" ] && [ -n "$TOTAL_TIME_CSR" ] && [ -n "$TOTAL_TIME_DENSE" ]; then
                            echo "Skipping: n=$np s=$size z=$zeros m=$mult (already completed successfully)"
                            continue
                        fi
                    fi
                fi
                
                # Run the test with retries
                RETRY_COUNT=0
                SUCCESS=0
                
                while [ $RETRY_COUNT -lt $MAX_RETRIES ] && [ $SUCCESS -eq 0 ]; do
                    if [ $RETRY_COUNT -gt 0 ]; then
                        echo "  Retry attempt $RETRY_COUNT for: n=$np s=$size z=$zeros m=$mult"
                    else
                        echo "Running: mpiexec -n $np with -s $size -z $zeros -m $mult"
                    fi
                    
                    # Run the program and save output
                    mpiexec -n "$np" --mca plm_rsh_agent "ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -x" -hostfile ./src/hosts.txt ./bin/main -s "$size" -z "$zeros" -m "$mult" > "$OUTPUT_FILE" 2>&1
                    
                    # Parse the output file and extract timing data
                    CSR_CONSTRUCTION=$(grep -oP 'CSR construction time: \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                    BROADCAST_TIME=$(grep -oP 'Broadcast time \(CSR\): \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                    COMPUTE_TIME=$(grep -oP 'Compute time \(CSR\): \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                    TOTAL_TIME_CSR=$(grep -oP 'Total time \(CSR\): \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                    TOTAL_TIME_DENSE=$(grep -oP 'Total time \(Dense\): \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                    
                    # Check if all values were successfully parsed
                    if [ -n "$CSR_CONSTRUCTION" ] && [ -n "$BROADCAST_TIME" ] && [ -n "$COMPUTE_TIME" ] && [ -n "$TOTAL_TIME_CSR" ] && [ -n "$TOTAL_TIME_DENSE" ]; then
                        SUCCESS=1
                        echo "  -> Saved to $OUTPUT_FILE"
                        echo "  -> CSR construction: $CSR_CONSTRUCTION, Broadcast: $BROADCAST_TIME, Compute: $COMPUTE_TIME, Total CSR: $TOTAL_TIME_CSR, Total Dense: $TOTAL_TIME_DENSE"
                        
                        # Append to CSV
                        echo "${np}|${size}|${zeros}|${mult}|${CSR_CONSTRUCTION}|${BROADCAST_TIME}|${COMPUTE_TIME}|${TOTAL_TIME_CSR}|${TOTAL_TIME_DENSE}" >> "$CSV_FILE"
                    else
                        RETRY_COUNT=$((RETRY_COUNT + 1))
                        
                        if [ $RETRY_COUNT -lt $MAX_RETRIES ]; then
                            echo "  -> Test failed (N/A values detected). Retrying..."
                            sleep 2  # Wait 2 seconds before retry
                        else
                            echo "  -> Test failed after $MAX_RETRIES attempts. Saving as N/A."
                            
                            # Handle empty values
                            CSR_CONSTRUCTION=${CSR_CONSTRUCTION:-"N/A"}
                            BROADCAST_TIME=${BROADCAST_TIME:-"N/A"}
                            COMPUTE_TIME=${COMPUTE_TIME:-"N/A"}
                            TOTAL_TIME_CSR=${TOTAL_TIME_CSR:-"N/A"}
                            TOTAL_TIME_DENSE=${TOTAL_TIME_DENSE:-"N/A"}
                            
                            # Append to CSV
                            echo "${np}|${size}|${zeros}|${mult}|${CSR_CONSTRUCTION}|${BROADCAST_TIME}|${COMPUTE_TIME}|${TOTAL_TIME_CSR}|${TOTAL_TIME_DENSE}" >> "$CSV_FILE"
                            echo "  -> Saved to $OUTPUT_FILE (with N/A values)"
                        fi
                    fi
                done
            done
        done
    done
done

echo ""
echo "All tests completed!"
echo "Results saved to: $CSV_FILE"
echo "Output files saved to: result/output/"
