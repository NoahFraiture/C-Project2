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

/**
 * Checks whether an entry exists in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive,
 *         any other value otherwise.
 */
int exists(int tar_fd, char *path) {
    char *command = "tar -cvf temp.tar ";
    strcat(command, path);
    system(command);

    int fd = open("temp.tar", O_RDONLY);
    char buffer[512];
    int size = 0;
    int hash_path = 0;
    int err;
    while (1) {
        err = read(fd, buffer, 512);
        if (err == -1) {printf("Error with path reading"); return -1;}
        if (!err) {break;}
        size++;
        // hash_path += hash(buffer)
    }

    int current_hash = 0;
    int hash_per_bloc[size];
    int index = 0;
    while (1) {
        err = read(tar_fd, buffer, 512);
        if (err == -1) {printf("Error with tar reading"); return -1;}
        if (!err) {break;};
        index = (index+1) % size;
        // hash_per_bloc[index] = hash(buffer);
        // current_hash = actualize_hash(hash_per_bloc);
        // if current_hash == hash_path: return 1
    }
    system("rm temp.tar");
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
    DIR *dir = opendir(path);
    return exists(tar_fd, path) & dir;
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
    return (exists(tar_fd, path) == 1) & (access(path, F_OK) == 0)
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
    return 0; // little adaptation of exists()
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
    return 0;
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
