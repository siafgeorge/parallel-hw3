#!/usr/bin/env python3
"""
CSV Master - Plotting script for parallel polynomial computation results
Reads results from multiple result folders (result1, result2, result3, result4),
calculates averages, and creates multiple visualization plots.
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os
import glob

# Configuration
BASE_DIR = os.path.join(os.path.dirname(__file__), '../../3.1/results')
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), '../../3.1/results/plots')

def load_single_csv(csv_path):
    """Load a single CSV file, handling different delimiters."""
    # Try pipe delimiter first, then comma
    try:
        df = pd.read_csv(csv_path, delimiter='|')
        if len(df.columns) == 1:  # Wrong delimiter
            df = pd.read_csv(csv_path, delimiter=',')
    except:
        df = pd.read_csv(csv_path, delimiter=',')
    
    df.columns = df.columns.str.strip()
    return df

def load_all_data(base_dir):
    """
    Load data from all result folders and calculate averages.
    Returns a DataFrame with averaged values for each (processes, grade) combination.
    """
    all_data = []
    
    # Find all result folders
    result_folders = sorted(glob.glob(os.path.join(base_dir, 'result*')))
    print(f"Found {len(result_folders)} result folders")
    
    for folder in result_folders:
        csv_path = os.path.join(folder, 'csv', 'results.csv')
        if os.path.exists(csv_path):
            print(f"  Loading: {csv_path}")
            try:
                df = load_single_csv(csv_path)
                
                # Convert timing columns to numeric, coercing errors (N/A) to NaN
                time_cols = ['Broadcast time', 'Compute time', 'Reduce Time', 'Total time']
                for col in time_cols:
                    if col in df.columns:
                        df[col] = pd.to_numeric(df[col], errors='coerce')
                
                # Add source folder for reference
                df['source'] = os.path.basename(folder)
                all_data.append(df)
                print(f"    Loaded {len(df)} records")
            except Exception as e:
                print(f"    Error loading: {e}")
    
    if not all_data:
        raise ValueError("No data files found!")
    
    # Combine all data
    combined_df = pd.concat(all_data, ignore_index=True)
    print(f"\nTotal records before filtering: {len(combined_df)}")
    
    # Filter out rows with N/A (NaN) values
    time_cols = ['Broadcast time', 'Compute time', 'Reduce Time', 'Total time']
    valid_df = combined_df.dropna(subset=time_cols)
    print(f"Valid records after filtering N/A: {len(valid_df)}")
    
    # Group by (numberofprocesses, gradenumber) and calculate averages
    grouped = valid_df.groupby(['numberofprocesses', 'gradenumber']).agg({
        'Broadcast time': ['mean', 'std', 'count'],
        'Compute time': ['mean', 'std'],
        'Reduce Time': ['mean', 'std'],
        'Total time': ['mean', 'std']
    }).reset_index()
    
    # Flatten column names
    grouped.columns = [
        'numberofprocesses', 'gradenumber',
        'Broadcast time', 'Broadcast time_std', 'sample_count',
        'Compute time', 'Compute time_std',
        'Reduce Time', 'Reduce Time_std',
        'Total time', 'Total time_std'
    ]
    
    print(f"\nAveraged data: {len(grouped)} unique configurations")
    print(f"Processes: {sorted(grouped['numberofprocesses'].unique())}")
    print(f"Grades: {sorted(grouped['gradenumber'].unique())}")
    
    return grouped, valid_df

def ensure_output_dir(output_dir):
    """Create output directory if it doesn't exist."""
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

