# Technical Architecture: Generational GC, LISP-2 Compactor & Bytecode VM

Spesifikasi teknis mendalam arsitektur memory management tingkat sistem dan runtime virtual machine.

---

## 1. Fitur Generational Object Promotion (Scavenging)

### Masalah pada Bump Allocator Sederhana
Bump allocator di ruang Nursery mengalokasikan memori secara linear dengan memajukan pointer offset ($O(1)$). Namun, jika ada objek yang tetap hidup (*survivor*) di Nursery, bump pointer tidak dapat di-reset kembali ke nol tanpa menimpa objek hidup tersebut.

### Solusi Teknis: *Cheney-Style Promotion*
Pada fungsi `gc_collect_minor()`:
1. **Mark Phase**: Menelusuri seluruh graf objek dari akar (*roots*). Objek hidup ditandai hitam (`GC_COLOR_BLACK`).
2. **Copy & Forward**: Objek hidup di Nursery yang usianya mencapai `tenuring_threshold` disalin ke Tenured Space (`freelist_alloc`).
3. **Forwarding Address**: Alamat baru di Tenured Space dicatat pada field `hdr->forwarding_address`.
4. **Update Pointers**: Seluruh variabel root di stack dan field objek hidup diperbarui untuk menunjuk ke alamat baru.
5. **Nursery Reset**: Karena semua objek hidup telah dipromosikan ke Tenured Space, Nursery kini berisi 100% objek mati. `bump_alloc_reset()` dijalankan, mengembalikan ruang Nursery ke kapasitas 100% secara instan!

---

## 2. Fitur Defragmentasi Heap: LISP-2 Compactor

Seiring berjalannya program, alokasi dan pembebasan objek dengan berbagai ukuran di Tenured Space menimbulkan fragmentasi eksternal (*holes / checkerboard pattern*).

Algoritma **LISP-2 Two-Pass Compactor** di `gc_compact()` merapatkan objek hidup menjadi satu blok kontigu:

```
Sebelum Kompaksi (Terfragmentasi):
[Objek A] [Lubang Kosong] [Objek B] [Lubang Kosong] [Objek C] [Sisa Free]

Pass 1: Menghitung Forwarding Address secara kontigu dari pangkal arena (tenured_raw).
Pass 2: Memperbarui semua pointer di roots dan graf objek ke alamat masa depan.
Pass 3: Melakukan memmove fisik objek ke lokasi kontigu baru.

Setelah Kompaksi (Zero Fragmentation):
[Objek A][Objek B][Objek C] [============= SATU BLOK FREE BESAR =============]
```

Hasilnya, seluruh sisa ruang Tenured disatukan menjadi satu blok free-list raksasa, mengeliminasi fragmentasi secara total.

---

## 3. Bytecode Virtual Machine (Stack Machine)

### Arsitektur VM
Runtime stack-machine yang dirancang untuk membuktikan integrasi nyata GC:
- **Stack Operan**: 1024 slot `vm_val_t` (mendukung `VAL_NIL`, `VAL_INT`, `VAL_OBJ`).
- **Integrasi GC Root**: Seluruh slot aktif pada stack VM didaftarkan ke *Root Registry* GC. Setiap objek (String, Array, Pair) yang berada di stack secara otomatis dilindungi dari pembersihan saat GC terpicu.
- **Pembersihan Slot Pop**: Setiap kali sebuah nilai di-pop dari stack VM, slot tersebut di-reset menjadi `nil` untuk mencegah *stale reference retention* (objek mati yang tertahan karena pointer basi di stack).
- **Deep Tracing Array & Pair**: Objek array dan pair memiliki callback penelusur (`trace_vm_array` dan `trace_vm_pair`) yang menelusuri elemen-elemen di dalamnya secara rekursif.
