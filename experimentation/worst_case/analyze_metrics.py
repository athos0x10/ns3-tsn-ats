import argparse
import os
import re
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def parse_filename_info(filepath):
    """Extracts scenario name and interference load from the CSV filename.

    Expected format: packet_metrics_<SCENARIO>_load_<LOAD>.csv
    Example: packet_metrics_S1.1.1_load_0.150000.csv -> ('S1.1.1', '15.0%')
    """
    filename = os.path.basename(filepath)
    pattern = r"packet_metrics_(?P<scenario>[\w\.]+)_load_(?P<load>[\d\.]+)\.csv"
    match = re.search(pattern, filename)

    if match:
        scenario = match.group("scenario")
        raw_load = float(match.group("load"))
        load_percent = f"{raw_load * 100:.1f}%"
        return scenario, load_percent
    else:
        return "Unknown Scenario", "Unknown Load"


def plot_packet_metrics(csv_file, save_fig=True):
    # 1. Load CSV Data
    df = pd.read_csv(csv_file)

    time_cols = [
        "Stay_Time_SW1",
        "Stay_Time_SW2",
        "Diff_Eli_Dest_SW1",
        "Diff_Eli_Dest_SW2",
        "Diff_EntrySW1_ExitSW2",
    ]

    # --- DATA CLEANING (-1 -> NaN) ---
    # Replace all values <= 0 (or -1) with NaN so they are ignored in calculations/plots
    df_clean = df.copy()
    for col in time_cols:
        df_clean[col] = df_clean[col].apply(lambda x: np.nan if x < 0 else x)

    # Convert seconds to microseconds (µs)
    df_us = df_clean.copy()
    for col in time_cols:
        df_us[col] = df_us[col] * 1e6  # Convert s -> µs

    # 2. Extract metadata for Subtitle/Title
    scenario, load = parse_filename_info(csv_file)

    # 3. Figure Setup
    fig, axes = plt.subplots(3, 1, figsize=(12, 10), sharex=True)
    fig.suptitle(
        f"ATS Flow Analysis: Per-Switch & End-to-End Metrics\n"
        f"Scenario: {scenario}  |  Interference Load: {load}",
        fontsize=14,
        fontweight="bold",
    )

    color_sw1 = "#1f77b4"   # Blue
    color_sw2 = "#ff7f0e"   # Orange
    color_total = "#2ca02c" # Green

    packet_ids = df_us["Packet_UID"]

    # Utility function to automatically adjust the Y-axis scale ignoring outliers/NaN values
    def set_smart_ylim(ax, series_list, margin_ratio=0.10):
        combined = pd.concat(series_list).dropna()
        if not combined.empty:
            y_min, y_max = combined.min(), combined.max()
            if y_min == y_max:
                # If all values are constant, add a small range around them
                ax.set_ylim(y_min * 0.9, y_max * 1.1 if y_max != 0 else 1.0)
            else:
                margin = (y_max - y_min) * margin_ratio
                ax.set_ylim(max(0, y_min - margin), y_max + margin)

    # --- Plot 1: Switch Residence/Stay Time (SW1 vs SW2) ---
    axes[0].plot(
        packet_ids,
        df_us["Stay_Time_SW1"],
        label="Switch 1 Residence Time",
        color=color_sw1,
        linewidth=1.5,
    )
    axes[0].plot(
        packet_ids,
        df_us["Stay_Time_SW2"],
        label="Switch 2 Residence Time",
        color=color_sw2,
        linewidth=1.5,
    )
    axes[0].set_title(
        "Residence Time Inside Switches (Ingress Arrival to Egress Departure)",
        fontsize=11,
    )
    axes[0].set_ylabel("Time (µs)", fontsize=10)
    axes[0].grid(True, linestyle="--", alpha=0.6)
    axes[0].legend(loc="upper right")
    set_smart_ylim(axes[0], [df_us["Stay_Time_SW1"], df_us["Stay_Time_SW2"]])

    # --- Plot 2: Delay from Eligibility Time to Destination ---
    axes[1].plot(
        packet_ids,
        df_us["Diff_Eli_Dest_SW1"],
        label="SW1 Eligibility to Destination",
        color=color_sw1,
        linewidth=1.5,
    )
    axes[1].plot(
        packet_ids,
        df_us["Diff_Eli_Dest_SW2"],
        label="SW2 Eligibility to Destination",
        color=color_sw2,
        linewidth=1.5,
    )
    axes[1].set_title(
        "Delay from ATS Eligibility Calculation to Destination Arrival",
        fontsize=11,
    )
    axes[1].set_ylabel("Time (µs)", fontsize=10)
    axes[1].grid(True, linestyle="--", alpha=0.6)
    axes[1].legend(loc="upper right")
    set_smart_ylim(axes[1], [df_us["Diff_Eli_Dest_SW1"], df_us["Diff_Eli_Dest_SW2"]])

    # --- Plot 3: Cumulative Delay SW1 Ingress to SW2 Egress ---
    axes[2].plot(
        packet_ids,
        df_us["Diff_EntrySW1_ExitSW2"],
        label="SW1 Entry to SW2 Exit Delay",
        color=color_total,
        linewidth=1.5,
    )
    axes[2].set_title(
        "Network Transit Delay (SW1 Ingress to SW2 Egress)", fontsize=11
    )
    axes[2].set_xlabel("Packet Unique Identifier (UID)", fontsize=10)
    axes[2].set_ylabel("Time (µs)", fontsize=10)
    axes[2].grid(True, linestyle="--", alpha=0.6)
    axes[2].legend(loc="upper right")
    set_smart_ylim(axes[2], [df_us["Diff_EntrySW1_ExitSW2"]])

    plt.tight_layout()

    if save_fig:
        output_image = (
            f"analysis_{scenario}_load_{load.replace('%', 'pct')}.png"
        )
        plt.savefig(output_image, dpi=300)
        print(f"[+] Plot saved successfully as: {output_image}")

    plt.show()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Analyze ATS Packet Metrics CSV File."
    )
    parser.add_argument(
        "csv_file",
        type=str,
        help="Path to the packet metrics CSV file (e.g., packet_metrics_S1.1.1_load_0.100000.csv)",
    )
    args = parser.parse_args()

    if os.path.exists(args.csv_file):
        plot_packet_metrics(args.csv_file)
    else:
        print(f"[-] Error: File '{args.csv_file}' not found.")