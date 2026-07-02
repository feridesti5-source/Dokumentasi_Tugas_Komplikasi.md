# Tugas Proyek Akhir: Representasi Tahapan Kompilasi
## Konstruksi: Percabangan / Kondisi (`If-Else`) — Bahasa Implementasi: C++

---

## 1. Pilihan Konstruksi

Konstruksi sintaksis yang dipilih adalah **percabangan `if-else`**, yaitu struktur kondisional yang mengeksekusi salah satu dari dua blok statement berdasarkan hasil evaluasi sebuah kondisi boolean.

Selain mendukung kondisi relasional sederhana (`x > 5`), implementasi ini juga mendukung **ekspresi aritmatika** di dalam kondisi maupun di sisi kanan assignment (misalnya `a + b > c * 2`), sehingga tahap generasi kode antara (TAC) dapat didemonstrasikan secara lebih realistis melalui penggunaan variabel sementara (*temporary variable*).

---

## 2. Pattern (Pola Sintaks / Grammar)

Grammar didefinisikan menggunakan pendekatan **Backus-Naur Form (BNF)**:

```text
<if_stmt>     ::= "if" "(" <condition> ")" "{" <stmt_list> "}"
                  "else" "{" <stmt_list> "}"

<condition>   ::= <expr> <relop> <expr>

<relop>       ::= "<" | ">" | "<=" | ">=" | "==" | "!="

<stmt_list>   ::= <assign_stmt>*

<assign_stmt> ::= <identifier> "=" <expr> ";"

<expr>        ::= <term> (("+" | "-") <term>)*

<term>        ::= <factor> (("*" | "/") <factor>)*

<factor>      ::= <identifier> | <number> | "(" <expr> ")"
```

Grammar ini bersifat **rekursif** pada level `<expr>`, `<term>`, dan `<factor>`, sehingga mendukung ekspresi aritmatika bersarang seperti `(a + b) * 2` maupun kondisi kompleks seperti `a + b > c * 2`.

---

## 3. Arsitektur Program

Program (`ifelse_compiler.cpp`) dibagi menjadi empat modul utama yang merepresentasikan empat tahap kompilasi, ditambah driver program utama (`main`).

```
┌─────────────┐    ┌─────────────┐    ┌──────────────┐    ┌──────────────┐
│   Lexer     │───▶│   Parser    │───▶│  Semantic    │───▶│     TAC      │
│ (Tokenizer) │    │ (AST Build) │    │  Analyzer    │    │  Generator   │
└─────────────┘    └─────────────┘    └──────────────┘    └──────────────┘
   source code         AST                AST (valid)         daftar TAC
```

### 3.1 Tahap Analisis Leksikal — kelas `Lexer`

Kelas `Lexer` membaca *source code* karakter demi karakter dan mengelompokkannya menjadi token dengan tipe berikut:

| Tipe Token   | Contoh          |
|--------------|-----------------|
| `IF`, `ELSE` | `if`, `else`    |
| `IDENT`      | `x`, `y`, `a`   |
| `NUMBER`     | `5`, `1`, `0`   |
| `RELOP`      | `>`, `==`, `<=` |
| `ASSIGN`     | `=`             |
| `PLUS`/`MINUS`/`STAR`/`SLASH` | `+ - * /` |
| `LPAREN`/`RPAREN`/`LBRACE`/`RBRACE`/`SEMICOLON` | `( ) { } ;` |

Proses ini mengabaikan *whitespace*, mengenali kata kunci (`if`, `else`) secara khusus dibanding identifier biasa, serta menangani operator satu atau dua karakter (misalnya `=` vs `==`, `<` vs `<=`).

### 3.2 Tahap Analisis Sintaksis — kelas `Parser`

Parser menggunakan teknik **Recursive Descent Parsing**, di mana setiap aturan grammar (`<if_stmt>`, `<condition>`, `<expr>`, `<term>`, `<factor>`, dst.) diimplementasikan sebagai satu fungsi (`parseIfElse()`, `parseCondition()`, `parseExpr()`, `parseTerm()`, `parseFactor()`).

Hasil akhirnya adalah **Abstract Syntax Tree (AST)** yang direpresentasikan oleh struct:

- `ExprNode` — node ekspresi (leaf `IDENT`/`NUMBER`, atau `BINOP` untuk operasi biner)
- `ConditionNode` — menyimpan ekspresi kiri, operator relasional, dan ekspresi kanan
- `AssignStmt` — pasangan variabel dan ekspresi hasil assignment
- `IfElseNode` — node akar berisi kondisi, blok `then`, dan blok `else`

Fungsi `printAST()` mencetak struktur pohon ini secara berjenjang (indented) agar mudah dibaca.

**Contoh AST** untuk `if (a + b > c * 2) { z = a + b; } else { z = c - a; }`:

```
IfElseNode
|- Condition (>)
|  |- Left:  BinOp(+) -> Ident(a), Ident(b)
|  |- Right: BinOp(*) -> Ident(c), Number(2)
|- Then Block:
|  |- Assign(z) -> BinOp(+) -> Ident(a), Ident(b)
|- Else Block:
|  |- Assign(z) -> BinOp(-) -> Ident(c), Ident(a)
```

### 3.3 Tahap Analisis Semantik — kelas `SemanticAnalyzer`

