//
//  Database.hh
//  CBForest
//
//  Created by Jens Alfke on 5/12/14.
//  Copyright (c) 2014 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#ifndef __CBNano__Database__
#define __CBNano__Database__
#include "KeyStore.hh"
#include <vector>
#include <unordered_map>
#include <atomic> // for std::atomic_uint
#ifdef check
#undef check
#endif

namespace cbforest {

    using namespace std;

    class Transaction;


    /** A database file, primarily a container of KeyStores which store the actual data.
        This is an abstract class, with concrete subclasses for different database engines. */
    class Database {
    public:

        enum EncryptionAlgorithm : uint8_t {
            kNoEncryption = 0,      /**< No encryption (default) */
            kAES256                 /**< AES with 256-bit key */
        };

        struct Options {
            KeyStore::Capabilities keyStores;
            bool create         :1;     //< Should the db be created if it doesn't exist?
            bool writeable      :1;     //< If false, db is opened read-only
            EncryptionAlgorithm encryptionAlgorithm;
            alloc_slice encryptionKey;

            static const Options defaults;
        };

        Database(const string &path, const Options* =nullptr);
        virtual ~Database();

        const string& filename() const;
        const Options& options() const              {return _options;}

        virtual bool isOpen() const =0;

        /** Closes the database. Do not call any methods on this object afterwards,
            except isOpen() or mustBeOpen(), before deleting it. */
        virtual void close();

        /** Reopens database after it's been closed. */
        virtual void reopen() =0;

        /** Closes the database and deletes its file. */
        virtual void deleteDatabase() =0;

        /** Deletes a database that isn't open. */
        static void deleteDatabase(const string &path);

        virtual void compact() =0;
        bool isCompacting() const;
        static bool isAnyCompacting();

        typedef function<void(bool compacting)> OnCompactCallback;

        void setOnCompact(OnCompactCallback callback)   {_onCompactCallback = callback;}

        virtual bool setAutoCompact(bool autoCompact)   {return false;}

        virtual void rekey(EncryptionAlgorithm, slice newKey);

        /** The number of deletions that have been purged via compaction. (Used by the indexer) */
        uint64_t purgeCount() const;

        //////// KEY-STORES:

        static const string kDefaultKeyStoreName;

        /** The Database's default key-value store. */
        KeyStore& defaultKeyStore() const           {return defaultKeyStore(_options.keyStores);}
        KeyStore& defaultKeyStore(KeyStore::Capabilities) const;

        KeyStore& getKeyStore(const string &name) const;
        KeyStore& getKeyStore(const string &name, KeyStore::Capabilities) const;

        /** The names of all existing KeyStores (whether opened yet or not) */
        virtual vector<string> allKeyStoreNames() =0;

        void closeKeyStore(const string &name);

        /** Permanently deletes a KeyStore. */
        virtual void deleteKeyStore(const string &name) =0;

    protected:
        /** Override to instantiate a KeyStore object. */
        virtual KeyStore* newKeyStore(const string &name, KeyStore::Capabilities) =0;

        /** Override to begin a database transaction. */
        virtual void _beginTransaction(Transaction*) =0;

        /** Override to commit or abort a database transaction. */
        virtual void _endTransaction(Transaction*) =0;

        /** Is this Database object currently in a transaction? */
        bool inTransaction() const                  {return _inTransaction;}

        /** Runs the function/lambda while holding the file lock. This doesn't create a real
            transaction (at the ForestDB/SQLite/etc level), but it does ensure that no other thread
            is in a transaction, nor starts a transaction while the function is running. */
        void withFileLock(function<void(void)> fn);

        void updatePurgeCount(Transaction&);

        void beganCompacting();
        void finishedCompacting();

    private:
        class File;
        friend class KeyStore;
        friend class Transaction;

        KeyStore& addKeyStore(const string &name, KeyStore::Capabilities);
        void beginTransaction(Transaction*);
        void endTransaction(Transaction*);

        Database(const Database&) = delete;
        Database& operator=(const Database&) = delete;

        void incrementDeletionCount(Transaction &t);

        File* const             _file;                          // Shared state of file (lock)
        const Options           _options;                       // Option/capability flags
        KeyStore*               _defaultKeyStore {nullptr};     // The default KeyStore
        unordered_map<string, unique_ptr<KeyStore> > _keyStores;// Opened KeyStores
        bool _inTransaction     {false};                        // Am I in a Transaction?
        OnCompactCallback       _onCompactCallback {nullptr};   // Client callback for compacts
    };


    /** Grants exclusive write access to a Database while in scope.
        The transaction is committed when the object exits scope, unless abort() was called.
        Only one Transaction object can be created on a database file at a time.
        Not just per Database object; per database _file_. */
    class Transaction {
    public:
        enum state {
            kNoOp,
            kAbort,
            kCommit,
            kCommitManualWALFlush
        };

        explicit Transaction(Database*);
        Transaction(Database &db)               :Transaction(&db) { }
        ~Transaction()                          {_db.endTransaction(this);}

        Database& database() const          {return _db;}
        state state() const                 {return _state;}

        /** Tells the Transaction that it should rollback, not commit, when exiting scope. */
        void abort()                        {if (_state != kNoOp) _state = kAbort;}

        /** Force the database write-ahead log to be completely flushed on commit. */
        void flushWAL()                     {if (_state == kCommit) _state = kCommitManualWALFlush;}

    private:
        friend class Database;
        friend class KeyStore;

        void incrementDeletionCount()       {_db.incrementDeletionCount(*this);}

        Transaction(Database*, bool begin);
        Transaction(const Transaction&) = delete;

        Database&   _db;        // The Database
        enum state  _state;     // Remembers what action to take on destruct
    };
    
}

#endif /* defined(__CBNano__Database__) */
