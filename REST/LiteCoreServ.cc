//
//  LiteCoreServ.cc
//  LiteCore
//
//  Created by Jens Alfke on 4/17/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "c4REST.h"
#include "FilePath.hh"
#include <chrono>
#include <iostream>
#include <thread>

using namespace std;
using namespace litecore;


static C4RESTListener *gListener;

static C4RESTConfig gRESTConfig;

static C4DatabaseConfig gDatabaseConfig = {
    kC4DB_Bundled | kC4DB_SharedKeys
};

static string gDirectory;


static inline string to_string(C4String s) {
    return string((const char*)s.buf, s.size);
}

static inline C4Slice c4str(const string &s) {
    return {s.data(), s.size()};
}


static void usage() {
    cerr << "Usage: LiteCoreServ <options> <dbpath> ...  (serves each database)\n"
            "   or: LiteCoreServ <options> --dir <dir>   (serves all databases in <dir>)\n"
            "Options:\n"
            "       --port <n>         Listen on TCP port <n> (default is 59840)\n"
            "       --create           Create database(s) that don't exist\n"
            "       --readonly         Open database(s) read-only\n";
}


static void fail(const char *message) {
    cerr << "Error: " << message << "\n";
    exit(1);
}


static void fail(const char *what, C4Error err) {
    auto message = c4error_getMessage(err);
    cerr << "Error " << what << ": ";
    if (message.buf)
        cerr << to_string(message);
    cerr << "(" << err.domain << "/" << err.code << ")\n";
    exit(1);
}


static void failMisuse(const char *message ="Invalid parameters") {
    cerr << "Error: " << message << "\n";
    usage();
    exit(1);
}


static string databaseNameFromPath(const char *path) {
    C4StringResult nameSlice = c4rest_databaseNameFromPath(c4str(path));
    if (!nameSlice.buf)
        return string();
    string name((char*)nameSlice.buf, nameSlice.size);
    c4slice_free(nameSlice);
    return name;
}


static void startListener() {
    C4Error err;
    if (!gListener) {
        gListener = c4rest_start(&gRESTConfig, &err);
        if (!gListener)
            fail("starting REST listener", err);
    }
}


static void shareDatabase(const char *path, string name) {
    startListener();
    C4Error err;
    auto db = c4db_open(c4str(path), &gDatabaseConfig, &err);
    if (!db)
        fail("opening database", err);
    c4rest_shareDB(gListener, c4str(name), db);
    c4db_free(db);
}


static void shareDatabaseDir(const char *dirPath) {
    gDirectory = dirPath;
    gRESTConfig.directory = c4str(gDirectory);
    cerr << "Sharing all databases in " << dirPath << ": ";
    int n = 0;
    FilePath dir(dirPath, "");
    dir.forEachFile([=](const FilePath &file) mutable {
        if (file.extension() == kC4DatabaseFilenameExtension && file.existsAsDir()) {
            if (n++) cerr << ", ";
            auto dbPath = file.path().c_str();
            string name = databaseNameFromPath(dbPath);
            if (!name.empty()) {
                cerr << name;
                shareDatabase(dbPath, name);
            }
        }
    });
    cerr << "\n";
    if (n == 0)
        fail("No databases found");
}


int main(int argc, const char** argv) {
    gRESTConfig.port = 59840;
    gRESTConfig.allowCreateDBs = gRESTConfig.allowDeleteDBs = true;

    try {
        auto restLog = c4log_getDomain("REST", true);
        c4log_setLevel(restLog, kC4LogInfo);

        for (int i = 1; i < argc; ++i) {
            auto arg = argv[i];
            if (arg[0] == '-') {
                // Flags:
                while (arg[0] == '-')
                    ++arg;
                string flag = arg;
                if (flag == "help") {
                    usage();
                    exit(0);
                } else if (flag == "dir") {
                    if (++i >= argc)
                        failMisuse();
                    shareDatabaseDir(argv[i]);
                } else if (flag == "port") {
                    if (++i >= argc)
                        failMisuse();
                    gRESTConfig.port = (uint16_t) stoi(string(argv[i]));
                } else if (flag == "readonly") {
                    gDatabaseConfig.flags |= kC4DB_ReadOnly;
                    gRESTConfig.allowCreateDBs = gRESTConfig.allowDeleteDBs = false;
                } else if (flag == "create") {
                    gDatabaseConfig.flags |= kC4DB_Create;
                } else {
                    failMisuse("Unknown flag");
                }
            } else {
                // Paths:
                string name = databaseNameFromPath(arg);
                if (name.empty())
                    fail("Invalid database name");
                cerr << "Sharing database '" << name << "' from " << arg << " ...\n";
                shareDatabase(arg, name);
            }
        }

        if (!gListener) {
            failMisuse("You must provide the path to at least one Couchbase Lite database to share.");
            exit(1);
        }
    } catch (const exception &x) {
        cerr << "\n";
        fail(x.what());
    }

    cerr << "LiteCoreServ is now listening at http://localhost:" << gRESTConfig.port << "/ ...\n";

    // Sleep to keep the process from exiting, while the server threads run:
    while(true)
        this_thread::sleep_for(chrono::hours(1000));
    return 0;
}
