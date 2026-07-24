import argparse
import os
import re
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def parse_filename_info(filepath):
    """Extracts scenario name and network load from the CSV filename."""
    filename = os.path.basename(filepath)
    # Match scenario and load strictly without capturing trailing extensions
    pattern = r"packet_metrics_(?P<scenario>[\w\.]+)_load_(?P<load>\d+(?:\.\d+)?)"
    match = re.search(pattern, filename)

    if match:
        scenario = match.group("scenario")
        raw_load = float(match.group("load"))
        load_percent = f"{raw_load * 100:.1f}%"
        return scenario, load_percent
    else:
        return "Unknown Scenario", "Unknown Load"


def set_smart_ylim(ax, series_list, margin_ratio=0.10):
    """Dynamically adjusts Y-axis limits ignoring NaNs."""
    combined = pd.concat(series_list).dropna()
    if not combined.empty:
        y_min, y_max = combined.min(), combined.max()
        if y_min == y_max:
            ax.set_ylim(y_min * 0.9, y_max * 1.1 if y_max != 0 else 1.0)
        else:
            margin = (y_max - y_min) * margin_ratio
            ax.set_ylim(max(0, y_min - margin), y_max + margin)


def plot_packet_metrics(csv_file, mode=None, save_fig=True):
    # 1. Load CSV data
    df = pd.read_csv(csv_file)

    # Automatic mode detection (ATS or CBS) if not specified
    is_ats = "Diff_Eli_Dest_SW1" in df.columns
    if mode is None:
        mode = "ats" if is_ats else "cbs"
    else:
        mode = mode.lower()

    # Define metric columns based on the selected mode
    time_cols = ["Stay_Time_SW1", "Stay_Time_SW2", "Diff_EntrySW1_ExitSW2"]
    if mode == "ats" and is_ats:
        time_cols.extend(["Diff_Eli_Dest_SW1", "Diff_Eli_Dest_SW2"])

    # --- DATA CLEANING (-1 -> NaN) ---
    df_clean = df.copy()
    for col in time_cols:
        if col in df_clean.columns:
            df_clean[col] = df_clean[col].apply(lambda x: np.nan if x < 0 else x)

    # Calculate packet loss / drop statistics
    total_packets = len(df)
    dropped_packets = df["Diff_EntrySW1_ExitSW2"].apply(lambda x: x < 0).sum()
    drop_rate = (dropped_packets / total_packets) * 100 if total_packets > 0 else 0.0

    shaper_name = "ATS" if mode == "ats" else "CBS"

    print(f"\n--- {shaper_name} Analysis Report ---")
    print(f"File: {csv_file}")
    print(f"Total Packets:   {total_packets}")
    print(f"Dropped Packets: {dropped_packets} ({drop_rate:.2f}%)")

    # Convert seconds to microseconds (µs)
    df_us = df_clean.copy()
    for col in time_cols:
        if col in df_us.columns:
            df_us[col] = df_us[col] * 1e6

    # 2. Metadata extraction
    scenario, load = parse_filename_info(csv_file)

    color_sw1 = "#1f77b4"   # Blue
    color_sw2 = "#ff7f0e"   # Orange
    color_total = "#2ca02c" # Green

    packet_ids = df_us["Packet_UID"]

    # 3. Figure Setup (3 plots for ATS, 2 plots for CBS)
    num_plots = 3 if (mode == "ats" and is_ats) else 2
    fig, axes = plt.subplots(
        num_plots, 1, figsize=(12, 4 * num_plots), sharex=True
    )
    if num_plots == 1:
        axes = [axes]

    title_drop_info = f"  |  Drop Rate: {drop_rate:.1f}%" if mode == "cbs" else ""
    fig.suptitle(
        f"{shaper_name} Flow Analysis: Per-Switch & End-to-End Metrics\n"
        f"Scenario: {scenario}  |  Interference Load: {load}{title_drop_info}",
        fontsize=14,
        fontweight="bold",
    )

    # --- Plot 1: Residence / Stay Time (SW1 vs SW2) ---
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

    current_idx = 1

    # --- Plot 2 (ATS ONLY): Delay from Eligibility Time ---
    if mode == "ats" and is_ats:
        axes[current_idx].plot(
            packet_ids,
            df_us["Diff_Eli_Dest_SW1"],
            label="SW1 Eligibility to Destination",
            color=color_sw1,
            linewidth=1.5,
        )
        axes[current_idx].plot(
            packet_ids,
            df_us["Diff_Eli_Dest_SW2"],
            label="SW2 Eligibility to Destination",
            color=color_sw2,
            linewidth=1.5,
        )
        axes[current_idx].set_title(
            "Delay from ATS Eligibility Calculation to Destination Arrival",
            fontsize=11,
        )
        axes[current_idx].set_ylabel("Time (µs)", fontsize=10)
        axes[current_idx].grid(True, linestyle="--", alpha=0.6)
        axes[current_idx].legend(loc="upper right")
        set_smart_ylim(
            axes[current_idx],
            [df_us["Diff_Eli_Dest_SW1"], df_us["Diff_Eli_Dest_SW2"]],
        )
        current_idx += 1

    # --- Final Plot: Transit Delay (SW1 Ingress -> SW2 Egress) ---
    axes[current_idx].plot(
        packet_ids,
        df_us["Diff_EntrySW1_ExitSW2"],
        label="SW1 Entry to SW2 Exit Delay",
        color=color_total,
        linewidth=1.5,
    )
    axes[current_idx].set_title(
        "Network Transit Delay (SW1 Ingress to SW2 Egress)", fontsize=11
    )
    axes[current_idx].set_xlabel(
        "Packet Unique Identifier (UID)", fontsize=10
    )
    axes[current_idx].set_ylabel("Time (µs)", fontsize=10)
    axes[current_idx].grid(True, linestyle="--", alpha=0.6)
    axes[current_idx].legend(loc="upper right")
    set_smart_ylim(axes[current_idx], [df_us["Diff_EntrySW1_ExitSW2"]])

    plt.tight_layout()

    if save_fig:
        output_image = (
            f"analysis_{shaper_name}_{scenario}_load_{load.replace('%', 'pct')}.png"
        )
        plt.savefig(output_image, dpi=300)
        print(f"[+] Plot saved successfully as: {output_image}\n")

    plt.show()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Analyze ATS or CBS Packet Metrics CSV File."
    )
    parser.add_argument(
        "csv_file", type=str, help="Path to the packet metrics CSV file."
    )
    parser.add_argument(
        "--mode",
        type=str,
        choices=["ats", "cbs"],
        default=None,
        help="Specify shaper mode: 'ats' or 'cbs' (Auto-detected by default).",
    )

    args = parser.parse_args()

    if os.path.exists(args.csv_file):
        plot_packet_metrics(args.csv_file, mode=args.mode)
    else:
        print(f"[-] Error: File '{args.csv_file}' not found.")