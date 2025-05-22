# Sisop-4-2025-IT32
- Ananda Fitri Wibowo (5027241057)
- Raihan Fahri Ghazali (5027241061)

| No | Nama                   | NRP         |
|----|------------------------|-------------|
| 1  | Ananda Fitri Wibowo    | 5027241057  |
| 2  | Raihan Fahri Ghazali   | 5027241061  |

# Soal 1
Solverd by. 061_Raihan Fahri Ghazali

## A. Unduh dan Ekstrasi File ZIP Anomali 
Pada modul kali ini kita diharuskan untuk menggunakan FUSE dan harus :
- Mendownload file
- zip file
- unzip file

### Download Zip  File
Fungsi ini digunakan untuk mengunduh file dari link Google Drive yang telah disediakan, dengan menggunakan perintah "wget" yang dijalankan melalui shell.

```
void download_zip() {
    char command[512];
    snprintf(command, sizeof(command), "wget \"%s\" -O %s", ZIP_URL, ZIP_NAME);
    system(command);
}
```

### Ekstrak Zip File
Setelah file sudah ter download. Seluruh file beserta semua isinya di ekstrak dan menghasilkan satu file zip terbaru. 
```
void extract_zip() {
    char command[256];
    snprintf(command, sizeof(command), "unzip -o -q %s -d .", ZIP_NAME);
    system(command);
}
```

### Hapus zip File 
Bagian ini digunakan untuk menghapus file Zip setelah proses ektraksi selesai, sehingga hanya menyisakan hasil ekstraknya saja.
```
void delete_zip() {
    remove(ZIP_NAME);
}
```

Semua fungsi ini dipanggil di dalam fungsi main
```
if (access("anomali/1.txt", F_OK) != 0) {
    download_zip();
    extract_zip();
    delete_zip();
}
```

## B. Detect format and conver Hexadecimal to Image
Sekarang kita diharuskan mengubah format teks awal yang awalnya adalah String Hexadecimal dan mengubahnya menjadi file image. 


### Function string Hex to Image 
Fungsi ini awalannya membuka file .txt berisi string hex lalu mengambil 2 karakter sekaligus.
Mengubah 2 karakter hexadesimal menjadi 1 byte dan menghasilkan file output PNG. Lalu mengakhirinya dengan menutup file .txt 
```
void convert_hex_to_image(const char *abs_input_path, const char *filename) {
    FILE *in = fopen(abs_input_path, "r");
    if (!in) return;

    create_directory(IMAGE_DIR);

    time_t raw_time = time(NULL);
    struct tm *timeinfo = localtime(&raw_time);

    char tanggal[16], waktu_file[16], waktu_log[16];
    strftime(tanggal, sizeof(tanggal), "%Y-%m-%d", timeinfo);
    strftime(waktu_file, sizeof(waktu_file), "%H-%M-%S", timeinfo); // untuk filename
    strftime(waktu_log, sizeof(waktu_log), "%H:%M:%S", timeinfo);   // untuk log

    char base_name[128];
    strncpy(base_name, filename, sizeof(base_name));
    char *dot = strrchr(base_name, '.');
    if (dot) *dot = '\0';

    char output_file[512];
    snprintf(output_file, sizeof(output_file), "%s/%s_image_%s_%s.png", IMAGE_DIR, base_name, tanggal, waktu_file);

    FILE *out = fopen(output_file, "wb");
    if (!out) {
        fclose(in);
        return;
    }

    int c1, c2;
    while ((c1 = fgetc(in)) != EOF && (c2 = fgetc(in)) != EOF) {
        if (!isxdigit(c1) || !isxdigit(c2)) continue;
        unsigned char byte = parse_byte((char)c1, (char)c2);
        fwrite(&byte, sizeof(unsigned char), 1, out);
    }

    fclose(in);
    fclose(out);

    FILE *log = fopen(LOG_FILE_PATH, "a");
    if (log) {
        fprintf(log, "[%s][%s]: Successfully converted hexadecimal text %s to %s.\n",
                tanggal, waktu_log, filename, strrchr(output_file, '/') + 1);
        fclose(log);
    }
}
```
Fuse akan memanggil "fuse_open()" saat pengguna membuka file di mount point
```
static int fuse_open(const char *path, struct fuse_file_info *fi) {
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", source_dir, path);

    int res = open(full_path, fi->flags);
    if (res == -1) return -errno;
    close(res);

    // ✅ Inilah bagian yang memicu konversi hexadecimal → image
    const char *filename = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
    if (strstr(filename, ".txt")) {
        convert_hex_to_image(full_path, filename);  // ← dipanggil di sini!
    }

    return 0;
}
```

## C. Naming the Converted Image File 
Bagian ini mengatur penamaan file hasil konversi dari string hexadecimal menjadi gambar, yang harus mengikuti format yang telah ditetapkan agar konsisten dan mudah dilacak.

### Membaca jenis file Image
Code pada bagian ini bertugas untuk membaca dan memproses penamaan file hasil konversi gambar.


```
char output_file[512];
snprintf(output_file, sizeof(output_file), "%s/%s_image_%s_%s.png",
         IMAGE_DIR, base_name, tanggal, waktu_file);
```


### Menyesuaikan format tanggal dan waktu 
Bagian kode ini digunakan untuk mengambil tanggal dan waktu saat ini, lalu memformatnya sesuai dengan format yang diinginkan untuk keperluan penamaan file atau pencatatan log.


```
strftime(tanggal, sizeof(tanggal), "%Y-%m-%d", timeinfo);
strftime(waktu_file, sizeof(waktu_file), "%H-%M-%S", timeinfo);  // ← untuk nama file
```


### Menggambungkan semua 
Code pada bagian ini bertugas untuk menggabungkan elemen-elemen yang dibutuhkan, yaitu nama file string, tanggal, bulan, tahun, serta waktu saat proses dijalankan, menjadi satu format penamaan yang sesuai.
```
snprintf(output_file, sizeof(output_file), "%s/%s_image_%s_%s.png", IMAGE_DIR, base_name, tanggal, waktu_file);
```


## D. Make conversion.log

### Library path log
Berikut adalah library yang digunakan untuk mendefinisikan path log dan memastikan program dapat dijalankan dengan benar.
```
#define LOG_FILE_PATH "anomali/conversion.log"
```

### Membuat conversion.log 
Diharapkan isi dari file conversion.log mengikuti format yang telah ditentukan, seperti contoh berikut:
```
[2025-05-11][18:35:26]: Successfully converted hexadecimal text 1.txt to 1_image_2025-05-11_18:35:26.png.
```

Bagian kode ini bertanggung jawab untuk membuat dan mengisi file conversion.log, yang mencatat aktivitas konversi secara otomatis dengan format log yang sesuai instruksi soal.

```
FILE *log = fopen(LOG_FILE_PATH, "a");  // ← Membuka atau membuat conversion.log
if (log) {
    fprintf(log, "[%s][%s]: Successfully converted hexadecimal text %s to %s.\n",
            tanggal, waktu_log, filename, strrchr(output_file, '/') + 1);  // ← Menulis isi log
    fclose(log);  // ← Menutup file log
}

```









