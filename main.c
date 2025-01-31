#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <curses.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFERSIZE 4096
#define COPYMODE 0644

typedef struct {
    char filename[256];
    char mode[11];
    int nlink;
    char uid[32];
    char gid[32];
    long size;
    char mtime[13];
} FileInfo;

FileInfo file_infos[256];
char fullpath[1024] = "/home";
char newpath[1024] = "/home";
char newmode[3];
int row = 1, col = 1;
int dir_num = 0;
int highlight = 0;
struct stat prebuf;

/* directory function */
void do_ls(char dirname[]);
void do_stat(char *filename, char dirname[]);
void store_file_info(int index, char *filename, struct stat *info_p);
void mode_to_letters(mode_t mode, char mode_str[]);
char *uid_to_name(uid_t uid);
char *gid_to_name(uid_t gid);
void display_files(WINDOW *win, int highlight, int start_row);
void display_menu(WINDOW *win, int *selected_option);
void find_newpath();

/* command execution function */
void copy_file(char *src, char *dest);
void move_file(char *src, char *dest);
void chmod_file(char *filename, char *newmode);
int bitMasking(int mode);

int main() {
    char yn;
    while(1) {
        int ch;
        int start_row;
        int max_rows;
        int selected_option = 0;

        initscr();
        clear();
        noecho();
        cbreak();
        keypad(stdscr, TRUE);

        strcpy(fullpath, "/home");

        while (1) {
            max_rows = LINES - 2;
            highlight = 0;
            start_row = 0;
            do_ls(fullpath);

            while (1) {
                display_files(stdscr, highlight, start_row);
                ch = getch();
                
                switch (ch) {
                    case KEY_UP:
                        if (highlight > 0) {
                            highlight--;
                        }
                        if (highlight < start_row) {
                            start_row--;
                        }
                        break;
                    case KEY_DOWN:
                        if (highlight < dir_num - 1) {
                            highlight++;
                        }
                        if (highlight >= start_row + max_rows) {
                            start_row++;
                        }
                        break;
                    case 'q':
                        endwin();
                        exit(0);
                    default:
                        break;
                }

                if (ch == '\n')
                    break;
            }  


            char temp[1024];
            if (strlen(fullpath) + strlen(file_infos[highlight].filename) + 1 >= sizeof(temp)) {
                fprintf(stderr, "Path length exceeds buffer size: %s/%s\n", fullpath, file_infos[highlight].filename);
                endwin();
                exit(1);
            }

            strcpy(temp, fullpath);
            strcat(temp, "/");
            strcat(temp, file_infos[highlight].filename);
            strncpy(fullpath, temp, sizeof(fullpath) - 1);
            fullpath[sizeof(fullpath) - 1] = '\0';

            if (file_infos[highlight].mode[0] != 'd')
                break;
        }

        display_menu(stdscr, &selected_option);
        endwin();

        printf("%s\n", fullpath);
        switch (selected_option) {
            case 1:
                printf("Command excuted: cp(copy).\n");
                printf(" [%s]\n -> [%s]\ncopied successfully.\n", fullpath, newpath);
                break;
            case 2:
                printf("Command excuted: mv(move).\n");
                printf(" [%s]\n -> [%s]\nmoved successfully.\n", fullpath, newpath);
                break;
            case 3:
                printf("Command excuted: rm(remove).\n");
                printf(" [%s] removed successfully.\n", fullpath);
                break;
            case 4:
                printf("Command excuted: chmode(change mode).\n");
                struct stat nowbuf;
                stat(fullpath, &nowbuf);
                printf(" [%o] -> [%o] changed the mode successfully.\n", prebuf.st_mode, nowbuf.st_mode);
                break;
            default:
                break;
        }

        printf("-------------------------------------------------------------\n");
        printf("Do you want to continue to carry out this program?(y/n): ");
        scanf(" %c", &yn);
        printf("-------------------------------------------------------------\n");

        if (yn == 'n')
            break;
    }
    printf("Exit the program.\n");
    return 0;
}

