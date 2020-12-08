function(set_litecore_source_base)
    set(oneValueArgs RESULT)
    cmake_parse_arguments(BASE_SSS ""  ${oneValueArgs} "" ${ARGN})
    if(NOT DEFINED BASE_SSS_RESULT)
        message(FATAL_ERROR set_source_files_base needs to be called with RESULT)
    endif()

    set(LITECORE_SHARED_LINKER_FLAGS "" CACHE INTERNAL "")
    set(LITECORE_C_FLAGS "" CACHE INTERNAL "")
    SET(LITECORE_CXX_FLAGS "" CACHE INTERNAL "")

    # Error.cc will be added here *and* in Support because the Support version is a stub
    # That goes through the C API of LiteCore.  If the stub were compiled into LiteCore
    # itself, that's an infinite recursive call
    set(
        ${BASE_SSS_RESULT}
        C/c4Base.cc
        C/c4BlobStore.cc
        C/c4Certificate.cc
        C/c4Database.cc
        C/c4DocEnumerator.cc
        C/c4DocExpiration.cc
        C/c4Document.cc
        C/c4Observer.cc
        C/c4PredictiveQuery.cc
        C/c4Query.cc
        Crypto/SecureRandomize.cc
        Crypto/mbedUtils.cc
        Crypto/mbedSnippets.cc
        Crypto/Certificate.cc
        Crypto/PublicKey.cc
        Crypto/SecureDigest.cc
        Crypto/SecureSymmetricCrypto.cc
        LiteCore/BlobStore/BlobStore.cc
        LiteCore/BlobStore/Stream.cc
        LiteCore/Database/BackgroundDB.cc
        LiteCore/Database/Database.cc
        LiteCore/Database/Document.cc
        LiteCore/Database/Housekeeper.cc
        LiteCore/Database/LeafDocument.cc
        LiteCore/Database/LegacyAttachments.cc
        LiteCore/Database/LiveQuerier.cc
        LiteCore/Database/PrebuiltCopier.cc
        LiteCore/Database/SequenceTracker.cc
        LiteCore/Database/TreeDocument.cc
        LiteCore/Database/Upgrader.cc
        LiteCore/Query/IndexSpec.cc
        LiteCore/Query/PredictiveModel.cc
        LiteCore/Query/Query.cc
        LiteCore/Query/QueryParser+Prediction.cc
        LiteCore/Query/QueryParser.cc
        LiteCore/Query/SQLiteDataFile+Indexes.cc
        LiteCore/Query/SQLiteFleeceEach.cc
        LiteCore/Query/SQLiteFleeceFunctions.cc
        LiteCore/Query/SQLiteFleeceUtil.cc
        LiteCore/Query/SQLiteFTSRankFunction.cc
        LiteCore/Query/SQLiteKeyStore+ArrayIndexes.cc
        LiteCore/Query/SQLiteKeyStore+FTSIndexes.cc
        LiteCore/Query/SQLiteKeyStore+Indexes.cc
        LiteCore/Query/SQLiteKeyStore+PredictiveIndexes.cc
        LiteCore/Query/SQLiteN1QLFunctions.cc
        LiteCore/Query/SQLitePredictionFunction.cc
        LiteCore/Query/SQLiteQuery.cc
        LiteCore/Query/N1QL_Parser/n1ql.cc
        LiteCore/RevTrees/RawRevTree.cc
        LiteCore/RevTrees/RevID.cc
        LiteCore/RevTrees/RevTree.cc
        LiteCore/RevTrees/VersionedDocument.cc
        LiteCore/Storage/DataFile.cc
        LiteCore/Storage/KeyStore.cc
        LiteCore/Storage/Record.cc
        LiteCore/Storage/RecordEnumerator.cc
        LiteCore/Storage/SQLiteDataFile.cc
        LiteCore/Storage/SQLiteEnumerator.cc
        LiteCore/Storage/SQLiteKeyStore.cc
        LiteCore/Storage/UnicodeCollator.cc
        Networking/Address.cc
        Networking/HTTP/CookieStore.cc
        vendor/SQLiteCpp/src/Backup.cpp
        vendor/SQLiteCpp/src/Column.cpp
        vendor/SQLiteCpp/src/Database.cpp
        vendor/SQLiteCpp/src/Exception.cpp
        vendor/SQLiteCpp/src/Statement.cpp
        vendor/SQLiteCpp/src/Transaction.cpp
        Replicator/c4Replicator.cc
        Replicator/c4Socket.cc
        Replicator/ChangesFeed.cc
        Replicator/Checkpoint.cc
        Replicator/Checkpointer.cc
        Replicator/DatabaseCookies.cc
        Replicator/DBAccess.cc
        Replicator/IncomingRev.cc
        Replicator/IncomingRev+Blobs.cc
        Replicator/Inserter.cc
        Replicator/Puller.cc
        Replicator/Pusher.cc
        Replicator/Pusher+Attachments.cc
        Replicator/Pusher+Revs.cc
        Replicator/Replicator.cc
        Replicator/ReplicatorTypes.cc
        Replicator/RevFinder.cc
        Replicator/Worker.cc
        LiteCore/Support/c4ExceptionUtils.cc
        LiteCore/Support/Logging.cc
        LiteCore/Support/DefaultLogger.cc
        LiteCore/Support/Error.cc
        LiteCore/Support/EncryptedStream.cc
        LiteCore/Support/FilePath.cc
        LiteCore/Support/LogDecoder.cc
        LiteCore/Support/LogEncoder.cc
        LiteCore/Support/PlatformIO.cc
        LiteCore/Support/StringUtil.cc
        LiteCore/Support/ChannelManifest.cc
        PARENT_SCOPE
    )
endfunction()