def plot_total_time_vs_processes(df, output_dir):
    """
    Plot 1: Total time vs Number of Processes for different grade numbers.
    Shows how parallelization affects total execution time with error bars.
    """
    fig, ax = plt.subplots(figsize=(12, 8))
    
    grades = sorted(df['gradenumber'].unique())
    colors = plt.cm.viridis(np.linspace(0, 1, len(grades)))
    
    for grade, color in zip(grades, colors):
        subset = df[df['gradenumber'] == grade].sort_values('numberofprocesses')
        yerr = subset['Total time_std'].fillna(0) if 'Total time_std' in subset.columns else None
        ax.errorbar(subset['numberofprocesses'], subset['Total time'], yerr=yerr,
                    marker='o', label=f'Grade {grade}', color=color, linewidth=2, 
                    markersize=8, capsize=4)
    
    ax.set_xlabel('Number of Processes', fontsize=12)
    ax.set_ylabel('Total Time (seconds)', fontsize=12)
    ax.set_title('Total Execution Time vs Number of Processes\n(by Polynomial Grade, averaged over runs)', fontsize=14)
    ax.legend(title='Polynomial Grade', loc='best')
    ax.grid(True, alpha=0.3)
    ax.set_yscale('log')
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'total_time_vs_processes.png'), dpi=300)
    plt.close()
    print("Created: total_time_vs_processes.png")

def plot_total_time_vs_grade(df, output_dir):
    """
    Plot 2: Total time vs Grade Number for different process counts.
    Shows how problem size affects execution time with error bars.
    """
    fig, ax = plt.subplots(figsize=(12, 8))
    
    processes = sorted(df['numberofprocesses'].unique())
    colors = plt.cm.plasma(np.linspace(0, 1, len(processes)))
    
    for proc, color in zip(processes, colors):
        subset = df[df['numberofprocesses'] == proc].sort_values('gradenumber')
        yerr = subset['Total time_std'].fillna(0) if 'Total time_std' in subset.columns else None
        ax.errorbar(subset['gradenumber'], subset['Total time'], yerr=yerr,
                    marker='s', label=f'{proc} processes', color=color, linewidth=2, 
                    markersize=8, capsize=4)
    
    ax.set_xlabel('Polynomial Grade (Problem Size)', fontsize=12)
    ax.set_ylabel('Total Time (seconds)', fontsize=12)
    ax.set_title('Total Execution Time vs Problem Size\n(by Number of Processes, averaged over runs)', fontsize=14)
    ax.legend(title='Processes', loc='best')
    ax.grid(True, alpha=0.3)
    ax.set_xscale('log')
    ax.set_yscale('log')
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'total_time_vs_grade.png'), dpi=300)
    plt.close()
    print("Created: total_time_vs_grade.png")

def plot_time_breakdown(df, output_dir):
    """
    Plot 3: Stacked bar chart showing time breakdown (Broadcast, Compute, Reduce).
    Shows the contribution of each phase to total time.
    """
    fig, axes = plt.subplots(2, 3, figsize=(18, 12))
    axes = axes.flatten()
    
    grades = sorted(df['gradenumber'].unique())
    
    for idx, grade in enumerate(grades):
        ax = axes[idx]
        subset = df[df['gradenumber'] == grade].sort_values('numberofprocesses')
        
        x = np.arange(len(subset))
        width = 0.6
        
        broadcast = subset['Broadcast time'].values
        compute = subset['Compute time'].values
        reduce_t = subset['Reduce Time'].values
        
        ax.bar(x, broadcast, width, label='Broadcast', color='#2ecc71')
        ax.bar(x, compute, width, bottom=broadcast, label='Compute', color='#3498db')
        ax.bar(x, reduce_t, width, bottom=broadcast+compute, label='Reduce', color='#e74c3c')
        
        ax.set_xlabel('Number of Processes')
        ax.set_ylabel('Time (seconds)')
        ax.set_title(f'Grade {grade}')
        ax.set_xticks(x)
        ax.set_xticklabels(subset['numberofprocesses'].values)
        ax.legend(loc='upper right')
        ax.grid(True, alpha=0.3, axis='y')
    
    # Hide empty subplot if any
    for idx in range(len(grades), len(axes)):
        axes[idx].set_visible(False)
    
    fig.suptitle('Time Breakdown by Phase\n(Broadcast + Compute + Reduce)', fontsize=16)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'time_breakdown.png'), dpi=300)
    plt.close()
    print("Created: time_breakdown.png")

