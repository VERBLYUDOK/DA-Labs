#!/usr/bin/env python3
"""
Benchmark script for the archiver (per-dataset plots).
Usage:
  python3 test_archiver.py --arch /path/to/archiver [--out results.json] [--datasets ./datasets]
"""
import argparse, subprocess, time, json, os, sys
from pathlib import Path
import re
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import pandas as pd

def run_cmd_capture_stdout(cmd, out_path, timeout):
    with open(out_path, 'wb') as fout:
        proc = subprocess.run(cmd, stdout=fout, stderr=subprocess.PIPE, timeout=timeout)
        return proc.returncode, proc.stderr

def benchmark_one(arch, infile, mode, tmpdir, timeout):
    if mode == 'lz':
        level_flag = '-1'
    else:
        level_flag = '-9'
    compressed = tmpdir / (infile.name + '.' + mode + '.lz')

    cmd = [arch, level_flag, '-c', '-k', str(infile)]
    t0 = time.perf_counter()
    rc, stderr = run_cmd_capture_stdout(cmd, compressed, timeout)
    t1 = time.perf_counter()
    if rc != 0:
        raise RuntimeError(f"Compression failed: {stderr.decode(errors='replace') if stderr else '<no stderr>'}")
    compress_time = t1 - t0
    compressed_size = compressed.stat().st_size

    decompressed = tmpdir / (infile.name + f'.{mode}.dec')
    cmd = [arch, '-d', '-c', str(compressed)]
    t0 = time.perf_counter()
    rc, stderr = run_cmd_capture_stdout(cmd, decompressed, timeout)
    t1 = time.perf_counter()
    if rc != 0:
        raise RuntimeError(f"Decompression failed: {stderr.decode(errors='replace') if stderr else '<no stderr>'}")
    decompress_time = t1 - t0
    dec_size = decompressed.stat().st_size
    return compress_time, decompress_time, compressed_size, dec_size, str(compressed), str(decompressed)

