#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

CuiFile *
cui_file_open(CuiArena *temporary_memory, CuiString filename, uint32_t mode)
{
    CuiFile *result = 0;
    mode_t access_mode = 0;

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

static inline CuiFileAttributes
_cui_get_file_attributes(struct stat stats)
{
    CuiFileAttributes result = { 0 };

    if (stats.st_mode & S_IFDIR)
    {
        result.flags |= CUI_FILE_ATTRIBUTE_IS_DIRECTORY;
    }

    result.size = stats.st_size;

    return result;
}

CuiFileAttributes
cui_file_get_attributes(CuiFile *file)
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
cui_get_file_attributes(CuiArena *temporary_memory, CuiString filename)
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
cui_file_read(CuiFile *file, void *buffer, uint64_t offset, uint64_t size)
{
    CuiAssert(file);
    CuiAssert((uint64_t) file <= 0x80000000);

    lseek(*(int *) &file - 1, offset, SEEK_SET);
    read(*(int *) &file - 1, buffer, size);
}

void
cui_file_close(CuiFile *file)
{
    CuiAssert(file);
    CuiAssert((uint64_t) file <= 0x80000000);
    close(*(int *) &file - 1);
}

void
cui_get_files(CuiArena *temporary_memory, CuiString directory, CuiFileInfo **file_list, CuiArena *arena)
{
    if ((directory.count > 1) && (directory.data[directory.count - 1] == '/'))
    {
        directory.count -= 1;
    }

    DIR *dir = opendir(cui_to_c_string(temporary_memory, directory));

    if (!dir) return;

    struct dirent *entry;

    while ((entry = readdir(dir)))
    {
        CuiAssert(entry->d_type != DT_UNKNOWN);

        CuiFileInfo *file_info = cui_array_append(*file_list);

        CuiString filename = CuiCString(entry->d_name);
        CuiString fullpath = cui_path_concat(temporary_memory, directory, filename);

        file_info->name = cui_copy_string(arena, filename);
        file_info->attr = cui_get_file_attributes(temporary_memory, fullpath);
    }

    closedir(dir);
}
