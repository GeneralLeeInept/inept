#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


namespace gli
{

class FileContainer;


class File
{
    friend FileContainer;

public:
    File(FileContainer* container, void* handle);
    ~File();

    void close();
    bool valid();

protected:
    FileContainer* _container;
    void* _handle;
};


class FileContainer
{
public:
    FileContainer() = default;
    virtual ~FileContainer() = default;

    bool open(const char* path, File*& handle);
    void close(File* handle);
    bool valid(File* handle);
    
    bool read_entire_file(const char* path, std::vector<uint8_t>& contents);

    virtual bool attach(const char* container_name) = 0;
    virtual void dettach() = 0;

protected:
    virtual bool open_internal(const char* path, void*& handle) = 0;
    virtual void close_internal(void* handle) = 0;
    virtual bool valid_internal(void* handle) = 0;
    virtual bool read_entire_file_internal(const char* path, std::vector<uint8_t>& contents) = 0;
};


class FileSystem
{
public:
    static FileSystem* get();

    ~FileSystem();

    void shutdown();

    bool open(const char* path, File*& handle);
    bool read_entire_file(const char* path, std::vector<uint8_t>& contents);

private:
    using FileContainerPtr = std::unique_ptr<FileContainer>;
    using FileContainerList = std::vector<FileContainerPtr>;
    using FileContainerLookup = std::unordered_map<std::string, size_t>;

    FileContainerList _containers;
    FileContainerLookup _container_lookup;

    FileContainer* get_or_create_container(const std::string& container_name);

    static FileSystem* _singleton;
};

}
