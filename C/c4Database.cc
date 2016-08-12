//
//  c4Database.cc
//  CBForest
//
//  Created by Jens Alfke on 9/8/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "c4Internal.hh"
#include "c4DatabaseInternal.hh"
#include "c4Database.h"
#include "c4Private.h"

#include "ForestDatabase.hh"
#include "SQLiteDatabase.hh"
#include "Collatable.hh"
#include "Document.hh"
#include "DocEnumerator.hh"


using namespace cbforest;


Database* c4Database::newDatabase(std::string path,
                                  C4DatabaseFlags flags,
                                  const C4EncryptionKey *encryptionKey,
                                  bool isMainDB)
{
    Database::Options options { };
    options.keyStores.sequences = options.keyStores.softDeletes = isMainDB;
    options.create = (flags & kC4DB_Create) != 0;
    options.writeable = (flags & kC4DB_ReadOnly) == 0;
    if (encryptionKey) {
        options.encryptionAlgorithm = (Database::EncryptionAlgorithm)encryptionKey->algorithm;
        options.encryptionKey = alloc_slice(encryptionKey->bytes,
                                            sizeof(encryptionKey->bytes));
    }

    switch (flags & kC4DB_StorageTypeMask) {
        case kC4DB_ForestDBStorage:
            return new ForestDatabase(path, &options);
        case kC4DB_SQLiteStorage:
            return new SQLiteDatabase(path, &options);
        default:
            error::_throw(error::Unimplemented);
    }
}


#pragma mark - C4DATABASE CLASS:


c4Database::c4Database(std::string path,
                       C4DatabaseFlags flags_,
                       const C4EncryptionKey *encryptionKey)
:flags(flags_),
 _db(newDatabase(path, flags_, encryptionKey, true))
{ }

bool c4Database::mustBeSchema(int requiredSchema, C4Error *outError) {
    if (schema() == requiredSchema)
        return true;
    recordError(CBForestDomain, kC4ErrorUnsupported, outError);
    return false;
}

void c4Database::beginTransaction() {
#if C4DB_THREADSAFE
    _transactionMutex.lock(); // this is a recursive mutex
#endif
    if (++_transactionLevel == 1) {
        WITH_LOCK(this);
        _transaction = new Transaction(_db.get());
    }
}

bool c4Database::inTransaction() {
#if C4DB_THREADSAFE
    std::lock_guard<std::recursive_mutex> lock(_transactionMutex);
#endif
    return _transactionLevel > 0;
}

bool c4Database::mustBeInTransaction(C4Error *outError) {
    if (inTransaction())
        return true;
    recordError(CBForestDomain, kC4ErrorNotInTransaction, outError);
    return false;
}

bool c4Database::mustNotBeInTransaction(C4Error *outError) {
    if (!inTransaction())
        return true;
    recordError(CBForestDomain, kC4ErrorTransactionNotClosed, outError);
    return false;
}

bool c4Database::endTransaction(bool commit) {
#if C4DB_THREADSAFE
    std::lock_guard<std::recursive_mutex> lock(_transactionMutex);
#endif
    if (_transactionLevel == 0)
        return false;
    if (--_transactionLevel == 0) {
        WITH_LOCK(this);
        auto t = _transaction;
        _transaction = NULL;
        if (!commit)
            t->abort();
        delete t; // this commits/aborts the transaction
    }
#if C4DB_THREADSAFE
    _transactionMutex.unlock(); // undoes lock in beginTransaction()
#endif
    return true;
}


/*static*/ bool c4Database::rekey(Database* database, const C4EncryptionKey *newKey,
                                  C4Error *outError)
{
    try {
        if (newKey) {
            database->rekey((Database::EncryptionAlgorithm)newKey->algorithm,
                            slice(newKey->bytes, 32));
        } else {
            database->rekey(Database::kNoEncryption, slice::null);
        }
        return true;
    } catchError(outError);
    return false;
}


#pragma mark - DATABASE API:


C4Database* c4db_open(C4Slice path,
                      C4DatabaseFlags flags,
                      const C4EncryptionKey *encryptionKey,
                      C4Error *outError)
{
    try {
        if (flags & kC4DB_V2Format)
            return (new c4DatabaseV2((std::string)path, flags, encryptionKey))->retain();
        else
            return (new c4DatabaseV1((std::string)path, flags, encryptionKey))->retain();
    }catchError(outError);
    return NULL;
}


bool c4db_close(C4Database* database, C4Error *outError) {
    if (database == NULL)
        return true;
    if (!database->mustNotBeInTransaction(outError))
        return false;
    WITH_LOCK(database);
    try {
        database->db()->close();
        return true;
    } catchError(outError);
    return false;
}


bool c4db_free(C4Database* database) {
    if (database == NULL)
        return true;
    if (!database->mustNotBeInTransaction(NULL))
        return false;
    WITH_LOCK(database);
    try {
        database->release();
        return true;
    } catchError(NULL);
    return false;
}


bool c4db_delete(C4Database* database, C4Error *outError) {
    if (!database->mustNotBeInTransaction(outError))
        return false;
    WITH_LOCK(database);
    try {
        if (database->refCount() > 1) {
            recordError(CBForestDomain, kC4ErrorBusy, outError);
        }
        database->db()->deleteDatabase();
        return true;
    } catchError(outError);
    return false;
}


bool c4db_deleteAtPath(C4Slice dbPath, C4DatabaseFlags flags, C4Error *outError) {
    try {
        Database::deleteDatabase((std::string)dbPath);
        return true;
    } catchError(outError);
    return false;
}


