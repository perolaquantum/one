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

#include "PoolSQLCache.h"
#include "Nebula.h"

PoolSQLCache::PoolSQLCache(bool oa):only_active(oa)
{
    Nebula&  nd = Nebula::instance();

    if (only_active)
    {
        nd.get_configuration_attribute("POOL_CACHE_PRESSURE", max_elements);
    }
    else
    {
        nd.get_configuration_attribute("POOL_CACHE_SIZE", max_elements);
    }
};

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int PoolSQLCache::get(int oid, PoolObjectSQL ** object, bool olock)
{
    std::map<int, CacheLine *>::iterator it = cache.find(oid);

    if ( it == cache.end() )
    {
        return -1;
    }
    else if ( it->second->dirty || only_active )
    {
        if ( it->second->object != 0 )
        {
            it->second->object->lock();
        }

        delete_cache_line(it);

        return -1;
    }
    else
    {
        *object = it->second->object;

        if (olock)
        {
            (*object)->lock();
        }

        if ( (*object)->is_deleted() )
        {
            delete_cache_line(it);

            *object = 0;
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void PoolSQLCache::insert(PoolObjectSQL * object)
{
    CacheLine *  cl = new CacheLine(object);
    unsigned int cs = cache.size();

    if ( cs > max_elements )
    {
        if ( only_active )
        {
            flush_cache_lines();
        }
        else
        {
            delete_cache_block(max_elements * 0.15);
        }
    }

    cache.insert(make_pair(object->get_oid(), cl));

    fifo.push(object->get_oid());
};

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void PoolSQLCache::enable()
{
    Nebula&  nd = Nebula::instance();

    flush_cache_lines();

    only_active = false;

    nd.get_configuration_attribute("POOL_CACHE_SIZE", max_elements);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void PoolSQLCache::disable()
{
    Nebula&  nd = Nebula::instance();

    flush_cache_lines();

    only_active  = true;

    nd.get_configuration_attribute("POOL_CACHE_PRESSURE", max_elements);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void PoolSQLCache::flush_cache_lines()
{
    std::map<int, CacheLine *>::iterator it;

    while (!fifo.empty())
    {
        fifo.pop();
    }

    for ( it = cache.begin() ; it != cache.end() ; )
    {
        // Any other locked object is just ignored
        int rc = it->second->object->try_lock();

        if ( rc == EBUSY ) // In use by other thread
        {
            fifo.push(it->first);

            it->second->dirty = true;

            ++it;
            continue;
        }

        it = delete_cache_line(it);
    }
};

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void PoolSQLCache::delete_cache_block(int block_size)
{
    int deleted = 0;
    int qs    = fifo.size();

    int oid;
    std::map<int, CacheLine *>::iterator it;

    for ( int i = 0 ; i < qs && deleted < block_size ; ++i )
    {
        oid = fifo.front();
        it  = cache.find(oid);

        if ( it == cache.end() )
        {
            fifo.pop();
        }
        else
        {
            int rc = it->second->object->try_lock();

            if ( rc == EBUSY ) // In use, move to back
            {
                fifo.pop();
                fifo.push(oid);
            }
            else
            {
                delete_cache_line(it);

                ++deleted;
            }
        }
    }
}

