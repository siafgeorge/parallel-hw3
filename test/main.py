#!/usr/bin/env python3
"""
Test suite for polynomial multiplication using MPI.
Tests both serial (single process with -s flag) and parallel (max processes) execution.
Outputs results to CSV files in src/result/
"""

import subprocess
import os
import sys
import time
import csv
from datetime import datetime

# Path to the polynomial executable
POLYNO_PATH = os.path.join(os.path.dirname(__file__), "..", "3.1", "bin", "polyno")

# Path for results
RESULT_DIR = os.path.join(os.path.dirname(__file__), "..", "3.1", "result", "csv")
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "..", "3.1", "result", "output")

def ensure_result_dir():
    """Ensure the result directories exist."""
    os.makedirs(RESULT_DIR, exist_ok=True)
    os.makedirs(OUTPUT_DIR, exist_ok=True)

def get_max_processes():
    """Get the maximum number of MPI processes the system can support."""
    try:
        result = subprocess.run(["nproc"], capture_output=True, text=True)
        return int(result.stdout.strip())
    except Exception as e:
        print(f"Error getting max processes: {e}")
        return 4  # Default fallback

def run_polynomial_test(num_processes, degree, serial_flag=False):
    """
    Run the polynomial multiplication program with specified parameters.
    
    Args:
        num_processes: Number of MPI processes to use
        degree: Degree of the polynomial
        serial_flag: Whether to use the -s flag (only for single process)
    
    Returns:
        tuple: (success, output, execution_time)
    """
    ensure_result_dir()  # Ensure output directory exists
    
    cmd = [
        "mpiexec",
        "--allow-run-as-root",
        "-n", str(num_processes),
        POLYNO_PATH,
        "-n", str(degree),
    ]
    
    # Add -s flag only when processes = 1
    # if serial_flag and num_processes == 1:
    #     cmd.extend(["-s", "1"])
    # else:
    #     cmd.extend(["-s", "0"])
    
    print(f"\n{'='*60}")
    print(f"Running test: processes={num_processes}, degree={degree}, serial={serial_flag}")
    print(f"Command: {' '.join(cmd)}")
    print(f"{'='*60}")
    
    start_time = time.time()
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=300  # 5 minute timeout for large polynomials
        )
        execution_time = time.time() - start_time
        
        print(f"STDOUT:\n{result.stdout}")
        if result.stderr:
            print(f"STDERR:\n{result.stderr}")
        print(f"Return code: {result.returncode}")
        print(f"Execution time: {execution_time:.4f} seconds")
        
        # Save stdout to output file
        test_type = "serial" if serial_flag else "parallel"
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        output_filename = f"{test_type}_degree{degree}_procs{num_processes}_{timestamp}.txt"
        output_filepath = os.path.join(OUTPUT_DIR, output_filename)
        
        with open(output_filepath, 'w') as f:
            f.write(f"Command: {' '.join(cmd)}\n")
            f.write(f"Degree: {degree}\n")
            f.write(f"Processes: {num_processes}\n")
            f.write(f"Serial Flag: {serial_flag}\n")
            f.write(f"Return Code: {result.returncode}\n")
            f.write(f"Execution Time: {execution_time:.6f} seconds\n")
            f.write(f"\n{'='*60}\nSTDOUT:\n{'='*60}\n")
            f.write(result.stdout)
            if result.stderr:
                f.write(f"\n{'='*60}\nSTDERR:\n{'='*60}\n")
                f.write(result.stderr)
        
        print(f"Output saved to: {output_filepath}")
        
        return result.returncode == 0, result.stdout, execution_time
        
    except subprocess.TimeoutExpired:
        print(f"ERROR: Test timed out after 300 seconds")
        return False, "", -1
    except Exception as e:
        print(f"ERROR: {e}")
        return False, "", -1

