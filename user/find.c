#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char* fmtname(char *path){ // stolen from /user/ls.c
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void find_func(char* path, char* target_file){ // most of code stolen from /user/ls.c
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(path, 0)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch(st.type){
        case T_FILE:
            // in this case the path refers to a file, I think nobody has such a strange requirement.
            if(strcmp(target_file, fmtname(path)) == 0){
                printf("%s\n", path);
            }
            break;
        case T_DIR:
            // common case: path refers to a dir, step into it recursively is needed
            if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
                printf("find: path too long\n");
                break;
            }
            strcpy(buf, path);
            p = buf+strlen(buf);
            *p++ = '/';
            while(read(fd, &de, sizeof(de)) == sizeof(de)){
                // Don't recurse into "." and ".."!!!
                if((de.inum == 0) || (strcmp(de.name, ".") == 0) || (strcmp(de.name, "..") == 0))
                    continue;
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;
                if(stat(buf, &st) < 0){
                    printf("ls: cannot stat %s\n", buf);
                    continue;
                }
                if(st.type == T_FILE){ // file type, compare the name
                    if(strcmp(de.name, target_file) == 0){
                        printf("%s\n", buf);
                    }
                }
                if(st.type == T_DIR){ // dir type, recurse
                    find_func(buf, target_file);
                }
            }
            break;
    }
    close(fd);
}

int
main(int argc, char *argv[])
{
    // find <path> <target file>
    if(argc != 3){
        fprintf(2, "find: oprand number donot satisfy.\n");
        exit(1);
    }
    find_func(argv[1], argv[2]);
    exit(0);
}