/* -------------------------------------------------------------------------- */
/* Copyright 2002-2018, OpenNebula Project, OpenNebula Systems                */
/*                                                                            */
/* Licensed under the Apache License, Version 2.0 (the "License"); you may    */
/* not use this file except in compliance with the License. You may obtain    */
/* a copy of the License at                                                   */
/*                                                                            */
/* http://www.apache.org/licenses/LICENSE-2.0                                 */
/*                                                                            */
/* Unless required by applicable law or agreed to in writing, software        */
/* distributed under the License is distributed on an "AS IS" BASIS,          */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   */
/* See the License for the specific language governing permissions and        */
/* limitations under the License.                                             */
/* -------------------------------------------------------------------------- */

#ifndef POOL_SQL_CACHE_H_
#define POOL_SQL_CACHE_H_

#include <map>
#include <string>
#include <queue>

#include "PoolObjectSQL.h"

/**
 *  This class stores the active reference to pool objects. It can also
 *  cache to not reload object state from the DB.
 *
 *  Access to the cache needs to happen in a critical section.
 */
class PoolSQLCache
{
public:

    PoolSQLCache(bool only_active);

    virtual ~PoolSQLCache(){};

    /**
     *  Gets an object from the cache:
     *
     *  @param oid of the object
     *  @param object pointer to the object it can be 0 if the object was deleted
     *  @param olock to lock the object mutex
     *
     *  @return 0, object found and valid. If -1 is returned the object was not
     *  found or it was invalid and needs to be reloaded from the DB.
     */
    int get(int oid, PoolObjectSQL ** object, bool olock);

    /**
     *  Insert a new element in the cache. This method updates the FIFO for the
     *  replacement policy and replace elements if needed. This method MUST be
     *  called as a failure on get object without unlocking the cache mutex.
     *
     *  @param obejct to be inserted int the cache
     */
    void insert(PoolObjectSQL * object);

    /**
     *  Set the deleted flag for the cache line
     *    @param oid of object
     */
    void set_deleted(int oid);

    /**
     *  Disable Cache. Flush cache lines, disable objects in use and update
     *  cache state
     */
    void disable();

    /**
     *  Activate Cache.
     */
    void enable();

private:
    struct CacheLine
    {
        CacheLine(PoolObjectSQL * o):dirty(false), deleted(false), object(o){};

        /**
         *  Object has been modified and needs to be reloaded from DB
         */
        bool dirty;

        /**
         *  Object has been delected and no longer exists
         */
        bool deleted;

        /**
         *  Reference to the object
         */
        PoolObjectSQL * object;
    };

    /**
     *  Max number of elements in the cache.
     */
    unsigned int max_elements;

    /**
     *  When set to true only active references are kept in the cache
     */
    bool only_active;

    /**
     *  Fifo queue to implement cache replacement policy
     */
    std::queue<int> fifo;

    /**
     *  Cache of pool objects indexed by their oid
     */
    std::map<int, CacheLine *> cache;

    /**
     *  Deletes a line from the cache
     *    @param it pointing to the line
     */
    void delete_cache_line(std::map<int, CacheLine *>::iterator& it);

    /**
     *  Deletes all cache lines if they are not in use.
     */
    void flush_cache_lines();

    /**
     *  Deletes a set of cache lines. The lines are removed for objects in
     *  a FIFO fashion.
     *    @param block_size number of lines to delete
     */
    void delete_cache_block(int block_size);
};

#endif /*POOL_SQL_CACHE_H_*/

