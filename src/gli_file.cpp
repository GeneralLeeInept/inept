#include "gli_file.h"

#include "gli_log.h"
#include "zlib/zlib.h"
#include "zlib/contrib/minizip/unzip.h"

#include <stdio.h>

namespace gli
{

bool FileContainer::open(const char* path, File*& handle)
{
    void* handlei = nullptr;

    if (!open_internal(path, handlei))
    {
        gliLog(LogLevel::Warning, "File", "FileContainer::open", "open_internal failed for '%s'.", path);
        return false;
    }

    handle = new File(this, handlei);
    return true;
}


void FileContainer::close(File* handle)
{
    if (handle)
    {
        close_internal(handle->_handle);
        delete handle;
    }
}


bool FileContainer::valid(File* handle)
{
    return valid_internal(handle->_handle);
}


bool FileContainer::read_entire_file(const char* path, std::vector<uint8_t>& contents)
{
    return read_entire_file_internal(path, contents);
}


File::File(FileContainer* container, void* handle)
    : _container(container)
    , _handle()
{
}


File::~File()
{
    close();
}


bool File::valid()
{
    return _container && _container->valid(this);
}


void File::close()
{
    if (_container)
    {
        _container->close(this);
        _container = nullptr;
        _handle = nullptr;
    }
}


// TODO: File container representing the OS file system (or part thereof)
class FileContainerSystem : public FileContainer
{
public:
    bool attach(const char* container_name) override { return true; }
    void dettach() override {}
    bool open_internal(const char* path, void*& handle) override { return false; }
    void close_internal(void* handle) override {}
    bool valid_internal(void* handle) override { return false; }
    bool read_entire_file_internal(const char* path, std::vector<uint8_t>& contents) override
    {
        // FIXME: do this better (Windows file functions / unicode paths / etc)
        FILE *fp = fopen(path, "rb");
        bool result = !!fp;

        if (result)
        {
            result = fseek(fp, 0, SEEK_END) == 0;
        }

        if (result)
        {
            long length = ftell(fp);
            result = length > 0;

            if (result)
            {
                contents.resize(length);
            }
        }

        if (result)
        {
            result = fseek(fp, 0, SEEK_SET) == 0;
        }

        if (result)
        {
            size_t bytes_read = fread(&contents[0], 1, contents.size(), fp);
            result = bytes_read == contents.size();
        }

        if (fp)
        {
            fclose(fp);
        }

        return result;
    }
};


class FileContainerZipFile : public FileContainer
{
public:
    bool attach(const char* container_name) override
    {
        _container_name = container_name;
        _unzfile = unzOpen(container_name);
        _file_opened = false;
        return !!_unzfile;
    }


    void dettach() override
    {
        if (_unzfile)
        {
            gliLog(LogLevel::Info, "File", "FileContainerZipFile::dettach", "Dettaching zip file container.");

            if (_file_opened)
            {
                unzCloseCurrentFile(_unzfile);
            }

            unzClose(_unzfile);
        }
    }


    bool open_internal(const char* path, void*& handle) override
    {
        if (!_unzfile)
        {
            gliLog(LogLevel::Error, "File", "FileContainerZipFile::open_internal", "Not attached.");
            return false;
        }

        if (_file_opened)
        {
            gliLog(LogLevel::Error, "File", "FileContainerZipFile::open_internal", "Only one file may be opened at a time.");
            return false;
        }

        if (unzLocateFile(_unzfile, path, 1))
        {
            gliLog(LogLevel::Error, "File", "FileContainerZipFile::open_internal", "File '%s' not found.", path);
            return false;
        }

        if (unzGetCurrentFileInfo(_unzfile, &_current_file_info, nullptr, 0, nullptr, 0, nullptr, 0))
        {
            gliLog(LogLevel::Error, "File", "FileContainerZipFile::open_internal", "Error getting file info for file '%s' [%d].", path, UNZ_ERRNO);
        }

        if (unzOpenCurrentFile(_unzfile))
        {
            gliLog(LogLevel::Error, "File", "FileContainerZipFile::open_internal", "Error opening '%s' [%d].", path, UNZ_ERRNO);
            return false;
        }

        _file_opened = true;

        return true;
    }


    void close_internal(void* handle) override
    {
        if (_unzfile && _file_opened)
        {
            unzCloseCurrentFile(_unzfile);
            _file_opened = false;
        }
    }


    bool valid_internal(void* handle) override { return _unzfile && _file_opened; }


