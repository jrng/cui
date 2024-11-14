#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#ifndef CUI_NO_BACKEND

static void *
_cui_worker_thread_proc(void *data)
{
    (void) data;

    CuiWorkerThreadQueue *queue = &_cui_context.common.worker_thread_queue;

    for (;;)
    {
        if (_cui_do_next_worker_thread_queue_entry(queue))
        {
            pthread_mutex_lock(&queue->semaphore_mutex);
            pthread_cond_wait(&queue->semaphore_cond, &queue->semaphore_mutex);
            pthread_mutex_unlock(&queue->semaphore_mutex);
        }
    }

    return 0;
}

static void *
_cui_background_thread_proc(void *data)
{
    CuiBackgroundThreadQueue *queue = (CuiBackgroundThreadQueue *) data;

    for (;;)
    {
        if (_cui_do_next_background_thread_queue_entry(queue))
        {
            pthread_mutex_lock(&queue->semaphore_mutex);
            pthread_cond_wait(&queue->semaphore_cond, &queue->semaphore_mutex);
            pthread_mutex_unlock(&queue->semaphore_mutex);
        }
    }

    return 0;
}

#endif

bool
_cui_set_fd_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL);

    if (flags == -1)
    {
        return false;
    }

    flags |= O_NONBLOCK;

    if (fcntl(fd, F_SETFL, flags) == -1)
    {
        return false;
    }

    return true;
}

CuiString
cui_platform_get_environment_variable(CuiArena *temporary_memory, CuiArena *arena, CuiString name)
{
    (void) arena;

    CuiString result = { 0 };

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    char *var = getenv(cui_to_c_string(temporary_memory, name));

    if (var)
    {
        result = CuiCString(var);
    }

    cui_end_temporary_memory(temp_memory);

    return result;
}

int32_t cui_platform_get_environment_variable_int32(CuiArena *temporary_memory, CuiString name)
{
    return cui_string_parse_int32(cui_platform_get_environment_variable(temporary_memory, 0, name));
}

void *
cui_platform_allocate(uint64_t size)
{
    void *result = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (result == MAP_FAILED) ? 0 : result;
}

void
cui_platform_deallocate(void *ptr, uint64_t size)
{
    munmap(ptr, size);
}

uint64_t
cui_platform_get_performance_counter(void)
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC_RAW, &time);
    return time.tv_sec * 1000000000ULL + time.tv_nsec;
}

uint64_t
cui_platform_get_performance_frequency(void)
{
    return 1000000000ULL;
}

bool
cui_platform_directory_exists(CuiArena *temporary_memory, CuiString directory)
{
    bool result = false;

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    struct stat stats;

    if (!stat(cui_to_c_string(temporary_memory, directory), &stats) && S_ISDIR(stats.st_mode))
    {
        result = true;
    }

    cui_end_temporary_memory(temp_memory);

    return result;
}

void
cui_platform_directory_create(CuiArena *temporary_memory, CuiString directory)
{
    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    mkdir(cui_to_c_string(temporary_memory, directory), 0775);

    cui_end_temporary_memory(temp_memory);
}

bool
cui_platform_file_exists(CuiArena *temporary_memory, CuiString filename)
{
    bool result = false;
    struct stat stats;

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    if (!stat(cui_to_c_string(temporary_memory, filename), &stats))
    {
        result = S_ISREG(stats.st_mode) ? true : false;
    }

    cui_end_temporary_memory(temp_memory);

    return result;
}

CuiFile *
cui_platform_file_open(CuiArena *temporary_memory, CuiString filename, uint32_t mode)
{
    CuiFile *result = 0;
    int access_mode = 0;

    if ((mode & CUI_FILE_MODE_READ) && (mode & CUI_FILE_MODE_WRITE))
    {
        access_mode = O_RDWR;
    }
    else if (mode & CUI_FILE_MODE_READ)
    {
        access_mode = O_RDONLY;
    }
    else if (mode & CUI_FILE_MODE_WRITE)
    {
        access_mode = O_WRONLY;
    }

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    int fd = open(cui_to_c_string(temporary_memory, filename), access_mode);

    if (fd >= 0)
    {
        result = (CuiFile *) ((uint64_t) fd + 1);
    }

    cui_end_temporary_memory(temp_memory);

    return result;
}

CuiFile *
cui_platform_file_create(CuiArena *temporary_memory, CuiString filename)
{
    CuiFile *result = 0;

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    int fd = creat(cui_to_c_string(temporary_memory, filename), 0644);

    if (fd >= 0)
    {
        result = (CuiFile *) ((uint64_t) fd + 1);
    }

    cui_end_temporary_memory(temp_memory);

    return result;
}

