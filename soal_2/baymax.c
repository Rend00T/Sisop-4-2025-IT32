#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define MAX_FRAG_SIZE 1024
#define MAX_FRAG_COUNT 1024

#define ROOT_DIR "."
#define RELICS_DIR "relics"
#define LOG_FILE "activity.log"
#define TMP_DIR "/tmp/fuse_frag_"

static const char *baymax_name = "Baymax.jpeg";

void tulis_log(const char *pesan) {
    FILE *log = fopen(LOG_FILE, "a");
    if (!log) return;

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char waktu[64];
    strftime(waktu, sizeof(waktu), "%Y-%m-%d %H:%M:%S", tm);

    fprintf(log, "[%s] %s\n", waktu, pesan);
    fclose(log);
}

static int dapetin_info(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (strcmp(path + 1, baymax_name) == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        char fragpath[256];
        size_t total = 0;
        for (int i = 0;; i++) {
            snprintf(fragpath, sizeof(fragpath), "%s/%s.%03d", RELICS_DIR, baymax_name, i);
            FILE *f = fopen(fragpath, "rb");
            if (!f) break;
            fseek(f, 0, SEEK_END);
            total += ftell(f);
            fclose(f);
        }
        stbuf->st_size = total;
        stbuf->st_nlink = 1;
    } else {
        return -ENOENT;
    }

    return 0;
}

static int tampilkan_isi_dir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
                              struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, baymax_name, NULL, 0, 0);
    return 0;
}

static int baca_dalem(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path + 1, baymax_name) != 0) return -ENOENT;

    char fragpath[256];
    size_t total = 0;
    char *temp = malloc(MAX_FRAG_SIZE * MAX_FRAG_COUNT);
    if (!temp) return -ENOMEM;

    for (int i = 0;; i++) {
        snprintf(fragpath, sizeof(fragpath), "%s/%s.%03d", RELICS_DIR, baymax_name, i);
        FILE *f = fopen(fragpath, "rb");
        if (!f) break;
        size_t len = fread(temp + total, 1, MAX_FRAG_SIZE, f);
        total += len;
        fclose(f);
    }

    if (offset >= total) {
        free(temp);
        return 0;
    }

    if (offset + size > total) size = total - offset;
    memcpy(buf, temp + offset, size);
    free(temp);

    tulis_log("READ: Baymax.jpeg");
    return size;
}

static int buka_file(const char *path, struct fuse_file_info *fi) {
    return 0;
}

static int buat_file(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s%s", TMP_DIR, path + 1);
    FILE *f = fopen(tmp, "wb");
    if (!f) return -EACCES;
    fclose(f);
    return 0;
}

static int tulis_file(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s%s", TMP_DIR, path + 1);
    FILE *f = fopen(tmp, "ab");
    if (!f) return -EIO;
    fwrite(buf, 1, size, f);
    fclose(f);
    return size;
}

static int lepas_file(const char *path, struct fuse_file_info *fi) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s%s", TMP_DIR, path + 1);
    FILE *f = fopen(tmp, "rb");
    if (!f) return 0;

    int frag = 0;
    char buf[MAX_FRAG_SIZE];
    size_t len;
    char logmsg[1024] = {0};
    snprintf(logmsg, sizeof(logmsg), "WRITE: %s -> ", path + 1);

    while ((len = fread(buf, 1, MAX_FRAG_SIZE, f)) > 0) {
        char fragpath[256];
        snprintf(fragpath, sizeof(fragpath), "%s/%s.%03d", RELICS_DIR, path + 1, frag);
        FILE *out = fopen(fragpath, "wb");
        if (!out) break;
        fwrite(buf, 1, len, out);
        fclose(out);

        char fragname[64];
        snprintf(fragname, sizeof(fragname), "%s.%03d", path + 1, frag);
        strcat(logmsg, fragname);
        frag++;
        if (len == MAX_FRAG_SIZE) strcat(logmsg, ", ");
    }

    tulis_log(logmsg);
    fclose(f);
    remove(tmp);
    return 0;
}

static int hapus_file(const char *path) {
    const char *base = path + 1;
    int i = 0;
    char fragpath[256];
    while (1) {
        snprintf(fragpath, sizeof(fragpath), "%s/%s.%03d", RELICS_DIR, base, i);
        if (access(fragpath, F_OK) != 0) break;
        remove(fragpath);
        i++;
    }
    if (i > 0) {
        char logmsg[256];
        snprintf(logmsg, sizeof(logmsg), "DELETE: %s.000 - %s.%03d", base, base, i - 1);
        tulis_log(logmsg);
    }
    return 0;
}

static const struct fuse_operations ops = {
    .getattr    = dapetin_info,
    .readdir    = tampilkan_isi_dir,
    .open       = buka_file,
    .create     = buat_file,
    .read       = baca_dalem,
    .write      = tulis_file,
    .release    = lepas_file,
    .unlink     = hapus_file,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &ops, NULL);
}