    bool read_entire_file_internal(const char* path, std::vector<uint8_t>& contents) override
    {
        if (!_unzfile)
        {
            gliLog(LogLevel::Error, "File", "FileContainerZipFile::read_entire_file_internal", "Not attached.");
            return false;
        }

        if (_file_opened)
        {
            gliLog(LogLevel::Error, "File", "FileContainerZipFile::read_entire_file_internal", "Only one file may be opened at a time.");
            return false;
        }

        if (unzLocateFile(_unzfile, path, 1))
        {
            gliLog(LogLevel::Error, "File", "FileContainerZipFile::read_entire_file_internal", "File '%s' not found.", path);
            return false;
        }

        if (unzGetCurrentFileInfo(_unzfile, &_current_file_info, nullptr, 0, nullptr, 0, nullptr, 0))
        {
            gliLog(LogLevel::Error, "File", "FileContainerZipFile::read_entire_file_internal", "Error getting file info for file '%s' [%d].", path,
                   UNZ_ERRNO);
            return false;
        }

        if (unzOpenCurrentFile(_unzfile))
        {
            gliLog(LogLevel::Error, "File", "FileContainerZipFile::read_entire_file_internal", "Error opening '%s' [%d].", path, UNZ_ERRNO);
            return false;
        }

        contents.resize(_current_file_info.uncompressed_size);
        int read_result = unzReadCurrentFile(_unzfile, contents.data(), _current_file_info.uncompressed_size);

        unzCloseCurrentFile(_unzfile);

        // FIXME: unzReadCurrentFile's return result is ambiguous if the high bit of read size is set (unzReadCurrentFile returns either the number
        // of bytes read or a negative number if an error occurred, but the requested read size is unsigned).
        if ((uLong)read_result != _current_file_info.uncompressed_size)
        {
            gliLog(LogLevel::Error, "File", "FileContainerZipFile::read_entire_file_internal", "Error reading file '%s' [%d].", path, UNZ_ERRNO);
            return false;
        }

        return true;
    }

private:
    std::string _container_name;
    unzFile _unzfile = nullptr;
    bool _file_opened = false;
    unz_file_info _current_file_info;
};


FileSystem g_singleton;
FileSystem* FileSystem::_singleton = &g_singleton;


FileSystem* FileSystem::get()
{
    return _singleton;
}


FileSystem::~FileSystem()
{
    shutdown();
}


void FileSystem::shutdown()
{
    for (auto& container : _containers)
    {
        container->dettach();
    }
}


bool FileSystem::open(const char* path, File*& handle)
{
    /*
        Paths are either raw system paths or paths to files in a container:
        //<container>//data/file.ext
    */
    bool success = false;
    std::string spath(path);
    FileContainer* container = nullptr;

    if (spath.rfind("//", 0) == 0)
    {
        size_t delim = spath.find("//", 2);

        if (delim == std::string::npos)
        {
            gliLog(LogLevel::Error, "File", "FileSystem::open", "Couldn't parse path '%s'.", path);
            return false;
        }

        std::string container_name = spath.substr(2, delim - 2);
        container = get_or_create_container(container_name);
        spath = spath.substr(delim + 2);
    }
    else
    {
        container = get_or_create_container("");
    }

    if (container)
    {
        success = container->open(spath.c_str(), handle);
    }
    else
    {
        gliLog(LogLevel::Error, "File", "FileSystem::open", "Could not find container to open file '%s'.", path);
    }

    return success;
}


bool FileSystem::read_entire_file(const char* path, std::vector<uint8_t>& contents)
{
    bool success = false;
    std::string spath(path);
    FileContainer* container = nullptr;

    if (spath.rfind("//", 0) == 0)
    {
        size_t delim = spath.find("//", 2);

        if (delim == std::string::npos)
        {
            gliLog(LogLevel::Error, "File", "FileSystem::read_entire_file", "Couldn't parse path '%s'.", path);
            return false;
        }

        std::string container_name = spath.substr(2, delim - 2);
        container = get_or_create_container(container_name);
        spath = spath.substr(delim + 2);
    }
    else
    {
        container = get_or_create_container("");
    }

    if (container)
    {
        success = container->read_entire_file(spath.c_str(), contents);
    }
    else
    {
        gliLog(LogLevel::Error, "File", "FileSystem::read_entire_file", "Could not find container to open file '%s'.", path);
    }

    return success;
}


FileContainer* FileSystem::get_or_create_container(const std::string& container_name)
{
    FileContainerLookup::const_iterator it = _container_lookup.find(container_name);

    if (it == _container_lookup.end())
    {
        FileContainerPtr new_container;

        if (container_name.empty())
        {
            gliLog(LogLevel::Info, "File", "FileSystem::get_or_create", "Creating system file container.");
            new_container = std::make_unique<FileContainerSystem>();
        }
        else
        {
            gliLog(LogLevel::Info, "File", "FileSystem::get_or_create", "Creating zip file container for '%s'", container_name.c_str());
            new_container = std::make_unique<FileContainerZipFile>();
        }

        if (!new_container->attach(container_name.c_str()))
        {
            gliLog(LogLevel::Info, "File", "FileSystem::get_or_create", "Failed to attach zip file container for '%s'", container_name.c_str());
            return nullptr;
        }

        auto result = _container_lookup.emplace(container_name, _containers.size());

        if (!result.second)
        {
            gliLog(LogLevel::Info, "File", "FileSystem::get_or_create", "Failed to insert container for '%s' into map", container_name.c_str());
            return nullptr;
        }

        _containers.push_back(std::move(new_container));
        it = result.first;
    }

    return _containers[it->second].get();
}

} // namespace gli