def plot_three_for_dataset(dataset_name, df_subset, out_prefix):
    """
    df_subset: DataFrame with rows for this dataset (columns: file,size,mode,compress_time,decompress_time,compressed_size)
    """
    # Extract sizes and sort
    sizes = []
    # We'll map (size -> metrics per mode)
    data = {}
    for _, row in df_subset.iterrows():
        fname = row['file']
        # parse size from filename if present: name_12345.ext
        m = re.match(r'^(.+?)_(\d+)\.', fname)
        if m:
            size = int(m.group(2))
        else:
            size = int(row['size'])
        mode = row['mode']
        sizes.append(size)
        if size not in data:
            data[size] = {}
        data[size][mode] = {
            'compress_time': row.get('compress_time', float('nan')),
            'decompress_time': row.get('decompress_time', float('nan')),
            'compressed_size': row.get('compressed_size', float('nan'))
        }
    sizes = sorted(set(sizes))
    if not sizes:
        print("No data for dataset", dataset_name)
        return

    # Prepare series
    lz_ct = []
    huff_ct = []
    lz_dt = []
    huff_dt = []
    lz_ratio = []
    huff_ratio = []

    for s in sizes:
        entry = data.get(s, {})
        # LZ
        if 'lz' in entry:
            lz_ct.append(entry['lz']['compress_time'])
            lz_dt.append(entry['lz']['decompress_time'])
            lz_ratio.append(entry['lz']['compressed_size'] / s if s>0 else float('nan'))
        else:
            lz_ct.append(float('nan')); lz_dt.append(float('nan')); lz_ratio.append(float('nan'))
        # Huff
        if 'huff' in entry:
            huff_ct.append(entry['huff']['compress_time'])
            huff_dt.append(entry['huff']['decompress_time'])
            huff_ratio.append(entry['huff']['compressed_size'] / s if s>0 else float('nan'))
        else:
            huff_ct.append(float('nan')); huff_dt.append(float('nan')); huff_ratio.append(float('nan'))

    # If only one size, use bar charts; otherwise line plots
    single = (len(sizes) == 1)
    x_labels = [str(s) for s in sizes]

    # 1) compress time
    plt.figure(figsize=(8,4))
    if single:
        indices = range(len(sizes))
        width = 0.35
        plt.bar([i - width/2 for i in indices], lz_ct, width=width, label='LZ77 (-1)')
        plt.bar([i + width/2 for i in indices], huff_ct, width=width, label='LZ77+Huff (-9)')
        plt.xticks(indices, x_labels)
    else:
        plt.plot(sizes, lz_ct, marker='o', label='LZ77 (-1)')
        plt.plot(sizes, huff_ct, marker='o', label='LZ77+Huff (-9)')
        plt.xscale('log' if max(sizes)/min(sizes) > 10 else 'linear')
    plt.xlabel('Input size (bytes)')
    plt.ylabel('Compression time (s)')
    plt.title(f'{dataset_name}: compression time')
    plt.legend()
    plt.tight_layout()
    outname = f"{out_prefix}_{dataset_name}_compress_time.png"
    plt.savefig(outname)
    plt.close()
    print("Saved", outname)

    # 2) decompress time
    plt.figure(figsize=(8,4))
    if single:
        indices = range(len(sizes))
        width = 0.35
        plt.bar([i - width/2 for i in indices], lz_dt, width=width, label='LZ77 (-1)')
        plt.bar([i + width/2 for i in indices], huff_dt, width=width, label='LZ77+Huff (-9)')
        plt.xticks(indices, x_labels)
    else:
        plt.plot(sizes, lz_dt, marker='o', label='LZ77 (-1)')
        plt.plot(sizes, huff_dt, marker='o', label='LZ77+Huff (-9)')
        plt.xscale('log' if max(sizes)/min(sizes) > 10 else 'linear')
    plt.xlabel('Input size (bytes)')
    plt.ylabel('Decompression time (s)')
    plt.title(f'{dataset_name}: decompression time')
    plt.legend()
    plt.tight_layout()
    outname = f"{out_prefix}_{dataset_name}_decompress_time.png"
    plt.savefig(outname)
    plt.close()
    print("Saved", outname)

    # 3) compression ratio
    plt.figure(figsize=(8,4))
    if single:
        indices = range(len(sizes))
        width = 0.35
        plt.bar([i - width/2 for i in indices], lz_ratio, width=width, label='LZ77 (-1)')
        plt.bar([i + width/2 for i in indices], huff_ratio, width=width, label='LZ77+Huff (-9)')
        plt.xticks(indices, x_labels)
    else:
        plt.plot(sizes, lz_ratio, marker='o', label='LZ77 (-1)')
        plt.plot(sizes, huff_ratio, marker='o', label='LZ77+Huff (-9)')
        plt.xscale('log' if max(sizes)/min(sizes) > 10 else 'linear')
    plt.xlabel('Input size (bytes)')
    plt.ylabel('Compressed / Original')
    plt.title(f'{dataset_name}: compression ratio')
    plt.legend()
    plt.tight_layout()
    outname = f"{out_prefix}_{dataset_name}_compression_ratio.png"
    plt.savefig(outname)
    plt.close()
    print("Saved", outname)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--arch', required=True)
    parser.add_argument('--out', default='results.json')
    parser.add_argument('--datasets', default='./mnt/data/datasets')
    parser.add_argument('--timeout', type=int, default=300)
    args = parser.parse_args()

    arch = args.arch
    data_dir = Path(args.datasets)
    if not Path(arch).exists():
        print("archiver not found:", arch); sys.exit(2)
    if not data_dir.exists():
        print("datasets dir not found:", data_dir); sys.exit(2)

    tmpdir = Path('./benchmark_tmp'); tmpdir.mkdir(exist_ok=True)
    rows = []
    files = sorted([p for p in data_dir.iterdir() if p.is_file()])

    for f in files:
        for mode in ('lz','huff'):
            print(f"Running {f.name} mode {mode} ...", flush=True)
            try:
                ctime, dtime, csize, dsize, comp_path, dec_path = benchmark_one(arch, f, mode, tmpdir, args.timeout)
            except Exception as e:
                print("Error processing", f, "->", e, flush=True)
                continue
            print(f"Done {f.name} mode {mode}: compress {ctime:.3f}s, decompress {dtime:.3f}s, csize={csize}", flush=True)
            rows.append({
                'file': f.name,
                'size': f.stat().st_size,
                'mode': mode,
                'compress_time': ctime,
                'decompress_time': dtime,
                'compressed_size': csize,
                'decompressed_size': dsize,
                'compressed_path': comp_path,
                'decompressed_path': dec_path
            })

    # Save raw results
    with open(args.out, 'w') as fout:
        json.dump(rows, fout, indent=2)
    print("Results saved to", args.out)

    if not rows:
        print("No successful runs, exiting.")
        return

    df = pd.DataFrame(rows)
    # group into datasets by parsing filenames like name_size.ext or by stem
    datasets = {}
    for _, r in df.iterrows():
        fname = r['file']
        m = re.match(r'^(.+?)_(\d+)\.', fname)
        if m:
            name = m.group(1)
        else:
            name = Path(fname).stem
        if name not in datasets:
            datasets[name] = []
        datasets[name].append(r)

    # For each dataset create three plots
    out_prefix = "plot"
    for name, rows_list in datasets.items():
        df_subset = pd.DataFrame(rows_list)
        print("Plotting dataset:", name, flush=True)
        plot_three_for_dataset(name, df_subset, out_prefix)

if __name__ == '__main__':
    main()
