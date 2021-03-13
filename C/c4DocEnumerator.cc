//
// c4DocEnumerator.cc
//
// Copyright (c) 2015 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "c4Internal.hh"
#include "c4DocEnumerator.hh"

#include "c4Database.hh"
#include "Document.hh"
#include "DataFile.hh"
#include "Record.hh"
#include "RecordEnumerator.hh"
#include "RevID.hh"
#include "VersionVector.hh"
#include "Error.hh"
#include "Logging.hh"
#include "InstanceCounted.hh"

using namespace c4Internal;


#pragma mark - DOC ENUMERATION:

CBL_CORE_API const C4EnumeratorOptions kC4DefaultEnumeratorOptions = {
    kC4IncludeNonConflicted | kC4IncludeBodies
};


class C4DocEnumerator::Impl : public RecordEnumerator, public fleece::InstanceCounted {
public:
    Impl(C4Database *database,
         sequence_t since,
         const C4EnumeratorOptions &options)
    :RecordEnumerator(asInternal(database)->defaultKeyStore(), since, recordOptions(options))
    ,_database(asInternal(database))
    ,_options(options)
    { }

    Impl(C4Database *database,
         const C4EnumeratorOptions &options)
    :RecordEnumerator(asInternal(database)->defaultKeyStore(), recordOptions(options))
    ,_database(asInternal(database))
    ,_options(options)
    { }

    static RecordEnumerator::Options recordOptions(const C4EnumeratorOptions &c4options)
    {
        RecordEnumerator::Options options;
        if (c4options.flags & kC4Descending)
            options.sortOption = kDescending;
        else if (c4options.flags & kC4Unsorted)
            options.sortOption = kUnsorted;
        options.includeDeleted = (c4options.flags & kC4IncludeDeleted) != 0;
        options.onlyConflicts  = (c4options.flags & kC4IncludeNonConflicted) == 0;
        if ((c4options.flags & kC4IncludeBodies) == 0)
            options.contentOption = kMetaOnly;
        else
            options.contentOption = kEntireBody;
        return options;
    }

    Retained<Document> getDoc() {
        if (!hasRecord())
            return nullptr;
        return _database->documentFactory().newDocumentInstance(record());
    }

    bool getDocInfo(C4DocumentInfo *outInfo) noexcept {
        if (!*this)
            return false;

        revid vers(record().version());
        if ((_options.flags & kC4IncludeRevHistory) && vers.isVersion())
            _docRevID = vers.asVersionVector().asASCII();
        else
            _docRevID = vers.expanded();

        outInfo->docID = record().key();
        outInfo->revID = _docRevID;
        outInfo->flags = (C4DocumentFlags)record().flags() | kDocExists;
        outInfo->sequence = record().sequence();
        outInfo->bodySize = record().bodySize();
        outInfo->metaSize = record().extraSize();
        outInfo->expiration = record().expiration();
        return true;
    }

private:
    Retained<DatabaseImpl> _database;
    C4EnumeratorOptions const _options;
    alloc_slice _docRevID;
};


C4DocEnumerator::C4DocEnumerator(C4Database *database,
                                 C4SequenceNumber since,
                                 const C4EnumeratorOptions &options)
:_impl(new Impl(database, since, options))
{ }

C4DocEnumerator::C4DocEnumerator(C4Database *database,
                                 const C4EnumeratorOptions &options)
:_impl(new Impl(database, options))
{ }

C4DocEnumerator::~C4DocEnumerator() = default;

bool C4DocEnumerator::getDocumentInfo(C4DocumentInfo &info) const noexcept {
    return _impl && _impl->getDocInfo(&info);
}

C4DocumentInfo C4DocEnumerator::documentInfo() const {
    C4DocumentInfo i;
    if (!getDocumentInfo(i))
        error::_throw(error::NotFound, "No more documents");
    return i;
}

Retained<C4Document> C4DocEnumerator::getDocument() const {
    return _impl ? _impl->getDoc() : nullptr;
}

bool C4DocEnumerator::next() {
    if (_impl && _impl->next())
        return true;
    _impl = nullptr;
    return false;
}

void C4DocEnumerator::close() noexcept {
    _impl = nullptr;
}
