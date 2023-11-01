//
// c4Base.cc
//
// Copyright 2016-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "c4Base.h"
#include "c4Private.h"
#include "c4Internal.hh"
#include "c4ExceptionUtils.hh"

#include "Actor.hh"
#include "Backtrace.hh"
#include "KeyStore.hh"
#include "Logging.hh"
#include "StringUtil.hh"
#include "WebSocketInterface.hh"  // For websocket::WSLogDomain
#include "fleece/Fleece.hh"
#include "fleece/InstanceCounted.hh"
#include "Encoder.hh"
#include "sqlite3.h"
#include "repo_version.h"  // Generated by get_repo_version.sh at build time
#include "ParseDate.hh"
#include "UnicodeCollator.hh"
#include <cctype>
#include <csignal>
#include <algorithm>
#include <set>

#ifdef _MSC_VER
#    include <winerror.h>
#endif


using namespace std;
using namespace litecore;


extern "C" {
CBL_CORE_API std::atomic_int gC4ExpectExceptions;
bool                         C4ExpectingExceptions();

bool C4ExpectingExceptions() { return gC4ExpectExceptions > 0; }  // LCOV_EXCL_LINE
}

// LCOV_EXCL_START
static string getBuildInfo() {
    static string commit;
#ifdef COUCHBASE_ENTERPRISE
    if ( commit.empty() ) { commit = format("%.16s+%.16s", GitCommitEE, GitCommit); }
    static const char* ee = "EE ";
#else
    if ( commit.empty() ) { commit = format("%.16s", GitCommit); }
    static const char* ee = "";
#endif
#if LiteCoreOfficial
    return format("%sbuild number %s, ID %.8s, from commit %s", ee, LiteCoreBuildNum, LiteCoreBuildID, commit.c_str());
#else
    if ( strcmp(GitBranch, "HEAD") == (0) )
        return format("%sbuilt from commit %s%s on %s %s", ee, commit.c_str(), GitDirty, __DATE__, __TIME__);
    else
        return format("%sbuilt from %s branch, commit %s%s on %s %s", ee, GitBranch, commit.c_str(), GitDirty, __DATE__,
                      __TIME__);
#endif
}

C4StringResult c4_getBuildInfo() C4API { return toSliceResult(getBuildInfo()); }

C4StringResult c4_getVersion() C4API {
    string vers;
#if LiteCoreOfficial
    vers = format("%s (%s)", LiteCoreVersion, LiteCoreBuildNum);
#else
#    ifdef COUCHBASE_ENTERPRISE
    static const char* ee     = "-EE";
    string             commit = format("%.16s+%.16s", GitCommitEE, GitCommit);
#    else
    static const char* ee     = "";
    string             commit = format("%.16s", GitCommit);
#    endif
    if ( strcmp(GitBranch, "master") == (0) || strcmp(GitBranch, "HEAD") == (0) )
        vers = format("%s%s (%s%.1s)", LiteCoreVersion, ee, commit.c_str(), GitDirty);
    else
        vers = format("%s%s (%s:%s%.1s)", LiteCoreVersion, ee, GitBranch, commit.c_str(), GitDirty);
#endif
    return toSliceResult(vers);
}

C4SliceResult c4_getEnvironmentInfo() C4API {
    fleece::Encoder e;
    e.beginDict(2);
    e.writeKey(FLSTR(kC4EnvironmentTimezoneKey));
    time_t now;
    time(&now);
    chrono::seconds offset = fleece::GetLocalTZOffset(localtime(&now), false);
    e.writeInt(offset.count());
    e.writeKey(FLSTR(kC4EnvironmentSupportedLocales));

    auto locales = SupportedLocales();
    e.beginArray(locales.size());
    for ( const auto& locale : locales ) { e.writeString(locale); }
    e.endArray();
    e.endDict();

    return C4SliceResult(e.finish());
}

// LCOV_EXCL_STOP


C4Timestamp c4_now(void) C4API { return KeyStore::now(); }

#pragma mark - SLICES:

namespace litecore {

    C4SliceResult toSliceResult(const string& str) { return C4SliceResult(alloc_slice(str)); }

    void destructExtraInfo(C4ExtraInfo& x) noexcept {
        if ( x.destructor ) {
            x.destructor(x.pointer);
            x.destructor = nullptr;
        }
        x.pointer = nullptr;
    }

}  // namespace litecore

#pragma mark - LOGGING:

// LCOV_EXCL_START
void c4log_writeToCallback(C4LogLevel level, C4LogCallback callback, bool preformatted) noexcept {
    LogDomain::setCallback((LogDomain::Callback_t)callback, preformatted);
    LogDomain::setCallbackLogLevel((LogLevel)level);
}

C4LogCallback c4log_getCallback() noexcept { return (C4LogCallback)LogDomain::currentCallback(); }

// LCOV_EXCL_STOP

bool c4log_writeToBinaryFile(C4LogFileOptions options, C4Error* outError) noexcept {
    return tryCatch(outError, [=] {
        LogFileOptions lfOptions{slice(options.base_path).asString(), (LogLevel)options.log_level,
                                 options.max_size_bytes, options.max_rotate_count, options.use_plaintext};

        const string header = options.header.buf != nullptr ? slice(options.header).asString()
                                                            : string("Generated by LiteCore ") + getBuildInfo();
        LogDomain::writeEncodedLogsTo(lfOptions, header);
    });
}

C4LogLevel c4log_callbackLevel() noexcept { return (C4LogLevel)LogDomain::callbackLogLevel(); }  // LCOV_EXCL_LINE

C4LogLevel c4log_binaryFileLevel() noexcept { return (C4LogLevel)LogDomain::fileLogLevel(); }

