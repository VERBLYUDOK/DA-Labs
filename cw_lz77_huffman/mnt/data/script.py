from pathlib import Path
import os
import textwrap
import random
import numpy as np
import matplotlib.pyplot as plt

out_dir = Path("./mnt/data/datasets")
out_dir.mkdir(parents=True, exist_ok=True)

# dataset sizes (bytes)
sizes = [100_000, 400_000, 800_000]

def write_file(path: Path, data: bytes):
    with open(path, "wb") as f:
        f.write(data)

import string
alphabet = (string.ascii_letters + string.digits).encode('ascii')

# Existing functions
def make_two_letter(size):
    pat = (b"a"*5 + b"b"*5)
    return (pat * (size // len(pat) + 1))[:size]

def make_uniform(size):
    return bytes(random.choices(alphabet, k=size))

def make_normal(size):
    arr = np.random.normal(loc=128, scale=30, size=size).astype(np.int16)
    arr = np.clip(arr, 0, 255).astype(np.uint8)
    return bytes(arr.tobytes())

# create an image (PNG) — gradient + noise
img_path = out_dir / "test_image.png"
width, height = 1024, 768
grad = np.tile(np.linspace(0, 1, width, dtype=np.float32), (height,1))
noise = np.random.rand(height, width) * 0.2
img = np.clip(grad + noise, 0, 1)
plt.imsave(str(img_path), img, cmap='gray', format='png')

generated = []
for size in sizes:
    # Existing
    p = out_dir / f"twoletter_{size}.bin"
    write_file(p, make_two_letter(size))
    generated.append((p.name, p.stat().st_size, "twoletter", size))
    
    p = out_dir / f"uniform_{size}.bin"
    write_file(p, make_uniform(size))
    generated.append((p.name, p.stat().st_size, "uniform", size))
    
    p = out_dir / f"normal_{size}.bin"
    write_file(p, make_normal(size))
    generated.append((p.name, p.stat().st_size, "normal", size))


generated.append((img_path.name, img_path.stat().st_size, "image_png", img_path.stat().st_size))

print("Dataset directory:", out_dir)
print("Image sample saved at:", img_path)
print("Generated files:")
for name, fsize, typ, req_size in generated:
    print(f"  {name} ({typ}): {fsize} bytes (requested ~{req_size})")