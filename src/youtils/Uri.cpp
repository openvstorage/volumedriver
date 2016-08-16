// Copyright (C) 2016 iNuron NV
//
// This file is part of Open vStorage Open Source Edition (OSE),
// as available from
//
//      http://www.openvstorage.org and
//      http://www.openvstorage.com.
//
// This file is free software; you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License v3 (GNU AGPLv3)
// as published by the Free Software Foundation, in version 3 as it comes in
// the LICENSE.txt file of the Open vStorage OSE distribution.
// Open vStorage is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY of any kind.

#include "Catchers.h"
#include "ScopeExit.h"
#include "Uri.h"

#include <uriparser/Uri.h>

#include <boost/lexical_cast.hpp>
#include <boost/optional/optional_io.hpp>

namespace youtils
{

using namespace std::literals::string_literals;

Uri::Uri(const std::string& s)
{
    *this = boost::lexical_cast<Uri>(s);
}

bool
Uri::operator==(const Uri& other) const
{
#define CMP(x)                                  \
    (x == other.x)

    return
        CMP(scheme_) and
        CMP(user_info_) and
        CMP(host_) and
        CMP(port_) and
        CMP(path_segments_) and
        CMP(query_) and
        CMP(fragment_) and
        CMP(absolute_path_);

#undef CMP
}

std::string
Uri::path() const
{
    std::string s;

    if (absolute_path())
    {
        s += "/"s;
    }

    bool first = true;
    for (const auto& p : path_segments())
    {
        if (first)
        {
            s += p;
            first = false;
        }
        else
        {
            s += "/"s + p;
        }
    }

    return s;
}

namespace
{

DECLARE_LOGGER("UriUtils");

UriTextRangeA
make_text_range(const char* start,
                const char* end)
{
    UriTextRangeA r;
    r.first = start;
    r.afterLast = end;
    return r;
}

UriTextRangeA
make_text_range(const std::string& s)
{
    return make_text_range(s.c_str(),
                           s.c_str() + s.size());
}

UriTextRangeA
make_text_range(const boost::optional<std::string>& s)
{
    if (s)
    {
        return make_text_range(*s);
    }
    else
    {
        return make_text_range(nullptr,
                               nullptr);
    }
}

UriPathSegmentA
make_path_segment(const std::string& s,
                  UriPathSegmentA* next)
{
    UriPathSegmentA seg;
    seg.text = make_text_range(s);
    seg.next = next;
    seg.reserved = nullptr;

    return seg;
}

std::vector<UriQueryListA>
get_query_list(const Uri::Query& query)
{
    std::vector<UriQueryListA> vec(query.size());

    UriQueryListA* prev = nullptr;
    ssize_t idx = vec.size() - 1;

    for (const auto& q : query)
    {
        VERIFY(idx >= 0);
        UriQueryListA& u = vec[idx];
        u.key = q.first.c_str();
        u.value = q.second ? q.second->c_str() : nullptr;
        u.next = prev;
        prev = vec.data() + idx;
        idx -= 1;

    }

    VERIFY(idx == -1);

    return vec;
}

std::ostream&
stream_out(std::ostream& os,
           const UriUriA& u)
{
    int size = 0;
    int ret = uriToStringCharsRequiredA(&u,
                                        &size);
    VERIFY(ret == URI_SUCCESS);

    std::vector<char> vec(size + 1);

    ret = uriToStringA(vec.data(),
                       &u,
                       vec.size(),
                       nullptr);

    VERIFY(ret == URI_SUCCESS);

    return os << vec.data();
}

std::ostream&
stream_out(std::ostream& os,
           const Uri& uri)
{
    const UriHostDataA host_data = { nullptr,
                                     nullptr,
                                     make_text_range(nullptr,
                                                     nullptr)
    };

    const boost::optional<std::string> portopt(uri.port() ?
                                               boost::optional<std::string>(boost::lexical_cast<std::string>(*uri.port())) :
                                               boost::none);

    UriUriA u;
    u.scheme = make_text_range(uri.scheme());
    u.userInfo = make_text_range(uri.user_info());
    u.hostText = make_text_range(uri.host());
    u.hostData = host_data;
    u.portText = make_text_range(portopt);
    u.pathHead = nullptr;
    u.pathTail = nullptr;

    const std::vector<std::string>& path = uri.path_segments();
    std::vector<UriPathSegmentA> psegments;
    psegments.reserve(path.size());

    ssize_t idx = -1;

    for (auto it = path.rbegin(); it != path.rend(); ++it)
    {
        psegments.emplace_back(make_path_segment(*it,
                                                 (idx < 0) ?
                                                 nullptr :
                                                 psegments.data() + idx));

        u.pathTail = u.pathHead;
        u.pathHead = &psegments.back();
        ++idx;
    }

    u.owner = false;

    const std::vector<UriQueryListA> qvec(get_query_list(uri.query()));
    std::vector<char> qstr;

    if (not qvec.empty())
    {
        int qsize = 0;
        int ret = uriComposeQueryCharsRequiredA(qvec.data(),
                                                &qsize);
        VERIFY(ret == URI_SUCCESS);
        VERIFY(qsize > 0);

        qstr.resize(qsize + 1);

        int written = 0;
        ret = uriComposeQueryA(qstr.data(),
                               qvec.data(),
                               qstr.size(),
                               &written);

        VERIFY(ret == URI_SUCCESS);
        VERIFY(written <= static_cast<ssize_t>(qstr.size()));

        // for some reason qsize is overzealous, so we need to use written.
        u.query = make_text_range(qstr.data(),
                                  qstr.data() + written);
    }
    else
    {
        u.query = make_text_range(nullptr,
                                  nullptr);
    }

    u.fragment = make_text_range(uri.fragment());
    u.absolutePath = uri.absolute_path();

    return stream_out(os,
                      u);
}


boost::optional<std::string>
get_string_opt(const UriTextRangeA& r)
{
    if (r.first and r.first != r.afterLast)
    {
        return std::string(r.first,
                           r.afterLast);
    }
    else
    {
        return boost::none;
    }
}

boost::optional<uint16_t>
get_port_opt(const UriUriA& u)
{
    const auto s(get_string_opt(u.portText));
    if (s)
    {
        return boost::lexical_cast<uint16_t>(*s);
    }
    else
    {
        return boost::none;
    }
}

Uri::Query
get_query(const UriUriA& u)
{
    Uri::Query query;

    if (u.query.first)
    {
        UriQueryListA* qlist = nullptr;
        int qsize = 0;

        int ret = uriDissectQueryMallocA(&qlist,
                                         &qsize,
                                         u.query.first,
                                         u.query.afterLast);

        auto cleanup_qlist(make_scope_exit([&]
                                           {
                                               uriFreeQueryListA(qlist);
                                           }));

        if (ret != URI_SUCCESS)
        {
            LOG_ERROR("Failed to dissect query " << get_string_opt(u.query));
            throw std::invalid_argument("failed to dissect query");
        }

        UriQueryListA* next = qlist;

        while (next)
        {
            VERIFY(next->key);

            const auto res(query.emplace(std::make_pair(next->key,
                                                        next->value ?
                                                        boost::optional<std::string>(next->value) :
                                                        boost::none)));
            if (not res.second)
            {
                LOG_ERROR("Invalid query - failed to insert key " << next->key);
                throw std::invalid_argument("failed to parse query");
            }

            next = next->next;
        }
    }

    return query;
}

std::vector<std::string>
get_path(const UriUriA& u)
{
    size_t n = 0;
    UriPathSegmentA* next = u.pathHead;

    while (next)
    {
        ++n;
        next = next->next;
    }

    std::vector<std::string> vec;
    vec.reserve(n);

    next = u.pathHead;

    while (next)
    {
        boost::optional<std::string> s(get_string_opt(next->text));
        if (s)
        {
            vec.emplace_back(std::move(*s));
        }
        next = next->next;
    }

    return vec;
}

Uri
parse_uri(const std::string& s)
{
    UriParserStateA state;
    UriUriA u;

    state.uri = &u;

    int ret = uriParseUriA(&state,
                           s.c_str());

    auto cleanup_uri(make_scope_exit([&]
                                     {
                                         uriFreeUriMembersA(&u);
                                     }));

    if (ret != URI_SUCCESS)
    {
        LOG_ERROR("Failed to parse " << s << " as URI: " << state.errorCode);
        throw std::invalid_argument("invalid URI string");
    }

    return Uri()
        .scheme(get_string_opt(u.scheme))
        .host(get_string_opt(u.hostText))
        .port(get_port_opt(u))
        .user_info(get_string_opt(u.userInfo))
        .path_segments(get_path(u))
        .query(get_query(u))
        .fragment(get_string_opt(u.fragment))
        .absolute_path(u.absolutePath)
        ;
}

}

std::ostream&
operator<<(std::ostream& os,
           const Uri& uri)
{
    try
    {
        stream_out(os,
                   uri);
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("Failed to stream out URI: " << EWHAT);
            os.setstate(std::ios_base::failbit);
        });

    return os;
}

std::istream&
operator>>(std::istream& is,
           Uri& uri)
{
    try
    {
        std::string s;
        is >> s;
        uri = parse_uri(s);
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("Failed to stream in URI: " << EWHAT);
            is.setstate(std::ios_base::failbit);
        });

    return is;
}

}