bool c4db_compact(C4Database* database, C4Error *outError) {
    if (!database->mustNotBeInTransaction(outError))
        return false;
    WITH_LOCK(database);
    try {
        database->db()->compact();
        return true;
    } catchError(outError);
    return false;
}


bool c4db_isCompacting(C4Database *database) {
    return database ? database->db()->isCompacting() : Database::isAnyCompacting();
}

void c4db_setOnCompactCallback(C4Database *database, C4OnCompactCallback cb, void *context) {
    WITH_LOCK(database);
    database->db()->setOnCompact([cb,context](bool compacting) {
        cb(context, compacting);
    });
}


bool c4db_rekey(C4Database* database, const C4EncryptionKey *newKey, C4Error *outError) {
    if (!database->mustNotBeInTransaction(outError))
        return false;
    WITH_LOCK(database);
    return c4Database::rekey(database->db(), newKey, outError);
}


C4SliceResult c4db_getPath(C4Database *database) {
    slice path(database->db()->filename());
    path = path.copy();  // C4SliceResult must be malloced & adopted by caller
    return {path.buf, path.size};
}


C4DatabaseFlags c4db_getFlags(C4Database *database) {
    return database->flags;
}


uint64_t c4db_getDocumentCount(C4Database* database) {
    try {
        WITH_LOCK(database);
        auto opts = DocEnumerator::Options::kDefault;
        opts.contentOptions = kMetaOnly;

        uint64_t count = 0;
        for (DocEnumerator e(database->defaultKeyStore(), slice::null, slice::null, opts);
                e.next(); ) {
            C4DocumentFlags flags;
            if (database->readDocMeta(e.doc(), &flags) && !(flags & kDeleted))
                ++count;
        }
        return count;
    } catchError(NULL);
    return 0;
}


C4SequenceNumber c4db_getLastSequence(C4Database* database) {
    WITH_LOCK(database);
    try {
        return database->defaultKeyStore().lastSequence();
    } catchError(NULL);
    return 0;
}


bool c4db_isInTransaction(C4Database* database) {
    WITH_LOCK(database);
    return database->inTransaction();
}


bool c4db_beginTransaction(C4Database* database,
                           C4Error *outError)
{
    try {
        database->beginTransaction();
        return true;
    } catchError(outError);
    return false;
}

bool c4db_endTransaction(C4Database* database,
                         bool commit,
                         C4Error *outError)
{
    try {
        bool ok = database->endTransaction(commit);
        if (!ok)
            recordError(CBForestDomain, kC4ErrorNotInTransaction, outError);
        return ok;
    } catchError(outError);
    return false;
}


bool c4db_purgeDoc(C4Database *database, C4Slice docID, C4Error *outError) {
    WITH_LOCK(database);
    if (!database->mustBeInTransaction(outError))
        return false;
    try {
        if (database->defaultKeyStore().del(docID, *database->transaction()))
            return true;
        else
            recordError(CBForestDomain, kC4ErrorNotFound, outError);
    } catchError(outError)
    return false;
}

uint64_t c4db_nextDocExpiration(C4Database *database)
{
    try {
        WITH_LOCK(database);
        KeyStore& expiryKvs = database->getKeyStore("expiry");
        DocEnumerator e(expiryKvs);
        if(e.next() && e.doc().body() == slice::null) {
            // Look for an entry with a null body (otherwise, its key is simply a doc ID)
            CollatableReader r(e.doc().key());
            r.beginArray();
            return (uint64_t)r.readInt();
        }
    } catchError(NULL)
    return 0ul;
}

bool c4_shutdown(C4Error *outError) {
    try {
        ForestDatabase::shutdown();
        SQLiteDatabase::shutdown();
        return true;
    } catchError(NULL) {
        return false;
    }
}

#pragma mark - RAW DOCUMENTS:


void c4raw_free(C4RawDocument* rawDoc) {
    if (rawDoc) {
        c4slice_free(rawDoc->key);
        c4slice_free(rawDoc->meta);
        c4slice_free(rawDoc->body);
        delete rawDoc;
    }
}


C4RawDocument* c4raw_get(C4Database* database,
                         C4Slice storeName,
                         C4Slice key,
                         C4Error *outError)
{
    WITH_LOCK(database);
    try {
        KeyStore& localDocs = database->getKeyStore((std::string)storeName);
        Document doc = localDocs.get(key);
        if (!doc.exists()) {
            recordError(CBForestDomain, kC4ErrorNotFound, outError);
            return NULL;
        }
        auto rawDoc = new C4RawDocument;
        rawDoc->key = doc.key().copy();
        rawDoc->meta = doc.meta().copy();
        rawDoc->body = doc.body().copy();
        return rawDoc;
    } catchError(outError);
    return NULL;
}


bool c4raw_put(C4Database* database,
               C4Slice storeName,
               C4Slice key,
               C4Slice meta,
               C4Slice body,
               C4Error *outError)
{
    if (!c4db_beginTransaction(database, outError))
        return false;
    bool commit = false;
    try {
        WITH_LOCK(database);
        KeyStore &localDocs = database->getKeyStore((std::string)storeName);
        auto &t = *database->transaction();
        if (body.buf || meta.buf)
            localDocs.set(key, meta, body, t);
        else
            localDocs.del(key, t);
        commit = true;
    } catchError(outError);
    c4db_endTransaction(database, commit, outError);
    return commit;
}