def plot_speedup(df, output_dir):
    """
    Plot 4: Speedup curve - comparing execution time with baseline (2 processes).
    Shows parallel efficiency.
    """
    fig, ax = plt.subplots(figsize=(12, 8))
    
    grades = sorted(df['gradenumber'].unique())
    colors = plt.cm.viridis(np.linspace(0, 1, len(grades)))
    
    for grade, color in zip(grades, colors):
        subset = df[df['gradenumber'] == grade].sort_values('numberofprocesses')
        baseline_time = subset[subset['numberofprocesses'] == subset['numberofprocesses'].min()]['Total time'].values[0]
        speedup = baseline_time / subset['Total time']
        ax.plot(subset['numberofprocesses'], speedup, 
                marker='o', label=f'Grade {grade}', color=color, linewidth=2, markersize=8)
    
    # Plot ideal speedup line
    processes = sorted(df['numberofprocesses'].unique())
    min_proc = min(processes)
    ideal_speedup = [p / min_proc for p in processes]
    ax.plot(processes, ideal_speedup, 'k--', label='Ideal Speedup', linewidth=2, alpha=0.7)
    
    ax.set_xlabel('Number of Processes', fontsize=12)
    ax.set_ylabel('Speedup (relative to 2 processes)', fontsize=12)
    ax.set_title('Parallel Speedup Analysis\n(Higher is better)', fontsize=14)
    ax.legend(title='Polynomial Grade', loc='best')
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'speedup.png'), dpi=300)
    plt.close()
    print("Created: speedup.png")

def plot_efficiency(df, output_dir):
    """
    Plot 5: Parallel efficiency - Speedup / Number of Processes.
    Shows how efficiently the parallel resources are being used.
    """
    fig, ax = plt.subplots(figsize=(12, 8))
    
    grades = sorted(df['gradenumber'].unique())
    colors = plt.cm.viridis(np.linspace(0, 1, len(grades)))
    
    for grade, color in zip(grades, colors):
        subset = df[df['gradenumber'] == grade].sort_values('numberofprocesses')
        baseline_time = subset[subset['numberofprocesses'] == subset['numberofprocesses'].min()]['Total time'].values[0]
        min_proc = subset['numberofprocesses'].min()
        speedup = baseline_time / subset['Total time']
        efficiency = speedup / (subset['numberofprocesses'] / min_proc) * 100
        ax.plot(subset['numberofprocesses'], efficiency, 
                marker='o', label=f'Grade {grade}', color=color, linewidth=2, markersize=8)
    
    ax.axhline(y=100, color='k', linestyle='--', label='Ideal Efficiency (100%)', alpha=0.7)
    ax.set_xlabel('Number of Processes', fontsize=12)
    ax.set_ylabel('Parallel Efficiency (%)', fontsize=12)
    ax.set_title('Parallel Efficiency Analysis\n(100% = Perfect scaling)', fontsize=14)
    ax.legend(title='Polynomial Grade', loc='best')
    ax.grid(True, alpha=0.3)
    ax.set_ylim(0, 150)
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'efficiency.png'), dpi=300)
    plt.close()
    print("Created: efficiency.png")

