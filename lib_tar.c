#include "lib_tar.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <dirent.h>

int ceilC(double val){
    if (val == 0.) {return 0;}
    if (val / (int) val != 1){ val++; }
    return (int) val;
}


unsigned long hash(char *str) {
    char *ptr = &str[0];
    int hash = 0;
    for(int i = 0; i < strlen(str); i++) {
        hash = ((hash << 5) + hash) + *ptr; /* hash * 33 + c */
        ptr++;
    }
    return hash;
}


void reset(int tar_fd) {
    lseek(tar_fd, 0, SEEK_SET);
}


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
    long checksum_calculated;
    long checksum_readed;
    int blocks_skip;
    int i;

    while (1){
        err = read(tar_fd, buffer, 512);
        if (err == -1) { reset(tar_fd); return -4; } // error on reading
        if (err < 512 && err > -1) { reset(tar_fd); return nb_headers; } // end of the file
        if (buffer[0] == 0) {reset(tar_fd); return nb_headers;}
        if (strncmp(&buffer[257], TMAGIC, TMAGLEN)) {reset(tar_fd); return -1;} // check magic value
        if (strncmp(&buffer[263], TVERSION, TVERSLEN)) {reset(tar_fd); return -2;}

        nb_headers++;

        checksum_calculated = 0;
        for (i = 0; i < 148; i++) {
            checksum_calculated += buffer[i];
        }
        checksum_calculated +=  8*' ';
        for (i = 156; i < 512; i++) {
            checksum_calculated += buffer[i];
        }
        checksum_readed = strtol(&buffer[148], NULL, 8);
        if (checksum_calculated != checksum_readed) {reset(tar_fd); return -3;}

        blocks_skip = ceilC(strtol(&buffer[124], NULL, 8) / 512.);
        lseek(tar_fd, (off_t) blocks_skip * 512, SEEK_CUR);
    }
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
    unsigned long path_name = hash(path);
    unsigned long current_name;
    char buffer[512];
    int err;
    int blocks_skip;

    while(1) {
        err = read(tar_fd, buffer, 512);
        if (err == -1) {reset(tar_fd); return -1; } // error on reading
        if (buffer[0] == '\0') {reset(tar_fd); return 0;}
        if (err < 512 && err > -1) { reset(tar_fd); return 0; } // end of the file

        current_name = hash(&buffer[0]);
        if (current_name == path_name) {reset(tar_fd); return 1;}
        blocks_skip = ceilC(strtol(&buffer[124], NULL, 8) / 512.);
        lseek(tar_fd, (off_t) blocks_skip * 512, SEEK_CUR);
    }
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
    unsigned long path_name = hash(path);
    unsigned long current_name;
    char buffer[512];
    int err;
    int blocks_skip;

    while(1) {
        err = read(tar_fd, buffer, 512);
        if (err == -1) {reset(tar_fd); return -1; } // error on reading
        if (buffer[0] == '\0') {reset(tar_fd); return 0;}
        if (err < 512 && err > -1) { reset(tar_fd); return 0; } // end of the file

        current_name = hash(&buffer[0]);
        if (current_name == path_name && buffer[156] == DIRTYPE) {reset(tar_fd); return 1;}
        blocks_skip = ceilC(strtol(&buffer[124], NULL, 8) / 512.);
        lseek(tar_fd, (off_t) blocks_skip * 512, SEEK_CUR);
    };
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
    unsigned long path_name = hash(path);
    unsigned long current_name;
    char buffer[512];
    int err;
    int blocks_skip;

    while(1) {
        err = read(tar_fd, buffer, 512);
        if (err == -1) {reset(tar_fd); return -1; } // error on reading
        if (buffer[0] == '\0') {reset(tar_fd); return 0;}
        if (err < 512 && err > -1) { reset(tar_fd); return 0; } // end of the file

        current_name = hash(&buffer[0]);
        if (current_name == path_name && (buffer[156] == REGTYPE || buffer[156] == AREGTYPE)) {reset(tar_fd); return 1;}
        blocks_skip = ceilC(strtol(&buffer[124], NULL, 8) / 512.);
        lseek(tar_fd, (off_t) blocks_skip * 512, SEEK_CUR);
    };
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
    unsigned long path_name = hash(path);
    unsigned long current_name;
    char buffer[512];
    int err;
    int blocks_skip;

    while(1) {
        err = read(tar_fd, buffer, 512);
        if (err == -1) {reset(tar_fd); return -1; } // error on reading
        if (buffer[0] == '\0') {reset(tar_fd); return 0;}
        if (err < 512 && err > -1) { reset(tar_fd); return 0; } // end of the file

        current_name = hash(&buffer[0]);
        if (current_name == path_name && buffer[156] == SYMTYPE) {reset(tar_fd); return 1;} // replace by '1' for hard link
        blocks_skip = ceilC(strtol(&buffer[124], NULL, 8) / 512.);
        lseek(tar_fd, (off_t) blocks_skip * 512, SEEK_CUR);
    };
}


