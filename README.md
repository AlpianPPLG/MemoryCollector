# Systems Memory Collector: Generational Tracing GC, LISP-2 Compactor & Bytecode VM in Modern C

Sebuah implementasi mandiri (*from-scratch*) dalam bahasa C modern (C23 standard) yang mencakup:
1. **Generational Garbage Collector** (Nursery Bump Allocator & Tenured Free-List).
2. **Object Promotion / Tenuring Engine**: Mempromosikan objek yang bertahan hidup dari Nursery ke Tenured Space sehingga Nursery selalu dapat di-reset 100%.
3. **LISP-2 Heap Compactor**: Mengatasi fragmentasi memori eksternal dengan merapatkan objek hidup menjadi satu blok kontigu (*zero fragmentation*).
4. **Bytecode Virtual Machine (Stack Machine)**: Runtime interpreter instruksi bytecode lengkap yang terintegrasi langsung dengan GC Engine melalui *stack root tracking*.

---

## 🏛️ Arsitektur Sistem

```
┌────────────────────────────────────────────────────────┐
│             Bytecode Virtual Machine (VM)              │
│    • Stack Evaluation (1024 slots registered as roots) │
│    • Instruction Dispatch (Arithmetic, Strings, Arrays)│
└──────────────────────────┬─────────────────────────────┘
                           │ gc_alloc() & Stack Roots
┌──────────────────────────▼─────────────────────────────┐
│                 Garbage Collector Engine               │
│   • Tri-Color Marking (White/Grey/Black Worklist)      │
│   • Object Promotion / Scavenging (Nursery -> Tenured) │
│   • LISP-2 Two-Pass Compactor & Defragmenter           │
│   • Precise Root Registry (Stack & Globals)            │
└─────────────┬────────────────────────────┬─────────────┘
              │ Minor Collection (Promote) │ Major Collection & Compact
┌─────────────▼──────────────┐ ┌───────────▼─────────────┐
│    Nursery (Young Gen)     │ │   Tenured (Old Gen)     │
│   Fast O(1) Bump Allocator │ │   Segregated Free-List  │
└─────────────┬──────────────┘ └───────────┬─────────────┘
              └──────────────┬─────────────┘
                             │ VirtualAlloc / mmap
┌────────────────────────────▼───────────────────────────┐
│           Platform Abstraction Layer (PAL)             │
│   Direct OS Kernel Pages & Hardware Guard Pages        │
└────────────────────────────────────────────────────────┘
```

---

## 📁 Struktur Repositori

```text
D:\Development\MemoryCollector\
├── include/
│   ├── mem_pal.h         # Platform Abstraction Layer (VirtualAlloc / mmap, page alignment)
│   ├── mem_object.h      # Layout header objek, tri-color tags, forwarding address
│   ├── mem_alloc.h       # Bump Allocator (Nursery) & Segregated Free-List (Tenured)
│   ├── mem_roots.h       # Precise Root Registry (melacak stack slots & global variables)
│   ├── mem_collector.h   # API Core GC (Promotion, Compaction, Scavenging)
│   └── mem_vm.h          # Bytecode VM (Opcodes, values, strings, arrays, stack roots)
├── src/
│   ├── mem_pal.c         # Implementasi memori virtual kernel (Windows & POSIX)
│   ├── mem_alloc.c       # Algoritma bump pointer O(1) & free-list coalescing
│   ├── mem_roots.c       # Registrasi dan traversal pointer root
│   ├── mem_collector.c   # Tri-color tracer, promotion engine, LISP-2 compactor
│   └── mem_vm.c          # Stack machine, instruction decoder, array/string tracer
├── examples/
│   ├── demo_basic.c      # Demo 1: Alokasi linked-list & deteksi kebocoran memori
│   ├── demo_cycles.c     # Demo 2: Siklus referensi melingkar (A -> B -> C -> A)
│   ├── demo_promotion.c  # Demo 3: Generational promotion & reset Nursery 100%
│   ├── demo_compaction.c # Demo 4: Defragmentasi heap dengan algoritma LISP-2
│   └── demo_vm.c         # Demo 5: Eksekusi bytecode VM di atas Garbage Collector
├── bin/                  # Executable hasil kompilasi
├── build.ps1             # Skrip build PowerShell
├── build.bat             # Skrip build CMD/Terminal (cukup ketik .\build.bat)
└── compile_commands.json # Konfigurasi language server Clangd / IDE
```

---

## 🚀 Cara Menjalankan

### 1. Kompilasi Seluruh Proyek
Jalankan di PowerShell:
```powershell
cd D:\Development\MemoryCollector
.\build.bat
# Atau: powershell -ExecutionPolicy Bypass -File .\build.ps1
```

### 2. Jalankan Pengujian Fitur
1. **Demo Dasar & Kebocoran Memori**:
   ```powershell
   .\bin\demo_basic.exe
   ```
2. **Demo Siklus Referensi Melingkar (*Cycle Detection*)**:
   ```powershell
   .\bin\demo_cycles.exe
   ```
3. **Demo Generational Promotion & Tenuring**:
   ```powershell
   .\bin\demo_promotion.exe
   ```
4. **Demo LISP-2 Heap Compaction (Defragmentasi)**:
   ```powershell
   .\bin\demo_compaction.exe
   ```
5. **Demo Bytecode Virtual Machine**:
   ```powershell
   .\bin\demo_vm.exe
   ```
