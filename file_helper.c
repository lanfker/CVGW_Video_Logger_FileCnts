#include <stdio.h>
#include <dirent.h>
#include <string.h>

void read_files (char* path, char subfolders[1024][50], int* cnt) {
    DIR *d; 
    struct dirent *dir; 
    d = opendir (path);
    *cnt = 0;
    if (d) {
        while ( (dir = readdir (d) ) != NULL) {
            if (strlen(dir->d_name) < 4) continue;
            strcpy (subfolders[*cnt], dir->d_name);
            *cnt = *cnt + 1;
        }
        closedir (d);
    }
}

long long count_regualr_files (char* path ) {
    long long file_cnt = 0;
    DIR * dirp; 
    printf ("path %s \n", path);

    struct dirent* entry; 

    dirp = opendir (path);
    while ((entry = readdir (dirp)) != NULL) {
        if (entry->d_type == DT_REG) {
            file_cnt ++;
        }
    }
    closedir (dirp);
    return file_cnt;
}