Analisis semantik melakukan dua jenis pengecekan dasar dengan menelusuri AST:

1. **Validasi keberadaan variabel (symbol table sederhana)**
   Sebuah `std::map<std::string, bool>` digunakan sebagai *symbol table*. Variabel yang muncul di sisi kiri assignment (`z = ...`) dianggap "dideklarasikan" secara implisit. Jika sebuah identifier dipakai (baik dalam kondisi maupun sisi kanan ekspresi) sebelum pernah di-assign atau didaftarkan sebagai variabel pra-deklarasi, program akan melempar `Semantic Error`.

2. **Deteksi pembagian dengan nol secara statis**
   Jika ditemukan pola `<expr> / 0` (angka literal nol sebagai pembagi), analyzer akan menghentikan proses dan melaporkan error, mensimulasikan pengecekan semantik seperti pada compiler sungguhan.

### 3.4 Tahap Generasi Kode Antara — kelas `TACGenerator`

TAC dihasilkan dengan menelusuri AST secara rekursif melalui fungsi `genExpr()`:

- Jika node adalah `IDENT` atau `NUMBER`, nilainya langsung dikembalikan (tidak perlu variabel sementara).
- Jika node adalah `BINOP`, maka operand kiri dan kanan dihitung lebih dahulu (rekursif), lalu dibuat instruksi baru dengan variabel sementara (`t1`, `t2`, ...) yang menyimpan hasil operasi tersebut.

Untuk struktur `if-else` secara keseluruhan, pola TAC yang dihasilkan mengikuti skema standar generasi kode untuk percabangan:

```text
<hitung kondisi ke temp/variabel>
ifFalse <kondisi> goto Lelse
<kode blok then>
goto Lend
Lelse:
<kode blok else>
Lend:
```

Dua *label* (`L1`, `L2`, dst.) dipakai untuk menandai titik lompatan blok `else` dan titik akhir percabangan, sesuai dengan teknik *backpatching* label yang umum dipakai pada tahap code generation compiler.

---

## 4. Contoh Eksekusi Program

### Contoh 1 — Kondisi sederhana

**Input:**
```c
if (x > 5) { y = 1; } else { y = 0; }
```

**Token (Leksikal):**
```
<IF,"if"> <LPAREN,"("> <IDENT,"x"> <RELOP,">"> <NUMBER,"5"> <RPAREN,")">
<LBRACE,"{"> <IDENT,"y"> <ASSIGN,"="> <NUMBER,"1"> <SEMICOLON,";"> <RBRACE,"}">
<ELSE,"else"> <LBRACE,"{"> <IDENT,"y"> <ASSIGN,"="> <NUMBER,"0"> <SEMICOLON,";"> <RBRACE,"}">
```

**Semantik:** `[Semantik OK]` — variabel `x` sudah dideklarasikan sebelumnya.

**Three-Address Code:**
```
ifFalse x > 5 goto L1
y = 1
goto L2
L1:
y = 0
L2:
```

### Contoh 2 — Ekspresi kompleks (menunjukkan penggunaan temporary variable)

**Input:**
```c
if (a + b > c * 2) { z = a + b; } else { z = c - a; }
```

**Three-Address Code:**
```
t1 = a + b
t2 = c * 2
ifFalse t1 > t2 goto L1
t3 = a + b
z = t3
goto L2
L1:
t4 = c - a
z = t4
L2:
```

Contoh ini memperlihatkan bagaimana ekspresi aritmatika `a + b` dan `c * 2` pada kondisi dipecah menjadi instruksi tiga-alamat (`t1`, `t2`) sebelum dibandingkan, sesuai prinsip TAC bahwa setiap instruksi hanya boleh memiliki maksimal satu operator.

### Contoh 3 — Demonstrasi Error Semantik

**Input:**
```c
if (m > 1) { n = 1; } else { n = 0; }
```
(dengan asumsi variabel `m` **belum pernah** dideklarasikan sebelumnya)

**Output:**
```
[GAGAL] Semantic Error: variabel 'm' digunakan sebelum dideklarasikan/di-assign.
```

Ini membuktikan bahwa tahap semantik benar-benar melakukan validasi, bukan sekadar meneruskan AST tanpa pengecekan.

---

## 5. Cara Menjalankan Program

```bash
g++ -std=c++17 ifelse_compiler.cpp -o ifelse_compiler
./ifelse_compiler
```

Program akan otomatis menjalankan ketiga contoh di atas secara berurutan melalui fungsi `main()`, masing-masing menampilkan keempat tahap kompilasi (Leksikal → Sintaksis/AST → Semantik → TAC).

---

## 6. Kesimpulan

Implementasi ini berhasil mensimulasikan pipeline kompilasi klasik untuk konstruksi `if-else`:

1. **Lexer** memecah source code menjadi token berdasarkan pola karakter.
2. **Parser** (recursive descent) membentuk AST sesuai grammar BNF yang mendukung ekspresi aritmatika bersarang.
3. **Semantic Analyzer** memvalidasi keberadaan variabel melalui symbol table sederhana dan mendeteksi pembagian dengan nol.
4. **TAC Generator** menerjemahkan AST menjadi instruksi tiga-alamat lengkap dengan variabel sementara dan label lompatan, mengikuti pola standar code generation untuk struktur percabangan pada compiler sungguhan.