/* directory function */
void do_ls(char dirname[]) {
    DIR *dir_ptr = NULL;
    struct dirent *dirent_ptr = NULL;
    dir_num = 0;

    if ((dir_ptr = opendir(dirname)) == NULL) {
        fprintf(stderr, "ls: cannot open %s\n", dirname);
    }
    else {
        while ((dirent_ptr = readdir(dir_ptr)) != NULL) {
            do_stat(dirent_ptr->d_name, dirname);
            dir_num++;
        }
        closedir(dir_ptr);
    }
    return;
}

void do_stat(char *filename, char dirname[]) {
    struct stat buf;
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s", dirname, filename);
    if (stat(filepath, &buf) == -1)
        perror(filename);
    else
        store_file_info(dir_num, filename, &buf);
    return;
}

void store_file_info(int index, char *filename, struct stat *info_p) {
    FileInfo *info = &file_infos[index];
    strncpy(info->filename, filename, 255);
    mode_to_letters(info_p->st_mode, info->mode);
    info->nlink = (int)info_p->st_nlink;
    strncpy(info->uid, uid_to_name(info_p->st_uid), 31);
    strncpy(info->gid, gid_to_name(info_p->st_gid), 31);
    info->size = (long)info_p->st_size;
    strncpy(info->mtime, 4 + ctime(&info_p->st_mtime), 12);
    info->mtime[12] = '\0'; // Null-terminate
    return;
}

void mode_to_letters(mode_t mode, char mode_str[]) {
    strcpy(mode_str, "----------");

    if (S_ISDIR(mode)) mode_str[0] = 'd';
    if (S_ISCHR(mode)) mode_str[0] = 'c';
    if (S_ISBLK(mode)) mode_str[0] = 'b';

    if (mode & S_IRUSR) mode_str[1] = 'r';
    if (mode & S_IWUSR) mode_str[2] = 'w';
    if (mode & S_IXUSR) mode_str[3] = 'x';

    if (mode & S_IRGRP) mode_str[4] = 'r';
    if (mode & S_IWGRP) mode_str[5] = 'w';
    if (mode & S_IXGRP) mode_str[6] = 'x';

    if (mode & S_IROTH) mode_str[7] = 'r';
    if (mode & S_IWOTH) mode_str[8] = 'w';
    if (mode & S_IXOTH) mode_str[9] = 'x';

    return;
}

char *uid_to_name(uid_t uid) {
    struct passwd *pw_ptr = NULL;
    static char uid_str[10];
    if ((pw_ptr = getpwuid(uid)) == NULL) {
        sprintf(uid_str, "%d", uid);
        return uid_str;
    }
    else {
        return pw_ptr->pw_name;
    }
}

char *gid_to_name(gid_t gid) {
    struct group *gp_ptr = NULL;
    static char gid_str[10];
    if ((gp_ptr = getgrgid(gid)) == NULL) {
        sprintf(gid_str, "%d", gid);
        return gid_str;
    }
    else {
        return gp_ptr->gr_name;
    }
}

