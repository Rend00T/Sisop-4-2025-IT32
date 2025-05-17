#define FUSE_USE_VERSION 28
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define SOURCE_DIR "anomali"
#define IMAGE_DIR "image"
#define LOG_FILE "conversion.log"

unsigned char parse_byte(char high, char low) {
    char byte_str[3] = {high, low, '\0'};
    unsigned int val;
    sscanf(byte_str, "%02x", &val);
    return (unsigned char)val;
}

void create_directory(const char *dir) {
    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        mkdir(dir, 0755);
    }
}

void log_conversion(const char *txt_name, const char *img_name) {
    FILE *log = fopen(LOG_FILE, "a");
    if (!log) return;

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    fprintf(log, "[%04d-%02d-%02d][%02d:%02d:%02d]: Successfully converted hexadecimal text %s to %s.\n",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec,
            txt_name, img_name);
    fclose(log);
}

static int hexfs_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    if (strstr(path, ".jpg")) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = 1024 * 50; // approx size
        return 0;
    }

    return -ENOENT;
}

static int hexfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    if (strcmp(path, "/") != 0) return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    DIR *dp = opendir(SOURCE_DIR);
    if (!dp) return -ENOENT;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strstr(de->d_name, ".txt")) {
            char name[256];
            snprintf(name, sizeof(name), "%.*s.jpg", (int)(strlen(de->d_name) - 4), de->d_name);
            filler(buf, name, NULL, 0);
        }
    }

    closedir(dp);
    return 0;
}

static int hexfs_open(const char *path, struct fuse_file_info *fi) {
    if (!strstr(path, ".jpg")) return -ENOENT;
    return 0;
}

static int hexfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    (void) fi;

    char name[256];
    if (sscanf(path, "/%[^.].jpg", name) != 1) return -ENOENT;

    char txt_path[512];
    snprintf(txt_path, sizeof(txt_path), "%s/%s.txt", SOURCE_DIR, name);

    FILE *file = fopen(txt_path, "r");
    if (!file) return -ENOENT;

    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    rewind(file);

    char *hex = malloc(fsize + 1);
    fread(hex, 1, fsize, file);
    fclose(file);
    hex[fsize] = '\0';

    char *clean = malloc(fsize + 1);
    int j = 0;
    for (int i = 0; i < fsize; ++i)
        if (hex[i] != '\n' && hex[i] != '\r')
            clean[j++] = hex[i];
    clean[j] = '\0';

    char *data = malloc(j / 2);
    size_t data_len = 0;
    for (int i = 0; i < j - 1; i += 2)
        data[data_len++] = parse_byte(clean[i], clean[i + 1]);

    create_directory(IMAGE_DIR);

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    char img_name[512];
    snprintf(img_name, sizeof(img_name), "%s_image_%04d-%02d-%02d_%02d-%02d-%02d.png",
             name, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);

    char img_path[1024];
    snprintf(img_path, sizeof(img_path), "%s/%s", IMAGE_DIR, img_name);

    FILE *out = fopen(img_path, "wb");
    if (out) {
        fwrite(data, 1, data_len, out);
        fclose(out);
        log_conversion(name, img_name);
    }

    if (offset >= data_len) {
        free(hex); free(clean); free(data);
        return 0;
    }

    if (offset + size > data_len)
        size = data_len - offset;

    memcpy(buf, data + offset, size);
    free(hex); free(clean); free(data);
    return size;
}

static struct fuse_operations hexfs_oper = {
    .getattr = hexfs_getattr,
    .readdir = hexfs_readdir,
    .open = hexfs_open,
    .read = hexfs_read,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &hexfs_oper, NULL);
}