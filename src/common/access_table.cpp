/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "access_table.h"

#include "cppan_string.h"
#include "database.h"
#include "lock.h"
#include "stamp.h"

#include <unordered_map>

struct AccessData
{
    Stamps stamps;
    bool do_not_update = false;
    int refs = 0;

    void load()
    {
        if (refs++ > 0)
            return;

        stamps = getServiceDatabase().getFileStamps();
    }

    void save()
    {
        if (--refs > 0)
            return;

        getServiceDatabase().setFileStamps(stamps);
    }

    void clear()
    {
        stamps.clear();
        getServiceDatabase().clearFileStamps();
    }
};

static AccessData data;

AccessTable::AccessTable(const path &cfg_dir)
    : root_dir(cfg_dir.parent_path())
{
    data.load();
}

AccessTable::~AccessTable()
{
    data.save();
}

bool AccessTable::must_update_contents(const path &p) const
{
    if (!fs::exists(p))
        return true;
    if (data.do_not_update)
        return false;
    if (!isUnderRoot(p))
        return true;
    return fs::last_write_time(p) != data.stamps[p];
}

bool AccessTable::updates_disabled() const
{
    return data.do_not_update;
}

void AccessTable::update_contents(const path &p, const String &s) const
{
    write_file_if_different(p, s);
    data.stamps[p] = fs::last_write_time(p);
}

void AccessTable::write_if_older(const path &p, const String &s) const
{
    if (!isUnderRoot(p))
    {
        write_file_if_different(p, s);
        return;
    }
    if (must_update_contents(p))
        update_contents(p, s);
}

void AccessTable::clear() const
{
    data.clear();
}

bool AccessTable::isUnderRoot(path p) const
{
    return is_under_root(p, root_dir);
}

void AccessTable::remove(const path &p) const
{
    std::set<path> rm;
    for (auto &s : data.stamps)
    {
        if (is_under_root(s.first, p))
            rm.insert(s.first);
    }
    for (auto &s : rm)
        data.stamps.erase(s);
}

void AccessTable::do_not_update_files(bool v)
{
    data.do_not_update = v;
}