void display_files(WINDOW *win, int highlight, int start_row) {
    int i;
    int max_rows = LINES - 2;

    clear();
    for (i = start_row; i < dir_num && i < start_row + max_rows; i++) {
        FileInfo *info = &file_infos[i];

        start_color();
        init_pair(1, COLOR_BLUE, COLOR_BLACK);
        init_pair(2, COLOR_BLUE, COLOR_WHITE);
        init_pair(3, COLOR_BLACK, COLOR_WHITE);

        if (info->mode[0] == 'd') {  // Directory
            wattron(win, COLOR_PAIR(1));
        }
        if (i == highlight && info->mode[0] == 'd') {
            wattron(win, COLOR_PAIR(2));
        } else if (i == highlight) {
            wattron(win, COLOR_PAIR(3));
        }

        mvwprintw(win, i - start_row + 1, 1, "%-30s", info->filename);
        
        if (i == highlight && info->mode[0] == 'd') {
            wattroff(win, COLOR_PAIR(2));
            wattron(win, COLOR_PAIR(3));
        } else if (info->mode[0] == 'd') {
            wattroff(win, COLOR_PAIR(1));
        }

        mvwprintw(win, i - start_row + 1, 31, "%s %4d %-8s %-8s %8ld %.12s",
                  info->mode, info->nlink, info->uid, info->gid, info->size, info->mtime);

        if (i == highlight) {
            wattroff(win, COLOR_PAIR(3));
        }
    }
    refresh();

    return;
}

void display_menu(WINDOW *win, int *selected_option) {
    clear();
    mvprintw(1, 1, "Selected path: %s", fullpath);
    mvprintw(3, 1, "Choose an option:");

    mvprintw(5, 3, "1. cp (copy)");
    mvprintw(6, 3, "2. mv (move)");
    mvprintw(7, 3, "3. rm (remove)");
    mvprintw(8, 3, "4. chmod (change mode)");
    mvprintw(9, 3, "5. Exit");

    mvprintw(11, 1, "Enter the desired task number (1~5): ");
    refresh();

    echo();
    scanw("%d", selected_option);

    switch (*selected_option) {
        case 1:
            char newname[100];
            mvprintw(13, 1, "Enter a name for new file: ");
            scanw("%s", newname);
            mvprintw(15, 1, "When you find the directory, enter 'y'.");
            refresh();
            sleep(1);
            find_newpath();
            copy_file(fullpath, newname);
            break;
        case 2:
            char name[100];
            strcpy(name, file_infos[highlight].filename);
            mvprintw(13, 1, "When you find the directory, enter 'y'.");
            refresh();
            sleep(1);
            find_newpath();
            move_file(fullpath, name);
            break;
        case 3:
            remove(fullpath);
            break;
        case 4:
            mvprintw(13, 1, "Enter the new mode in number(ex. 444): ");
            scanw("%s", newmode);
            chmod_file(fullpath, newmode);
            break;
        default:
            break;
    }

    return;
}

void find_newpath() {
    int ch;
    int start_row;
    int max_rows;

    strcpy(newpath, "/home");

    while (1) {
        max_rows = LINES - 2;
        highlight = 0;
        start_row = 0;
        do_ls(newpath);

        while (1) {
            display_files(stdscr, highlight, start_row);
            ch = getch();
            
            switch (ch) {
                case KEY_UP:
                    if (highlight > 0) {
                        highlight--;
                    }
                    if (highlight < start_row) {
                        start_row--;
                    }
                    break;
                case KEY_DOWN:
                    if (highlight < dir_num - 1) {
                        highlight++;
                    }
                    if (highlight >= start_row + max_rows) {
                        start_row++;
                    }
                    break;
                case 'y':
                    endwin();
                default:
                    break;
            }

            if (ch == 'y' || ch == '\n')
                break;
        }  

        if (ch == 'y')
            break;

        char temp[1024];
        if (strlen(newpath) + strlen(file_infos[highlight].filename) + 1 >= sizeof(temp)) {
            fprintf(stderr, "Path length exceeds buffer size: %s/%s\n", newpath, file_infos[highlight].filename);
            endwin();
            exit(1);
        }

        strcpy(temp, newpath);
        strcat(temp, "/");
        strcat(temp, file_infos[highlight].filename);
        strncpy(newpath, temp, sizeof(newpath) - 1);
        newpath[sizeof(newpath) - 1] = '\0';
    }

}

