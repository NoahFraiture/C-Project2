#include "lib_tar.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <dirent.h>

/**
 * Checks whether the archive is valid.
 *
 * Each non-null header of a valid archive has:
 *  - a magic value of "ustar" and a null,
 *  - a version value of "00" and no null,
 *  - a correct checksum
 *
 * @param tar_fd A file descriptor pointing to the start of a file supposed to contain a tar archive.
 *
 * @return a zero or positive value if the archive is valid, representing the number of non-null headers in the archive,
 *         -1 if the archive contains a header with an invalid magic value,
 *         -2 if the archive contains a header with an invalid version value,
 *         -3 if the archive contains a header with an invalid checksum value
 */
int check_archive(int tar_fd) {
    char buffer[512];
    int nb_headers = 0;
    int err;
    uint8_t checksum;

    int blocks_skip;
    char *size_str;

    while (1){
        err = read(tar_fd, buffer, 512);
        if (err == -1) { return -4; } // error on reading
        
        nb_headers++;
        if (err == 0) { return nb_headers; } // end of the file
        if (strcmp(&buffer[267], "ustar\0") != 0){ return -1; } // check magic value
        if (buffer[263] != '0' || buffer[263] != '0'){ return -2; } // check version value
            
        // Test d'un tar avec un fichier vide :
        // Somme des bytes du header : 4836
        // Somme bytes checksum : 301
        // Checksum en octal :  4759
        // ça colle pas et jsp pourquoi
        checksum = *(uint8_t*)(&buffer[148]);

        // Get numbers of text blocks 
        // Need to be compile with -lm to works. Need investigation
        size_str = &buffer[124];
        blocks_skip = ceil(strtol(size_str, NULL, 8) / 512.);
        lseek(tar_fd, (off_t) blocks_skip * 512, SEEK_CUR);
    }

    return 0;
}

unsigned long hash(char *str) { // found on https://stackoverflow.com/questions/7666509/hash-function-for-string
    int c = str[0];
    int hash = 0;
    while (c != '\n') {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        c = *str++;
    }
    return hash;
}

int ceilC(double val){
    if (val / (int) val != 1){ val++; }
    return (int) val;
}

/**
 * Checks whether an entry exists in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive,
 *         any other value otherwise.
 */
// contains the full path as name, not only the name of the file. Is it what we want ? 
int exists(int tar_fd, char *path) {
    char *command = "tar -cvf temp.tar ";
    strcat(command, path);
    system(command);

    int fd = open("temp.tar", O_RDONLY);
    char buffer[512];
    unsigned long hash_path = 0;
    int err = read(fd, buffer, 512);
    if (err == -1) {printf("Error with path reading"); return -1;}
    hash_path = hash(buffer);

    int current_hash = 0;
    int flag = 0;

    int blocks_skip;
    char *size_str;
    while (!flag) {
        err = read(tar_fd, buffer, 512);
        if (err == -1) {printf("Error with tar reading"); return -1;}
        current_hash = hash(buffer);
        if (current_hash == hash_path) { flag = 1; }

        size_str = &buffer[124];
        blocks_skip = ceil(strtol(size_str, NULL, 8) / 512.);
        lseek(tar_fd, (off_t) blocks_skip * 512, SEEK_CUR);
    }
    system("rm temp.tar");
    return flag;
}

int check_is(int tar_fd, char *path, int type){
    char buffer[512]; // because it is structured by blocks of 512 bytes !
    uint16_t blockskip;
    int err;

    int end = 0;
    int found;
    while (!end){
        err = read(tar_fd, buffer, 512);
        if (err == 0){ end = 1; }
        
        if (strcmp(path, buffer[0]) == 0) {
            if (buffer[156] == type || (type == 0 && buffer[156] == '\0')) { return 1; }
            return 0;
        }

        // Get blockskip
        blockskip = ceilC((double) strtol(&buffer[124], NULL, 8) / 512.);
        lseek(tar_fd, (off_t)blockskip*512, SEEK_CUR);
    }

    return 0;
}