def plot_time_components_heatmap(df, output_dir):
    """
    Plot 6: Heatmaps showing Broadcast, Compute, and Reduce times.
    Visual representation of how each component scales.
    """
    fig, axes = plt.subplots(1, 3, figsize=(18, 6))
    
    time_cols = ['Broadcast time', 'Compute time', 'Reduce Time']
    titles = ['Broadcast Time', 'Compute Time', 'Reduce Time']
    
    for ax, col, title in zip(axes, time_cols, titles):
        pivot = df.pivot(index='numberofprocesses', columns='gradenumber', values=col)
        
        im = ax.imshow(np.log10(pivot.values + 1e-10), aspect='auto', cmap='YlOrRd')
        
        ax.set_xticks(range(len(pivot.columns)))
        ax.set_xticklabels(pivot.columns)
        ax.set_yticks(range(len(pivot.index)))
        ax.set_yticklabels(pivot.index)
        
        ax.set_xlabel('Polynomial Grade')
        ax.set_ylabel('Number of Processes')
        ax.set_title(f'{title}\n(log scale)')
        
        cbar = plt.colorbar(im, ax=ax)
        cbar.set_label('log10(Time)')
    
    fig.suptitle('Time Components Heatmap', fontsize=16)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'time_heatmap.png'), dpi=300)
    plt.close()
    print("Created: time_heatmap.png")

def plot_communication_vs_computation(df, output_dir):
    """
    Plot 7: Communication overhead (Broadcast + Reduce) vs Computation time.
    Shows the ratio of communication to computation.
    """
    fig, axes = plt.subplots(1, 2, figsize=(16, 6))
    
    # Calculate communication overhead
    df['Communication time'] = df['Broadcast time'] + df['Reduce Time']
    df['Comm/Compute Ratio'] = df['Communication time'] / (df['Compute time'] + 1e-10)
    
    # Plot 1: Absolute times
    ax1 = axes[0]
    grades = sorted(df['gradenumber'].unique())
    x = np.arange(len(grades))
    width = 0.35
    
    for i, proc in enumerate(sorted(df['numberofprocesses'].unique())):
        subset = df[df['numberofprocesses'] == proc].sort_values('gradenumber')
        offset = (i - len(df['numberofprocesses'].unique())/2) * width/len(df['numberofprocesses'].unique())
    
    # Simplified: show for largest problem size
    subset = df[df['gradenumber'] == 100000].sort_values('numberofprocesses')
    x = np.arange(len(subset))
    
    ax1.bar(x - width/2, subset['Communication time'], width, label='Communication', color='#e74c3c')
    ax1.bar(x + width/2, subset['Compute time'], width, label='Computation', color='#3498db')
    ax1.set_xlabel('Number of Processes')
    ax1.set_ylabel('Time (seconds)')
    ax1.set_title('Communication vs Computation Time\n(Grade = 100,000)')
    ax1.set_xticks(x)
    ax1.set_xticklabels(subset['numberofprocesses'].values)
    ax1.legend()
    ax1.grid(True, alpha=0.3, axis='y')
    
    # Plot 2: Ratio across all configurations
    ax2 = axes[1]
    colors = plt.cm.viridis(np.linspace(0, 1, len(grades)))
    
    for grade, color in zip(grades, colors):
        subset = df[df['gradenumber'] == grade].sort_values('numberofprocesses')
        ax2.plot(subset['numberofprocesses'], subset['Comm/Compute Ratio'], 
                marker='o', label=f'Grade {grade}', color=color, linewidth=2, markersize=8)
    
    ax2.set_xlabel('Number of Processes')
    ax2.set_ylabel('Communication / Computation Ratio')
    ax2.set_title('Communication Overhead Ratio\n(Lower is better for parallel efficiency)')
    ax2.legend(title='Polynomial Grade', loc='best')
    ax2.grid(True, alpha=0.3)
    ax2.set_yscale('log')
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'comm_vs_compute.png'), dpi=300)
    plt.close()
    print("Created: comm_vs_compute.png")