// return 0 if it is a subdir, 1 if not
int notinsubdir(char *filename){
    int i=0;
    while(filename[i] != '\0'){
        if (filename[i] == '/' && filename[i+1] != '\0'){ return 0; }
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
    uint16_t blocks_skip;
    size_t max = *no_entries;
    int size = strlen(path);
    int err;

    size_t found = 0;
    while (max > found){
        err = read(tar_fd, buffer, 512);
        if (err == -1){ printf("Error while reading\n"); return 0; }
        // end of the file
        if (buffer[0] == '\0') {
            reset(tar_fd); 
            *no_entries = found;
            return found; }
        if (err < 512 && err > -1) { 
            reset(tar_fd);
            *no_entries = found;
            return found; }
        
        // Si le fichier est (dans) le dossier
        if (!strncmp(path, &buffer[0], size)){
            // Si le fichier n'est pas DANS un sous-dossier
            if (notinsubdir(&buffer[size])) {
                // Si symlink
                if (buffer[156] == SYMTYPE){
                    strncpy(entries[found], &buffer[157], 100); }
                // Si file ou whatever
                else { strncpy(entries[found], &buffer[0], 100); }
                // Anyway
                found++;
            }
        }
        // Si quelque chose a été copié, c'est qu'on a finit de parcourir les fichiers intéressants
        else if (found > 0) {
            *no_entries = found;
            return 1;
        }

        // next file
        blocks_skip = ceilC(strtol(&buffer[124], NULL, 8) / 512.);
        lseek(tar_fd, (off_t) blocks_skip * 512, SEEK_CUR);
    }

    *no_entries = found;
    return 1;
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
ssize_t read_file(int tar_fd, char *path, size_t offset, uint8_t *dest, size_t *len) { // todo : if sym
    unsigned long path_name = hash(path);
    unsigned long current_name;
    char buffer[512];
    int err;
    int size;
    int blocks_skip;
    int out;

    while(1) {
        err = read(tar_fd, buffer, 512);
        if (err == -1) {reset(tar_fd); return -3; } // error on reading
        if (buffer[0] == '\0') {reset(tar_fd); *len = 0; return -1;}
        if (err < 512 && err > -1) {reset(tar_fd); *len = 0; return -1; } // end of the file

        current_name = hash(&buffer[0]);
        size = strtol(&buffer[124], NULL, 8);
        if (current_name == path_name && (
            buffer[156] == REGTYPE ||
            buffer[156] == AREGTYPE ||
            buffer[156] == SYMTYPE
        )) {

            if (buffer[156] == SYMTYPE) {
                printf("Sym link detected !\n");
                reset(tar_fd);
                return read_file(tar_fd, &buffer[157], offset, dest, len);
            }

            if (offset >= size) {reset(tar_fd); *len = 0; return -2;}
            out = *len < size ? size - (*len + offset) : 0;
            out = out < 1 ? 0 : out;

            *len  = offset + *len > size ? size - offset : *len;
            lseek(tar_fd, (off_t) offset, SEEK_CUR); // start at the offset
            *len = read(tar_fd, dest, *len); // put bytes in dest

            reset(tar_fd);
            return out;
        }
        blocks_skip = ceilC(size / 512.);
        lseek(tar_fd, (off_t) blocks_skip * 512, SEEK_CUR);
    }
}