void c4log_setCallbackLevel(C4LogLevel level) noexcept {
    LogDomain::setCallbackLogLevel((LogLevel)level);
}  //LCOV_EXCL_LINE

void c4log_setBinaryFileLevel(C4LogLevel level) noexcept { LogDomain::setFileLogLevel((LogLevel)level); }

C4StringResult c4log_binaryFilePath(void) C4API {
    auto options = LogDomain::currentLogFileOptions();
    if ( !options.path.empty() && !options.isPlaintext ) return C4StringResult(alloc_slice(options.path));
    else
        return {};
}

// NOLINTBEGIN(misc-misplaced-const,cppcoreguidelines-interfaces-global-init)
CBL_CORE_API C4LogDomain const kC4DefaultLog   = (C4LogDomain)&kC4Cpp_DefaultLog;
CBL_CORE_API C4LogDomain const kC4DatabaseLog  = (C4LogDomain)&DBLog;
CBL_CORE_API C4LogDomain const kC4QueryLog     = (C4LogDomain)&QueryLog;
CBL_CORE_API C4LogDomain const kC4SyncLog      = (C4LogDomain)&SyncLog;
CBL_CORE_API C4LogDomain const kC4WebSocketLog = (C4LogDomain)&websocket::WSLogDomain;

// NOLINTEND(misc-misplaced-const,cppcoreguidelines-interfaces-global-init)

C4LogDomain c4log_getDomain(const char* name, bool create) noexcept {
    if ( !name ) return kC4DefaultLog;
    auto domain = LogDomain::named(name);
    if ( !domain && create ) domain = new LogDomain(name);
    return (C4LogDomain)domain;
}

const char* c4log_getDomainName(C4LogDomain c4Domain) noexcept {
    auto domain = (LogDomain*)c4Domain;
    return domain->name();
}

C4LogLevel c4log_getLevel(C4LogDomain c4Domain) noexcept {
    auto domain = (LogDomain*)c4Domain;
    return (C4LogLevel)domain->level();
}

void c4log_setLevel(C4LogDomain c4Domain, C4LogLevel level) noexcept {
    auto domain = (LogDomain*)c4Domain;
    domain->setLevel((LogLevel)level);
}

bool c4log_willLog(C4LogDomain c4Domain, C4LogLevel level) C4API {
    auto domain = (LogDomain*)c4Domain;
    return domain->willLog((LogLevel)level);
}

void c4log_warnOnErrors(bool warn) noexcept { error::sWarnOnError = warn; }

bool c4log_getWarnOnErrors() noexcept { return error::sWarnOnError; }

void c4log_enableFatalExceptionBacktrace() C4API {
    fleece::Backtrace::installTerminateHandler([](const string& backtrace) {
        c4log(kC4DefaultLog, kC4LogError,
              "COUCHBASE LITE CORE FATAL ERROR (backtrace follows)\n"
              "********************\n"
              "%s\n"
              "******************** NOW TERMINATING",
              backtrace.c_str());
    });
}

void c4log_flushLogFiles() C4API { LogDomain::flushLogFiles(); }

void c4log(C4LogDomain c4Domain, C4LogLevel level, const char* fmt, ...) noexcept {
    va_list args;
    va_start(args, fmt);
    c4vlog(c4Domain, level, fmt, args);
    va_end(args);
}

void c4vlog(C4LogDomain c4Domain, C4LogLevel level, const char* fmt, va_list args) noexcept {
    try {
        ((LogDomain*)c4Domain)->vlog((LogLevel)level, fmt, args);
    } catch ( ... ) {}
}

// LCOV_EXCL_START
void c4slog(C4LogDomain c4Domain, C4LogLevel level, C4Slice msg) noexcept {
    if ( msg.buf == nullptr ) { return; }

    try {
        ((LogDomain*)c4Domain)->logNoCallback((LogLevel)level, "%.*s", SPLAT(msg));
    } catch ( ... ) {}
}

// LCOV_EXCL_STOP


__cold void C4Error::warnCurrentException(const char* inFunction) noexcept {
    C4WarnError("Caught & ignored exception %s in %s", C4Error::fromCurrentException().description().c_str(),
                inFunction);
}

#pragma mark - REFERENCE COUNTED:

void* c4base_retain(void* obj) C4API { return retain((RefCounted*)obj); }

void c4base_release(void* obj) C4API { release((RefCounted*)obj); }

#pragma mark - INSTANCE COUNTED:

int c4_getObjectCount() noexcept { return fleece::InstanceCounted::liveInstanceCount(); }

// LCOV_EXCL_START
void c4_dumpInstances(void) C4API {
#if INSTANCECOUNTED_TRACK
    fleece::InstanceCounted::dumpInstances([](const fleece::InstanceCounted* obj) {
        if ( auto logger = dynamic_cast<const Logging*>(obj); logger )
            fprintf(stderr, "%s, ", logger->loggingName().c_str());
        fprintf(stderr, "a ");
    });
#endif
}

#pragma mark - MISCELLANEOUS:

bool c4_setTempDir(C4String path, C4Error* err) C4API {
    if ( sqlite3_temp_directory != nullptr ) {
        c4error_return(LiteCoreDomain, kC4ErrorUnsupported, C4STR("c4_setTempDir cannot be called more than once!"),
                       err);
        return false;
    }


    sqlite3_temp_directory = (char*)sqlite3_malloc((int)path.size + 1);
    memcpy(sqlite3_temp_directory, path.buf, path.size);
    sqlite3_temp_directory[path.size] = 0;
    return true;
}

// LCOV_EXCL_STOP


void c4_runAsyncTask(void (*task)(void*), void* context) C4API { actor::Mailbox::runAsyncTask(task, context); }
