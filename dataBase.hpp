#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <cassert>

#include "object.hpp"
class DataBase {
private:
    std::size_t sz_;
    objInfo*    content_;
public:
    explicit DataBase(std::size_t size);
    ~DataBase();
    DataBase(const DataBase& other);
    DataBase& operator=(const DataBase& other);
};

DataBase::DataBase(std::size_t size)
    : sz_      {size},
      content_ {new objInfo[size]{}}
{
}

DataBase::~DataBase() {
    delete[] content_;
}

DataBase::DataBase(const DataBase& other) 
    : sz_      {other.sz_},
      content_ {new objInfo[other.sz_]{}}
{
    for (auto it = 0; it < sz_; it++) {
        content_[it] = other.content_[it];
    }    
}

DataBase& DataBase::operator=(const DataBase& other) {
    if (this == &other) {
        return *this;
    }
    objInfo* newContent = new objInfo[sz_]{};

    for (auto it = 0; it < sz_; it++) {
        newContent[it] = other.content_[it];
    }

    delete[] content_;
    content_ = newContent;
    sz_ = other.sz_;

    return *this;
}