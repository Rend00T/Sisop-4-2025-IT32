#define FUSE_USE_VERSION 35

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>

#define ZIP_URL "https://drive.google.com/uc?export=download&id=1hi_GDdP51Kn2JJMw02WmCOxuc3qrXzh5"
#define ZIP_NAME "anomali.zip"
#define WORKING_DIR "anomali"
#define IMAGE_DIR "anomali/image"
#define LOG_FILE_PATH "anomali/conversion.log"

static char source_dir[512];

void create_directory(const char *dirname) {
    struct stat st = {0};
    if (stat(dirname, &st) == -1) {
        mkdir(dirname, 0700);
    }
}

void download_zip() {
    char command[512];
    snprintf(command, sizeof(command), "wget \"%s\" -O %s", ZIP_URL, ZIP_NAME);
    system(command);
}

void extract_zip() {
    char command[256];
    snprintf(command, sizeof(command), "unzip -o -q %s -d .", ZIP_NAME);
    system(command);
}

void delete_zip() {
    remove(ZIP_NAME);
}

unsigned char parse_byte(char high, char low) {
    char byte_str[3] = {high, low, '\0'};
    unsigned int byte_val;
    sscanf(byte_str, "%02x", &byte_val);
    return (unsigned char)byte_val;
}

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

static int fuse_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", source_dir, path);
    return lstat(full_path, stbuf);
}

static int fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi,
                        enum fuse_readdir_flags flags) {
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", source_dir, path);

    DIR *dp = opendir(full_path);
    if (!dp) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st = {0};
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, 0)) break;
    }

    closedir(dp);
    return 0;
}

static int fuse_open(const char *path, struct fuse_file_info *fi) {
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", source_dir, path);

    int res = open(full_path, fi->flags);
    if (res == -1) return -errno;
    close(res);

    const char *filename = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
    if (strstr(filename, ".txt")) {
        convert_hex_to_image(full_path, filename);
    }

    return 0;
}

static int fuse_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi) {
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", source_dir, path);
    int fd = open(full_path, O_RDONLY);
    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;

    close(fd);
    return res;
}

static const struct fuse_operations ops = {
    .getattr = fuse_getattr,
    .readdir = fuse_readdir,
    .open    = fuse_open,
    .read    = fuse_read,
};

int main(int argc, char *argv[]) {
    create_directory(WORKING_DIR);
    create_directory(IMAGE_DIR);

    if (access("anomali/1.txt", F_OK) != 0) {
        download_zip();
        extract_zip();
        delete_zip();
    }

    if (realpath(WORKING_DIR, source_dir) == NULL) {
        perror("realpath");
        exit(EXIT_FAILURE);
    }

    char *fuse_argv[] = { argv[0], argv[1], "-o", "allow_other" };
    return fuse_main(4, fuse_argv, &ops, NULL);
}
