#pragma once
#include <iostream>
#include <ostream>
#include <stdint.h>
#include <string>
#include <vector>

enum BlockType
{
    NOT_A_BLOCK = 0,
    BLOCK_SERVER = 1 << 0,
    BLOCK_LOCATION = 1 << 1,
    BLOCK_GLOBAL = 1 << 2
};

class ConfigItem
{
    std::string _name, _value;
    ConfigItem* _parent;
    BlockType _blockType;

    /* deleted for now */
    ConfigItem(const ConfigItem& other);
    ConfigItem& operator=(const ConfigItem& rhs);

  public:
    ConfigItem(const std::string& name,
               BlockType type,
               ConfigItem* parent,
               const std::string& value = "");
    ~ConfigItem(void);

    std::vector<ConfigItem*> children;

    BlockType getType(void) const;
    ConfigItem* getParent(void) const;
    const std::string& getName(void) const;
    const std::string& getValue(void) const;

    /*
    ** Find the nearest configuration atom of type `key` that applies to `this`.
    ** Search starts in this->parent block, and ends when a config item of type
    *`key`
    ** is found OR when global scope has been entirely searched.
    **
    ** A pointer to the item is returned if any is found, NULL otherwise.
    */

    ConfigItem* findNearest(const std::string& key);
    const ConfigItem* findNearest(const std::string& key) const;
    std::vector<ConfigItem*> findNearestBlocks(const std::string& key);

    /*
    ** Search a block config item for a configuration atom of type `key`.
    ** Nested blocks are not searched.
    ** If this function is called on a non-block configuration item, an
    *exception is thrown.
    */

    ConfigItem* findAtomInBlock(const std::string& key);
    const ConfigItem* findAtomInBlock(const std::string& key) const;

    /*
    ** Search for all the configuration blocks of type `key` in `this`, which
    *must be a
    ** block itself. If that's not the case, an exception is thrown.
    */

    const std::vector<ConfigItem*> findBlocks(const std::string& key);
    const std::vector<const ConfigItem*> findBlocks(
      const std::string& key) const;
};

std::ostream&
operator<<(std::ostream& os, const ConfigItem& item);

struct FindConfigItemPredicate
{
    std::string key;
    FindConfigItemPredicate(const std::string& name)
      : key(name)
    {}
    bool operator()(const ConfigItem* item) const
    {
        return item->getName() == key;
    }
};