/**
 * Checks whether an entry exists in the archive and is a directory.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a directory,
 *         any other value otherwise.
 */
int is_dir(int tar_fd, char *path) {
    return check_is(tar_fd, path, 5);
}

/**
 * Checks whether an entry exists in the archive and is a file.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a file,
 *         any other value otherwise.
 */
int is_file(int tar_fd, char *path) {
    return check_is(tar_fd, path, 0);
}

/**
 * Checks whether an entry exists in the archive and is a symlink.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 * @return zero if no entry at the given path exists in the archive or the entry is not symlink,
 *         any other value otherwise.
 */
int is_symlink(int tar_fd, char *path) {
    return check_is(tar_fd, path, 1);
}


// return 0 if it is a subdir, 1 if not
int isnotsubdir(char *filename){
    int i=0;
    while(filename[i] != '\0'){
        if (filename[i] == "/"){ return 0; }
        i++;
    }
    return 1;
}

/**
 * Lists the entries at a given path in the archive.
 * list() does not recurse into the directories listed at the given path.
 *
 * Example:
 *  dir/          list(..., "dir/", ...) lists "dir/a", "dir/b", "dir/c/" and "dir/e/"
 *   ├── a
 *   ├── b
 *   ├── c/
 *   │   └── d
 *   └── e/
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive. If the entry is a symlink, it must be resolved to its linked-to entry.
 * @param entries An array of char arrays, each one is long enough to contain a tar entry path.
 * @param no_entries An in-out argument.
 *                   The caller set it to the number of entries in `entries`.
 *                   The callee set it to the number of entries listed.
 *
 * @return zero if no directory at the given path exists in the archive,
 *         any other value otherwise.
 */
int list(int tar_fd, char *path, char **entries, size_t *no_entries) {
    char buffer[512]; // because it is structured by blocks of 512 bytes !
    uint16_t blockskip;
    int err;

    int size = strlen(path);

    int count = 0;
    size_t found = 0;
    while (1){
        err = read(tar_fd, buffer, 512);
        if (err == 0){ return 0; }
        
        if (strcmp(path, buffer[0]) == 0) { found = 1; }
        else if (found){
            // Si le fichier est dans le dossier ET qu'il est pas (dans) un sous-dossier
            if (strncmp(path, buffer[0], size) && isnotsubdir(&buffer[size])) {
                // Si file
                if (buffer[156] == 0 || buffer[156] == '\0'){ strcpy(entries[found], buffer[0]); }
                // Si symlink
                else if (buffer[156] == 1){ strcpy(entries[found], buffer[157]); }
                found++;
            }
            // Sinon c'est qu'on a finit de parcourir les dossiers
            else {
                *no_entries = found;
                return 1; 
            }
        }

        // next file
        blockskip = ceilC((double) strtol(&buffer[124], NULL, 8) / 512.);
        lseek(tar_fd, (off_t)blockskip*512, SEEK_CUR);
    }

    *no_entries = found;
    return found;
}

/**
 * Reads a file at a given path in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive to read from.  If the entry is a symlink, it must be resolved to its linked-to entry.
 * @param offset An offset in the file from which to start reading from, zero indicates the start of the file.
 * @param dest A destination buffer to read the given file into.
 * @param len An in-out argument.
 *            The caller set it to the size of dest.
 *            The callee set it to the number of bytes written to dest.
 *
 * @return -1 if no entry at the given path exists in the archive or the entry is not a file,
 *         -2 if the offset is outside the file total length,
 *         zero if the file was read in its entirety into the destination buffer,
 *         a positive value if the file was partially read, representing the remaining bytes left to be read to reach
 *         the end of the file.
 *
 */
ssize_t read_file(int tar_fd, char *path, size_t offset, uint8_t *dest, size_t *len) {
    return 0;
}


int main() {
    return 0;
}