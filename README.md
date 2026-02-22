# BPack - High-Performance Asset Pipeline & Packer

BPack is a minimalist asset packer written in pure C. It is designed with **Data Oriented Design** in mind, focusing on cache efficiency, memory-mapped I/O readiness, and FAST architecture.

## Technical Highlights

- **Custom Arena Allocation**: Uses a linear arena allocator to eliminate heap fragmentation and provide O(1) allocation/deallocation. Memory is managed in large contiguous blocks to maximize pointer locality.
- **LZ4 Compression**: Integrated the industry-standard LZ4 algorithm. BPack implements an intelligent compression heuristic: it only stores compressed data if the resulting size is smaller than the raw input, avoiding CPU overhead for already-compressed assets (like PNGs or Vorbis).
- **Cache-Line Alignment (64-byte)**: All asset data blocks are padded to 64-byte boundaries. This ensures that every asset starts at the beginning of a CPU cache line, preventing "false sharing" and optimizing SIMD/DMA transfers.
- **Zero-Dependency Win32 Backend**: Built directly on top of the Windows API (`FindFirstFileA`, `CreateFile`) for maximum filesystem performance without the overhead of the C Runtime (CRT) wrappers.
- **Deterministic Hashing**: Implements the DJB2 string hashing algorithm for O(1) asset lookups within the Table of Contents (TOC).

## Binary Layout

The `.pak` format is designed for **Zero-Parse Loading**. The layout allows a game engine to `mmap` the file and cast the memory directly to a struct.

| Offset | Content | Description |
| :--- | :--- | :--- |
| `0x00` | **Header** | Magic `BPCK`, version, and total asset count. |
| `0x10` | **TOC** | Array of `BPackEntry` (Hash, Offset, Raw Size, Comp Size, Flags). |
| `...` | **Data** | 64-byte aligned data blobs. |

## Building

BPack rejects complex build systems. It is a "one-click" build.

### Using MSVC (Windows)
From the Developer Command Prompt:
```batch
cl.exe /nologo /Zi /O2 /FC main.c lz4.c /Fe:bpack.exe
```
Or you can use the build.bat file in the root of the repository