def save_results_to_csv(results, max_processes):
    """Save test results to CSV files."""
    ensure_result_dir()
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    
    # Save summary results
    summary_file = os.path.join(RESULT_DIR, f"test_summary_{timestamp}.csv")
    with open(summary_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Degree', 'Serial_Status', 'Serial_Time_Sec', 'Parallel_Status', 
                        'Num_Processes', 'Parallel_Time_Sec', 'Speedup'])
        
        for r in results:
            serial_status = "PASS" if r['serial_success'] else "FAIL"
            parallel_status = "PASS" if r['parallel_success'] else "FAIL"
            serial_time = r['serial_time'] if r['serial_time'] >= 0 else -1
            parallel_time = r['parallel_time'] if r['parallel_time'] >= 0 else -1
            speedup = r['speedup'] if r['speedup'] > 0 else 0
            num_procs = r.get('num_processes', max_processes)
            
            writer.writerow([r['degree'], serial_status, f"{serial_time:.6f}", 
                           parallel_status, num_procs, f"{parallel_time:.6f}", f"{speedup:.4f}"])
    
    print(f"\nResults saved to: {summary_file}")
    
    # Save serial results
    serial_file = os.path.join(RESULT_DIR, f"serial_results_{timestamp}.csv")
    with open(serial_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Degree', 'Status', 'Time_Sec', 'Num_Processes'])
        
        # Only write unique degree entries for serial (since serial is the same for all process counts)
        seen_degrees = set()
        for r in results:
            if r['degree'] not in seen_degrees:
                seen_degrees.add(r['degree'])
                status = "PASS" if r['serial_success'] else "FAIL"
                time_val = r['serial_time'] if r['serial_time'] >= 0 else -1
                writer.writerow([r['degree'], status, f"{time_val:.6f}", 1])
    
    print(f"Serial results saved to: {serial_file}")
    
    # Save parallel results
    parallel_file = os.path.join(RESULT_DIR, f"parallel_results_{timestamp}.csv")
    with open(parallel_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Degree', 'Status', 'Time_Sec', 'Num_Processes'])
        
        for r in results:
            status = "PASS" if r['parallel_success'] else "FAIL"
            time_val = r['parallel_time'] if r['parallel_time'] >= 0 else -1
            num_procs = r.get('num_processes', max_processes)
            writer.writerow([r['degree'], status, f"{time_val:.6f}", num_procs])
    
    print(f"Parallel results saved to: {parallel_file}")
    
    return summary_file, serial_file, parallel_file

def run_all_tests():
    """Run all polynomial multiplication tests."""
    max_processes = get_max_processes()
    print(f"\n{'#'*60}")
    print(f"POLYNOMIAL MULTIPLICATION TEST SUITE")
    print(f"Maximum MPI processes available: {max_processes}")
    print(f"{'#'*60}")
    
    # Test degrees: 10, 100, 1000, 10000, 100000, 1000000
    test_degrees = [10, 100, 1000, 10000, 100000, 1000000]
    # test_degrees = [10, 100]
    
    results = []
    
    for degree in test_degrees:
        print(f"\n{'*'*60}")
        print(f"TESTING POLYNOMIAL DEGREE: {degree}")
        print(f"{'*'*60}")
        
        # Test 1: Serial execution (1 process with -s flag)
        print(f"\n--- Serial Test (1 process, -s flag) ---")
        serial_success, serial_output, serial_time = run_polynomial_test(
            num_processes=1,
            degree=degree,
            serial_flag=True
        )
        
        # Test 2: Parallel execution (max processes, no -s flag)
        print(f"\n--- Parallel Test ({max_processes} processes) ---")
        for i in range(2, max_processes + 1):
            parallel_success, parallel_output, parallel_time = run_polynomial_test(
            num_processes=i,
            degree=degree,
            serial_flag=False
            )
        
            results.append({
                'degree': degree,
                'serial_success': serial_success,
                'serial_time': serial_time,
                'num_processes': i,
                'parallel_success': parallel_success,
                'parallel_time': parallel_time,
                'speedup': serial_time / parallel_time if parallel_time > 0 and serial_time > 0 else 0
            })
    
    # Save results to CSV
    save_results_to_csv(results, max_processes)
    
    # Print summary
    print(f"\n{'='*90}")
    print("TEST SUMMARY")
    print(f"{'='*90}")
    print(f"{'Degree':<12} {'Processes':<12} {'Serial':<12} {'Serial Time':<15} {'Parallel':<12} {'Parallel Time':<15} {'Speedup':<10}")
    print(f"{'-'*90}")
    
    for r in results:
        serial_status = "PASS" if r['serial_success'] else "FAIL"
        parallel_status = "PASS" if r['parallel_success'] else "FAIL"
        serial_time_str = f"{r['serial_time']:.4f}s" if r['serial_time'] >= 0 else "TIMEOUT"
        parallel_time_str = f"{r['parallel_time']:.4f}s" if r['parallel_time'] >= 0 else "TIMEOUT"
        speedup_str = f"{r['speedup']:.2f}x" if r['speedup'] > 0 else "N/A"
        num_procs = r.get('num_processes', max_processes)
        
        print(f"{r['degree']:<12} {num_procs:<12} {serial_status:<12} {serial_time_str:<15} {parallel_status:<12} {parallel_time_str:<15} {speedup_str:<10}")
    
    print(f"{'='*90}")
    
    # Return overall success
    all_passed = all(r['serial_success'] and r['parallel_success'] for r in results)
    return all_passed, results

def run_specific_test(num_processes, degree, serial_flag=False):
    """Run a specific test with given parameters."""
    success, output, exec_time = run_polynomial_test(num_processes, degree, serial_flag)
    return success

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Test polynomial multiplication with MPI")
    parser.add_argument("-n", "--num-processes", type=int, help="Number of MPI processes")
    parser.add_argument("-d", "--degree", type=int, help="Polynomial degree")
    parser.add_argument("-s", "--serial", action="store_true", help="Use serial flag")
    parser.add_argument("-a", "--all", action="store_true", help="Run all tests")
    
    args = parser.parse_args()
    
    if args.all or (args.num_processes is None and args.degree is None):
        # Run all tests
        success, results = run_all_tests()
        sys.exit(0 if success else 1)
    else:
        # Run specific test
        if args.num_processes is None or args.degree is None:
            print("Error: Both -n and -d are required for specific tests")
            sys.exit(1)
        
        success = run_specific_test(args.num_processes, args.degree, args.serial)
        sys.exit(0 if success else 1)
