//
//  Response.hh
//  LiteCore
//
//  Created by Jens Alfke on 4/20/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "slice.hh"
#include "FleeceCpp.hh"
#include "c4Base.h"
#include <functional>
#include <map>
#include <memory>
#include <sstream>

struct mg_connection;

namespace litecore { namespace REST {

    /** An incoming HTTP body. */
    class Body {
    public:
        fleece::slice header(const char *name) const;
        fleece::slice operator[] (const char *name) const   {return header(name);}

        bool hasContentType(fleece::slice contentType) const;
        fleece::alloc_slice body() const;
        fleeceapi::Value bodyAsJSON() const;

        // Utilities:
        static std::string urlDecode(const std::string&);
        static std::string urlEncode(const std::string&);

    protected:
        Body(mg_connection*);

        mg_connection* const _conn;
        bool _gotBody {false};
        fleece::alloc_slice _body;
        bool _gotBodyFleece {false};
        fleece::alloc_slice _bodyFleece;
    };


    /** An HTTP response from a server, created by specifying a request to send.
        I.e. this is a simple HTTP client API. */
    class Response : public Body {
    public:
        Response(const std::string &method,
                 const std::string &hostname,
                 uint16_t port,
                 const std::string &uri);

        ~Response();

        explicit operator bool() const      {return _conn != nullptr;}

        int status() const;
        std::string statusMessage() const;

    private:
        char _errorBuf[200];
    };

} }
