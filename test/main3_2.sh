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
                        echo "Skipping: n=$np s=$size z=$zeros m=$mult (already completed)"
                        continue
                    fi
                fi
                
                echo "Running: mpiexec -n $np with -s $size -z $zeros -m $mult"
                
                # Run the program and save output
                mpiexec -n "$np" --allow-run-as-root ./bin/main -s "$size" -z "$zeros" -m "$mult" > "$OUTPUT_FILE" 2>&1
                
                # Parse the output file and extract timing data using more robust pattern
                CSR_CONSTRUCTION=$(grep -oP 'CSR construction time: \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                BROADCAST_TIME=$(grep -oP 'Broadcast time \(CSR\): \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                COMPUTE_TIME=$(grep -oP 'Compute time \(CSR\): \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                TOTAL_TIME_CSR=$(grep -oP 'Total time \(CSR\): \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                TOTAL_TIME_DENSE=$(grep -oP 'Total time \(Dense\): \K[0-9.]+' "$OUTPUT_FILE" | tail -1)
                
                # Handle empty values (in case parsing failed)
                CSR_CONSTRUCTION=${CSR_CONSTRUCTION:-"N/A"}
                BROADCAST_TIME=${BROADCAST_TIME:-"N/A"}
                COMPUTE_TIME=${COMPUTE_TIME:-"N/A"}
                TOTAL_TIME_CSR=${TOTAL_TIME_CSR:-"N/A"}
                TOTAL_TIME_DENSE=${TOTAL_TIME_DENSE:-"N/A"}
                
                # Append to CSV
                echo "${np}|${size}|${zeros}|${mult}|${CSR_CONSTRUCTION}|${BROADCAST_TIME}|${COMPUTE_TIME}|${TOTAL_TIME_CSR}|${TOTAL_TIME_DENSE}" >> "$CSV_FILE"
                
                echo "  -> Saved to $OUTPUT_FILE"
                echo "  -> CSR construction: $CSR_CONSTRUCTION, Broadcast: $BROADCAST_TIME, Compute: $COMPUTE_TIME, Total CSR: $TOTAL_TIME_CSR, Total Dense: $TOTAL_TIME_DENSE"
            done
        done
    done
done

echo ""
echo "All tests completed!"
echo "Results saved to: $CSV_FILE"
echo "Output files saved to: result/output/"
