//
// QueryParser+VectorSearch.cc
//
// Copyright 2023-Present Couchbase, Inc.
//
//  Use of this software is governed by the Business Source License included
//  in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
//  in that file, in accordance with the Business Source License, use of this
//  software will be governed by the Apache License, Version 2.0, included in
//  the file licenses/APL2.txt.
//

#include "QueryParser.hh"
#include "QueryParser+Private.hh"
#include "QueryParserTables.hh"
#include "Error.hh"
#include "SQLUtil.hh"
#include "StringUtil.hh"
#include "Dict.hh"
#include "Logging.hh"
#include "MutableArray.hh"

#ifdef COUCHBASE_ENTERPRISE

using namespace std;
using namespace fleece;
using namespace fleece::impl;
using namespace litecore::qp;

namespace litecore {

    static constexpr unsigned kMaxMaxResults = 10000;

    string QueryParser::tableFromVectorDistanceCall(const ArrayIterator& params) {
        auto  expr     = params[0];
        auto  exprJSON = expressionCanonicalJSON(expr);
        slice metricName;
        if ( auto metricVal = params[2] )
            metricName = requiredString(metricVal, "3rd argument (metric) to APPROX_VECTOR_DISTANCE");
        require(expr->type() == kArray,
                "first argument to APPROX_VECTOR_DISTANCE must evaluate to a vector; did you pass the index name %s "
                "instead?",
                exprJSON.c_str());
        return _delegate.vectorTableName(_defaultTableName, exprJSON, metricName);
    }

    // Writes the SQL vector MATCH expression itself, based on the args of APPROX_VECTOR_DISTANCE()
    void QueryParser::writeVectorMatchExpression(const ArrayIterator& params, string_view alias,
                                                 string_view tableName) {
        if ( !alias.empty() ) _sql << alias << '.';
        auto targetVectorParam = params[1];
        _sql << "vector MATCH encode_vector(";
        _context.push_back(&kArgListOperation);  // suppress unnecessary parens
        parseNode(targetVectorParam);
        _context.pop_back();
        _sql << ")";
        if ( const Value* numProbesVal = params[3] ) {
            auto numProbes = numProbesVal->asInt();
            require(numProbes > 0, "4th argument (numProbes) to APPROX_VECTOR_DISTANCE must be a positive integer");
            _sql << " AND vectorsearch_probes(";
            if ( !alias.empty() ) _sql << alias << '.';
            _sql << "vector, " << numProbes << ")";
        }
    }

    // Returns true if the WHERE clause does _not_ require a hybrid query,
    // i.e. if it's nonexistent or consists only of a test that APPROX_VECTOR_DISTANCE() is less than something.
    static bool nonHybridWhereClause(const Value* where) {
        if ( !where ) return true;
        const Array* expr = requiredArray(where, "WHERE clause");
        if ( expr->count() != 3 ) return false;
        slice op = requiredString(expr->get(0), "WHERE clause op");
        if ( op == "<" || op == "<=" ) expr = expr->get(1)->asArray();
        else if ( op == ">" || op == ">=" )
            expr = expr->get(2)->asArray();
        else
            return false;
        return expr && expr->get(0) && expr->get(0)->asString().caseEquivalent(kVectorDistanceFnNameWithParens);
    }

    // Scans the entire query for APPROX_VECTOR_DISTANCE() calls, and adds join tables for ones that are indexed.
    void QueryParser::addVectorSearchJoins(const Dict* select) {
        findNodes(select, kVectorDistanceFnNameWithParens, 1, [&](const Array* distExpr) {
            ArrayIterator params(distExpr);
            ++params;  // skip fn name
            if ( auto accurate = params[4] ) {
                require(accurate->type() == kBoolean, "APPROX_VECTOR_DISTANCE 'accurate' arg must be boolean");
                require(accurate->asBool() == false, "APPROX_VECTOR_DISTANCE does not support 'accurate'=true");
            }

            // Use the vector expression to identify the index:
            string         tableName = tableFromVectorDistanceCall(params);
            indexJoinInfo* info      = indexJoinTable(tableName, "vector");

            if ( nonHybridWhereClause(getCaseInsensitive(select, "WHERE")) ) {
                // If there is no WHERE clause, this is a simple non-hybrid query.
                // This is implemented by a nested SELECT that finds the nearest vectors in
                // the entire collection. Isolating this in a nested SELECT ensures SQLite doesn't
                // see the outer JOIN against the collection; if it did, the vectorsearch extension's
                // planner would see a constraint against `rowid` and interpret it as a hybrid search.
                // https://github.com/couchbaselabs/mobile-vector-search/blob/main/docs/Extension.md

                // Figure out the limit to use in the vector query:
                int64_t maxResults;
                auto    limitVal = getCaseInsensitive(select, "LIMIT");
                require(limitVal, "a LIMIT must be given when using APPROX_VECTOR_DISTANCE()");
                maxResults = limitVal->asInt();
                require(limitVal->isInteger() && maxResults > 0,
                        "LIMIT must be a positive integer when using APPROX_VECTOR_DISTANCE()");
                require(maxResults <= kMaxMaxResults, "LIMIT must not exceed %u when using APPROX_VECTOR_DISTANCE()",
                        kMaxMaxResults);

                // Register a callback to write the nested SELECT in place of a table name:
                info->writeTableSQL = [=] {
                    _sql << "(SELECT rowid, distance FROM " << sqlIdentifier(tableName) << " WHERE ";
                    writeVectorMatchExpression(params, "", tableName);
                    _sql << " LIMIT " << maxResults << ")";
                };
            } else {
                // In a hybrid query, add the MATCH condition to the JOIN's ON clause:
                info->writeExtraOnSQL = [=] {
                    _sql << " AND ";
                    writeVectorMatchExpression(params, info->alias, tableName);
                };
            }
        });
    }

    // Writes the SQL translation of the `APPROX_VECTOR_DISTANCE(...)` call.
    void QueryParser::writeVectorDistanceFn(ArrayIterator& params) {
        // APPROX_VECTOR_DISTANCE can only be used in a WHERE if it's not within an OR.
        auto validate = [&]() -> bool {
            bool foundOR = false;
            for ( auto i = _context.rbegin() + 1; i != _context.rend(); ++i ) {
                if ( (*i)->op == "OR" ) foundOR = true;
                else if ( *i == &kWhereOperation )
                    return !foundOR;
            }
            return true;  // not in a WHERE clause
        };
        require(validate(), "APPROX_VECTOR_DISTANCE can't be used within an OR in a WHERE clause");

        string         tableName = tableFromVectorDistanceCall(params);
        indexJoinInfo* join      = indexJoinTable(tableName, "vector");
        _sql << join->alias << ".distance";
    }

    // Given the expression to index from a vector index spec, returns the SQL of a
    // `fl_vector_to_index()` call whose value is a binary vector to pass to vectorsearch.
    std::string QueryParser::vectorToIndexExpressionSQL(const fleece::impl::Value* exprToIndex, unsigned dimensions) {
        auto a = MutableArray::newArray();
        a->append(dimensions);
        const Value* dimAsFleece = a->get(0);
        return functionCallSQL(kVectorToIndexFnName, exprToIndex, dimAsFleece);
    }

}  // namespace litecore

#endif  // COUCHBASE_ENTERPRISE
