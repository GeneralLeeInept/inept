#pragma once

#include <unordered_map>
#include <vector>
#include <memory>


class GliFile
{
public:
    GliFile(class GliFileContainer* container, void* handle);
    ~GliFile();

    void close();
    bool valid();

protected:
    friend class GliFileContainer;

    class GliFileContainer* _container;
    void* _handle;
};


class GliFileContainer
{
public:
    GliFileContainer() = default;
    virtual ~GliFileContainer() = default;

    bool open(const char* path, GliFile*& handle);
    void close(GliFile* handle);
    bool valid(GliFile* handle);
    
    bool read_entire_file(const char* path, std::vector<uint8_t>& contents);

    virtual bool attach(const char* container_name) = 0;
    virtual void dettach() = 0;

protected:
    virtual bool open_internal(const char* path, void*& handle) = 0;
    virtual void close_internal(void* handle) = 0;
    virtual bool valid_internal(void* handle) = 0;
    virtual bool read_entire_file_internal(const char* path, std::vector<uint8_t>& contents) = 0;
};


class GliFileSystem
{
public:
    static GliFileSystem* get();

    ~GliFileSystem();

    void shutdown();

    bool open(const char* path, GliFile*& handle);
    bool read_entire_file(const char* path, std::vector<uint8_t>& contents);

private:
    using GliFileContainerPtr = std::unique_ptr<GliFileContainer>;
    using GliFileContainerList = std::vector<GliFileContainerPtr>;
    using GliFileContainerLookup = std::unordered_map<std::string, size_t>;

    GliFileContainerList _containers;
    GliFileContainerLookup _container_lookup;

    GliFileContainer* get_or_create_container(const std::string& container_name);

    static GliFileSystem* _singleton;
};