def plot_summary_dashboard(df, output_dir):
    """
    Plot 8: Summary dashboard with key metrics.
    A comprehensive overview of all results.
    """
    fig = plt.figure(figsize=(20, 12))
    
    # Create grid
    gs = fig.add_gridspec(2, 3, hspace=0.3, wspace=0.3)
    
    # Plot 1: Total time vs processes (top-left)
    ax1 = fig.add_subplot(gs[0, 0])
    grades = sorted(df['gradenumber'].unique())
    for grade in grades:
        subset = df[df['gradenumber'] == grade].sort_values('numberofprocesses')
        ax1.plot(subset['numberofprocesses'], subset['Total time'], 
                marker='o', label=f'G={grade}', linewidth=2)
    ax1.set_xlabel('Processes')
    ax1.set_ylabel('Total Time (s)')
    ax1.set_title('Execution Time Scaling')
    ax1.legend(fontsize=8)
    ax1.set_yscale('log')
    ax1.grid(True, alpha=0.3)
    
    # Plot 2: Speedup (top-middle)
    ax2 = fig.add_subplot(gs[0, 1])
    for grade in grades:
        subset = df[df['gradenumber'] == grade].sort_values('numberofprocesses')
        baseline = subset['Total time'].iloc[0]
        speedup = baseline / subset['Total time']
        ax2.plot(subset['numberofprocesses'], speedup, marker='o', label=f'G={grade}', linewidth=2)
    processes = sorted(df['numberofprocesses'].unique())
    ideal = [p / min(processes) for p in processes]
    ax2.plot(processes, ideal, 'k--', label='Ideal', linewidth=2)
    ax2.set_xlabel('Processes')
    ax2.set_ylabel('Speedup')
    ax2.set_title('Parallel Speedup')
    ax2.legend(fontsize=8)
    ax2.grid(True, alpha=0.3)
    
    # Plot 3: Best configuration (top-right)
    ax3 = fig.add_subplot(gs[0, 2])
    best_times = df.loc[df.groupby('gradenumber')['Total time'].idxmin()]
    bars = ax3.bar(range(len(best_times)), best_times['Total time'], color='#3498db')
    ax3.set_xticks(range(len(best_times)))
    ax3.set_xticklabels([f"G={g}\n({p}p)" for g, p in zip(best_times['gradenumber'], best_times['numberofprocesses'])], fontsize=9)
    ax3.set_ylabel('Best Time (s)')
    ax3.set_title('Optimal Time per Grade\n(with best process count)')
    ax3.set_yscale('log')
    ax3.grid(True, alpha=0.3, axis='y')
    
    # Plot 4: Time breakdown for largest problem (bottom-left)
    ax4 = fig.add_subplot(gs[1, 0])
    subset = df[df['gradenumber'] == 100000].sort_values('numberofprocesses')
    x = range(len(subset))
    ax4.stackplot(x, 
                  subset['Broadcast time'], 
                  subset['Compute time'], 
                  subset['Reduce Time'],
                  labels=['Broadcast', 'Compute', 'Reduce'],
                  colors=['#2ecc71', '#3498db', '#e74c3c'])
    ax4.set_xticks(x)
    ax4.set_xticklabels(subset['numberofprocesses'].values)
    ax4.set_xlabel('Processes')
    ax4.set_ylabel('Time (s)')
    ax4.set_title('Time Breakdown (Grade=100000)')
    ax4.legend(loc='upper right', fontsize=8)
    ax4.grid(True, alpha=0.3, axis='y')
    
    # Plot 5: Compute time fraction (bottom-middle)
    ax5 = fig.add_subplot(gs[1, 1])
    df['Compute Fraction'] = df['Compute time'] / df['Total time'] * 100
    for grade in grades:
        subset = df[df['gradenumber'] == grade].sort_values('numberofprocesses')
        ax5.plot(subset['numberofprocesses'], subset['Compute Fraction'], 
                marker='o', label=f'G={grade}', linewidth=2)
    ax5.set_xlabel('Processes')
    ax5.set_ylabel('Compute Time %')
    ax5.set_title('Computation vs Overhead')
    ax5.legend(fontsize=8)
    ax5.grid(True, alpha=0.3)
    ax5.set_ylim(0, 105)
    
    # Plot 6: Performance summary table (bottom-right)
    ax6 = fig.add_subplot(gs[1, 2])
    ax6.axis('off')
    
    # Create summary statistics
    summary_data = []
    for grade in grades:
        subset = df[df['gradenumber'] == grade]
        best_row = subset.loc[subset['Total time'].idxmin()]
        summary_data.append([
            grade,
            f"{subset['Total time'].min():.4f}",
            int(best_row['numberofprocesses']),
            f"{subset['Total time'].max() / subset['Total time'].min():.2f}x"
        ])
    
    table = ax6.table(cellText=summary_data,
                     colLabels=['Grade', 'Best Time (s)', 'Best Procs', 'Speedup Range'],
                     loc='center',
                     cellLoc='center')
    table.auto_set_font_size(False)
    table.set_fontsize(10)
    table.scale(1.2, 1.5)
    ax6.set_title('Performance Summary', pad=20)
    
    fig.suptitle('Parallel Polynomial Computation - Performance Dashboard\n(Averaged over multiple runs)', fontsize=18, fontweight='bold')
    plt.savefig(os.path.join(output_dir, 'summary_dashboard.png'), dpi=300, bbox_inches='tight')
    plt.close()
    print("Created: summary_dashboard.png")

