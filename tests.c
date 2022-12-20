#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "lib_tar.h"

/**
 * You are free to use this file to write tests for your implementation
 */

void debug_dump(const uint8_t *bytes, size_t len) {
    for (int i = 0; i < len;) {
        printf("%04x:  ", (int) i);

        for (int j = 0; j < 16 && i + j < len; j++) {
            printf("%02x ", bytes[i + j]);
        }
        printf("\t");
        for (int j = 0; j < 16 && i < len; j++, i++) {
            printf("%c ", bytes[i]);
        }
        printf("\n");
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s tar_file\n", argv[0]);
        return -1;
    }

    int fd = open(argv[1] , O_RDONLY);
    if (fd == -1) {
        perror("open(tar_file)");
        return -1;
    }

    // ========== BASIC TESTING ==========
    // Check_archive & exists
    int ret = check_archive(fd);
    printf("check_archive returned %d\n", ret);
    int exst = exists(fd, "lib_tar.h");
    if (exst) {
        printf("Exists : lib_tar.h found !\n");
    } else {
        printf("Exists : lib_tar.h not found :(\n");
    }

    // ========== FILE NATURE TESTING ==========
    // Is file
    int file1 = is_file(fd, "lib_tar.h");
    if (file1) {printf("File well detected !\n");} else {printf("File not detected :(\n");}
    int file2 = is_file(fd, "folder/");
    if (!file2) {printf("File well not detected !\n");} else {printf("File wrongly detected :(\n");}

    // Is dir
    int folder1 = is_dir(fd, "lib_tar.h");
    if (!folder1) {printf("Folder well not detected !\n");} else {printf("Folder wrongly detected :(\n");}
    int folder2 = is_dir(fd, "test_yey/");
    if (folder2) {printf("Folder well detected !\n");} else {printf("Folder not detected :(\n");}

    // Is symlink
    int symfile1 = is_symlink(fd, "lib_link.c");
    if (symfile1) {printf("Link well detected !\n");} else {printf("Link not detected :(\n");}
    int symfile2 = is_symlink(fd, "lib_tar.h");
    if (!symfile2) {printf("Link well not detected !\n");} else {printf("Link wrongly detected :(\n");}

    // ========== LIST TESTING ==========
    // List -- good (folder)
    size_t no_entries = 10;
    char *entries[10]; for (int i=0; i<10; i++){ entries[i] = malloc(sizeof(char)*100);}
    int listresult = list(fd, "coucoucfolder/", (char **)entries, &no_entries);
    if (listresult) {
        printf("List well built ! (%d)\n", (int)no_entries);
        for (size_t en=0; en<no_entries; en++){
            //printf("--> %s\n", entries[en]);
        }
    } else {printf("List not built :(\n");}

    // List -- good (main folder)
    no_entries = 10;
    listresult = list(fd, "", (char **)entries, &no_entries);
    if (listresult) {
        printf("List well built ! (%d)\n", (int)no_entries);
        for (size_t en=0; en<no_entries; en++){
            //printf("--> %s\n", entries[en]);
        }
    } else {printf("List not built :(\n");}

    // List -- good (folder without '/')
    no_entries = 10;
    listresult = list(fd, "test_yey", (char **)entries, &no_entries);
    if (listresult) {
        printf("List well built ! (%d)\n", (int)no_entries);
        for (size_t en=0; en<no_entries; en++){
            //printf("--> %s\n", entries[en]);
        }
    } else {printf("List not built :(\n");}
    
    // List -- good (symlink)
    no_entries = 10;
    listresult = list(fd, "coucouclink", (char **)entries, &no_entries);
    if (listresult) {
        printf("List well built ! (%d)\n", (int)no_entries);
        for (size_t en=0; en<no_entries; en++){
            //printf("--> %s\n", entries[en]);
        }
    } else {printf("List not built :(\n");}

    // List -- good (symlink to symlink)
    no_entries = 10;
    listresult = list(fd, "coucouclinklink", (char **)entries, &no_entries);
    if (listresult) {
        printf("List well built ! (%d)\n", (int)no_entries);
        for (size_t en=0; en<no_entries; en++){
            //printf("--> %s\n", entries[en]);
        }
    } else {printf("List not built :(\n");}

    // List -- bad (dir doesn't exist)
    no_entries = 10;
    listresult = list(fd, "notarealdir/", (char **)entries, &no_entries);
    if (!listresult) {printf("List well not built !\n");} else {
        printf("List wrongly built (%d) :(\n", (int) no_entries);
        for (size_t en=0; en<no_entries; en++){
            printf("--> %s\n", entries[en]);
        }
    }

    // List -- bad (this is a file, not a dir)
    no_entries = 10;
    listresult = list(fd, "lib_tar.c", (char **)entries, &no_entries);
    if (!listresult) {printf("List well not built !\n");} else {
        printf("List wrongly built (%d) :(\n", (int) no_entries);
        for (size_t en=0; en<no_entries; en++){
            printf("--> %s\n", entries[en]);
        }
    }

    // List -- bad (symlink to a file)
    no_entries = 10;
    listresult = list(fd, "lib_link.c", (char **)entries, &no_entries);
    if (!listresult) {printf("List well not built !\n");} else {
        printf("List wrongly built (%d) :(\n", (int) no_entries);
        for (size_t en=0; en<no_entries; en++){
            printf("--> %s\n", entries[en]);
        }
    }
    
    // ========== READ TESTING ==========
    // Read file
    printf("\n");
    char *path = "folder/bidu.txt";
    size_t offset = 28;
    uint8_t dest[10];
    size_t len = 10;

    printf("File started at %ld, len of dest : %ld\n", offset, len);
    ssize_t read_res = read_file(fd, path, offset, dest, &len);
    printf("Remaining content of the file/return : %ld\n", read_res);
    printf("Content of the file : %s\n", dest);
    printf("Number written bytes/len : %ld\n", len);

    return 0;
}