/* command execution function */
void copy_file(char *src, char *dest) {
    int in_fd, out_fd, n_chars;
    char buf[BUFFERSIZE];

    char temp[1024];
    if (strlen(newpath) + strlen(dest) + 1 >= sizeof(temp)) {
        fprintf(stderr, "Path length exceeds buffer size: %s/%s\n", newpath, dest);
        endwin();
        exit(1);
    }

    strcpy(temp, newpath);
    strcat(temp, "/");
    strcat(temp, dest);
    strncpy(newpath, temp, sizeof(newpath) - 1);
    newpath[sizeof(newpath) - 1] = '\0';

    if ((in_fd = open(src, O_RDONLY)) == -1) {
        fprintf(stderr, "Error: Cannot open %s\n", src);
        exit(-1);
    }

    if ((out_fd = open(newpath, O_WRONLY | O_CREAT | O_TRUNC, COPYMODE)) == -1) {
        fprintf(stderr, "Error: Cannot open %s\n", dest);
        exit(-1);
    }

    while((n_chars = read(in_fd, buf, BUFFERSIZE)) > 0) {
        if (write(out_fd, buf, n_chars) != n_chars) {
            fprintf(stderr, "Error: Cannot write to %s\n", dest);
            exit(-1);
        }
    }

    if (n_chars == -1) {
        fprintf(stderr, "Error: Cannot read %s\n", src);
        exit(-1);
    }

    if (close(in_fd) == -1 || close(out_fd) == -1) {
        fprintf(stderr, "Error closeing files.\n");
        exit(-1);
    }

    return;
}

void move_file(char *src, char *dest) {
    int in_fd, out_fd, n_chars;
    char buf[BUFFERSIZE];

    char temp[1024];
    if (strlen(newpath) + strlen(dest) + 1 >= sizeof(temp)) {
        fprintf(stderr, "Path length exceeds buffer size: %s/%s\n", newpath, dest);
        endwin();
        exit(1);
    }

    strcpy(temp, newpath);
    strcat(temp, "/");
    strcat(temp, dest);
    strncpy(newpath, temp, sizeof(newpath) - 1);
    newpath[sizeof(newpath) - 1] = '\0';
    
    if ((in_fd = open(src, O_RDONLY)) == -1) {
        fprintf(stderr, "Error: Cannot open %s\n", src);
        exit(-1);
    }

    if ((out_fd = open(newpath, O_WRONLY | O_CREAT | O_TRUNC, COPYMODE)) == -1) {
        fprintf(stderr, "Error: Cannot open %s\n", dest);
        exit(-1);
    }

    while((n_chars = read(in_fd, buf, BUFFERSIZE)) > 0) {
        if (write(out_fd, buf, n_chars) != n_chars) {
            fprintf(stderr, "Error: Cannot write to %s\n", dest);
            exit(-1);
        }
    }

    if (n_chars == -1) {
        fprintf(stderr, "Error: Cannot read %s\n", src);
        exit(-1);
    }

    if (close(in_fd) == -1 || close(out_fd) == -1) {
        fprintf(stderr, "Eroor closeing files.\n");
        exit(-1);
    }

    remove(src);

    return;
}

void chmod_file(char *filename, char *newmode) {
    struct stat buf;

    if ((stat(filename, &buf) == -1) || (stat(filename, &prebuf) == -1)) {
        perror("Eroor");
        exit(-1);
    }

    // reset mode
    chmod(filename, buf.st_mode & 0xFE00);
    stat(filename, &buf);

    buf.st_mode |= bitMasking(newmode[0] - '0') << 6;
    buf.st_mode |= bitMasking(newmode[1] - '0') << 3;
    buf.st_mode |= bitMasking(newmode[2] - '0');

    chmod(filename, buf.st_mode);

    return;
}

int bitMasking(int mode) {
    int new_mode = 0;

    if (mode & S_IROTH) new_mode |= S_IROTH;
    if (mode & S_IWOTH) new_mode |= S_IWOTH;
    if (mode & S_IXOTH) new_mode |= S_IXOTH;

    return new_mode;
}