def plot_sample_count(df, output_dir):
    """
    Plot showing how many valid samples were averaged for each configuration.
    """
    fig, ax = plt.subplots(figsize=(12, 6))
    
    if 'sample_count' not in df.columns:
        print("Skipping sample_count plot (no sample_count column)")
        return
    
    pivot = df.pivot(index='numberofprocesses', columns='gradenumber', values='sample_count')
    
    im = ax.imshow(pivot.values, aspect='auto', cmap='YlGn')
    
    ax.set_xticks(range(len(pivot.columns)))
    ax.set_xticklabels(pivot.columns)
    ax.set_yticks(range(len(pivot.index)))
    ax.set_yticklabels(pivot.index)
    
    # Add text annotations
    for i in range(len(pivot.index)):
        for j in range(len(pivot.columns)):
            text = ax.text(j, i, int(pivot.values[i, j]),
                          ha="center", va="center", color="black", fontsize=12)
    
    ax.set_xlabel('Polynomial Grade')
    ax.set_ylabel('Number of Processes')
    ax.set_title('Number of Valid Samples Averaged\n(per configuration)')
    
    cbar = plt.colorbar(im, ax=ax)
    cbar.set_label('Sample Count')
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'sample_count.png'), dpi=300)
    plt.close()
    print("Created: sample_count.png")

def main():
    """Main function to generate all plots from averaged data."""
    print("=" * 60)
    print("CSV Master - Parallel Computation Results Plotter")
    print("(Averaging results from multiple test runs)")
    print("=" * 60)
    
    # Load and average data from all result folders
    print(f"\nLoading data from: {BASE_DIR}")
    df, raw_df = load_all_data(BASE_DIR)
    
    # Ensure output directory exists
    ensure_output_dir(OUTPUT_DIR)
    print(f"\nOutput directory: {OUTPUT_DIR}")
    
    # Save the averaged data to CSV
    avg_csv_path = os.path.join(OUTPUT_DIR, 'averaged_results.csv')
    df.to_csv(avg_csv_path, index=False)
    print(f"Saved averaged data to: {avg_csv_path}")
    
    # Generate all plots
    print("\nGenerating plots...")
    print("-" * 40)
    
    plot_total_time_vs_processes(df, OUTPUT_DIR)
    plot_total_time_vs_grade(df, OUTPUT_DIR)
    plot_time_breakdown(df, OUTPUT_DIR)
    plot_speedup(df, OUTPUT_DIR)
    plot_efficiency(df, OUTPUT_DIR)
    plot_time_components_heatmap(df, OUTPUT_DIR)
    plot_communication_vs_computation(df, OUTPUT_DIR)
    plot_sample_count(df, OUTPUT_DIR)
    plot_summary_dashboard(df, OUTPUT_DIR)
    
    print("-" * 40)
    print(f"\nAll plots saved to: {OUTPUT_DIR}")
    print("=" * 60)

if __name__ == "__main__":
    main()
