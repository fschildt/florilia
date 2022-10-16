void platform_free_buffer(Platform_Buffer *buff)
{
    free(buff->mem);
    buff->size = 0;
}

bool platform_read_file(Platform_Buffer *buff, const char *pathname)
{
    int fd = open(pathname, O_RDONLY);
    if (fd == -1)
    {
        printf("error reading file %s\n", pathname);
        return false;
    }

    struct stat statbuff;
    if (fstat(fd, &statbuff) == -1)
    {
        printf("cant fstat file %s\n", pathname);
        close(fd);
        return false;
    }

    void *file_content = malloc(statbuff.st_size);
    if (!file_content)
    {
        printf("error: out of memory\n");
        close(fd);
        return false;
    }

    ssize_t bytes_read = read(fd, file_content, statbuff.st_size);
    if (bytes_read != statbuff.st_size)
    {
        printf("error: only read %ld/%ld bytes from file %s\n", bytes_read, statbuff.st_size, pathname);
        close(fd);
        free(file_content);
        return false;
    }

    buff->size = statbuff.st_size;
    buff->mem = file_content;
    return true;
}

bool platform__change_to_runtime_dir()
{
    // prepare home
    char *home = getenv("HOME");
    int home_len = strlen(home);

    if (home_len == 0 || home_len >= 4096) {
        printf("home dir invalid\n");
        return false;
    }


    // prepare subdir
    // Note: it might never end with '/', but let's be sure
    bool home_ends_with_slash = home[home_len-1] == '/';

    const char *subdir = home_ends_with_slash ? ".local/share/florilia" : "/.local/share/florilia";
    int subdir_len = strlen(subdir);

    if (home_len + subdir_len >= 4096) {
        printf("home/subdir has invalid length\n");
        return false;
    }


    // set run_tree
    char run_tree[4096];
    memcpy(run_tree, home, home_len);
    strcpy(run_tree + home_len, subdir);


    // change to run_tree
    int changed = chdir(run_tree);
    if (changed == -1) {
        perror("chdir");
        return false;
    }

    return true;
}
