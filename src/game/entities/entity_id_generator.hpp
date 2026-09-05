#pragma once

class EntityIdGenerator {
public:
    uint32_t next() { return nextId_++; }
private:
    uint32_t nextId_ = 0;
};