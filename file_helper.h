#include <stdio.h>
#include <dirent.h>


/**
 * Read only regular files from the `path`. 
 *
 * \param path The path we need to explore
 *
 * \return The number of regular files in the `path` as an integer.
 */
int count_regualr_files (char* path );


/**
 * Read the path provided by `path` and put the file names into `subfolders` parameter, in the meantime increment `cnt` by one. 
 * If no manual file system manipulation, the path /home/pi/hzlogger/ should only contain folers. 
 *
 * This function ignores "." and "..". 
 *
 * \param subfolders A 2-D char array to store valid subfoler names. 
 * \param cnt An integer (passed by reference) to remember the number of valid folder names in parameter subfolder.
 *
 * \return void.  Real return values are stored in subfoler and cnt.
 */
void read_files (char* path, char subfolders[1024][50], int* cnt);