static inline CuiFileAttributes
_cui_get_file_attributes(struct stat stats)
{
    CuiFileAttributes result = { 0 };

    if (S_ISDIR(stats.st_mode))
    {
        result.flags |= CUI_FILE_ATTRIBUTE_IS_DIRECTORY;
    }

    result.size = stats.st_size;

    return result;
}

CuiFileAttributes
cui_platform_file_get_attributes(CuiFile *file)
{
    CuiAssert(file);
    CuiAssert((uint64_t) file <= 0x80000000);

    struct stat stats;
    CuiFileAttributes result = { 0 };

    if (!fstat(*(int *) &file - 1, &stats))
    {
        result = _cui_get_file_attributes(stats);
    }

    return result;
}

CuiFileAttributes
cui_platform_get_file_attributes(CuiArena *temporary_memory, CuiString filename)
{
    struct stat stats;
    CuiFileAttributes result = { 0 };

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    if (!lstat(cui_to_c_string(temporary_memory, filename), &stats))
    {
        result = _cui_get_file_attributes(stats);
    }

    cui_end_temporary_memory(temp_memory);

    return result;
}

void
cui_platform_file_truncate(CuiFile *file, uint64_t size)
{
    CuiAssert(file);
    CuiAssert((uint64_t) file <= 0x80000000);
    ftruncate(*(int *) &file - 1, size);
}

void
cui_platform_file_read(CuiFile *file, void *buffer, uint64_t offset, uint64_t size)
{
    CuiAssert(file);
    CuiAssert((uint64_t) file <= 0x80000000);

    lseek(*(int *) &file - 1, offset, SEEK_SET);
    read(*(int *) &file - 1, buffer, size);
}

void
cui_platform_file_write(CuiFile *file, void *buffer, uint64_t offset, uint64_t size)
{
    CuiAssert(file);
    CuiAssert((uint64_t) file <= 0x80000000);

    lseek(*(int *) &file - 1, offset, SEEK_SET);
    write(*(int *) &file - 1, buffer, size);
}

void
cui_platform_file_close(CuiFile *file)
{
    CuiAssert(file);
    CuiAssert((uint64_t) file <= 0x80000000);
    close(*(int *) &file - 1);
}

CuiString
cui_platform_get_canonical_filename(CuiArena *temporary_memory, CuiArena *arena, CuiString filename)
{
    CuiString result = { 0 };

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    if (cui_string_starts_with(filename, CuiStringLiteral("~")))
    {
        CuiString home_dir = cui_platform_get_environment_variable(temporary_memory, temporary_memory, CuiStringLiteral("HOME"));

        if (home_dir.count)
        {
            cui_string_advance(&filename, 1);
            filename = cui_path_concat(temporary_memory, home_dir, filename);
        }
    }

    char *canonical_filename = realpath(cui_to_c_string(temporary_memory, filename), 0);

    cui_end_temporary_memory(temp_memory);

    if (canonical_filename)
    {
        result = cui_copy_string(arena, CuiCString(canonical_filename));
        free(canonical_filename);
    }

    return result;
}

void
cui_platform_get_files(CuiArena *temporary_memory, CuiArena *arena, CuiString directory, CuiFileInfo **file_list)
{
    if ((directory.count > 1) && (directory.data[directory.count - 1] == '/'))
    {
        directory.count -= 1;
    }

    DIR *dir = opendir(cui_to_c_string(temporary_memory, directory));

    if (dir)
    {
        struct dirent *entry;

        while ((entry = readdir(dir)))
        {
            CuiAssert(entry->d_type != DT_UNKNOWN);

            CuiFileInfo *file_info = cui_array_append(*file_list);

            CuiString filename = CuiCString(entry->d_name);
            CuiString fullpath = cui_path_concat(temporary_memory, directory, filename);

            file_info->name = cui_copy_string(arena, filename);
            file_info->attr = cui_platform_get_file_attributes(temporary_memory, fullpath);
        }

        closedir(dir);
    }
}

CuiString
cui_platform_get_current_working_directory(CuiArena *temporary_memory, CuiArena *arena)
{
    CuiString result = { 0 };

    size_t buffer_size = CuiKiB(1);
    char *current_working_directory = cui_alloc_array(temporary_memory, char, buffer_size, CuiDefaultAllocationParams());

    if (getcwd(current_working_directory, buffer_size))
    {
        result = cui_copy_string(arena, CuiCString(current_working_directory));
    }

    return result;
}
