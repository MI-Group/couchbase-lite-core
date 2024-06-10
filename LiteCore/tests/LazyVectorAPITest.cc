//
// Created by Callum Birks on 28/05/2024.
//

#include "c4Base.hh"
#include "DatabaseImpl.hh"
#include "LazyIndex.hh"
#include "c4Collection.h"
#include "c4Collection.hh"
#include "c4Index.h"
#include "c4Index.hh"
#include "c4Query.h"
#include "c4Test.hh"  // IWYU pragma: keep
#include "LiteCoreTest.hh"
#include "SQLiteDataFile.hh"

#ifdef COUCHBASE_ENTERPRISE


using namespace litecore;

static constexpr float kWordsTargetVector[300] = {
        0.03193166106939316,     0.032055653631687164,  0.07188114523887634,   -0.09893740713596344,
        -0.07693558186292648,    0.07570040225982666,   0.42786234617233276,   -0.11442682892084122,
        -0.7863243818283081,     -0.47983086109161377,  -0.10168658196926117,  0.10985997319221497,
        -0.15261511504650116,    -0.08458329737186432,  -0.16363860666751862,  -0.20225222408771515,
        -0.2593214809894562,     -0.032738097012043,    -0.16649988293647766,  -0.059701453894376755,
        0.17472036182880402,     -0.007310086861252785, -0.13918264210224152,  -0.07260780036449432,
        -0.02461239881813526,    -0.04195880889892578,  -0.15714778006076813,  0.48038315773010254,
        0.7536261677742004,      0.41809454560279846,   -0.17144775390625,     0.18296195566654205,
        -0.10611499845981598,    0.11669538915157318,   0.07423929125070572,   -0.3105475902557373,
        -0.045081984251737595,   -0.18190748989582062,  0.22430984675884247,   0.05735112354159355,
        -0.017394868656992912,   -0.148889422416687,    -0.20618586242198944,  -0.1446581482887268,
        0.061972495168447495,    0.07787969708442688,   0.14225411415100098,   0.20560632646083832,
        0.1786964386701584,      -0.380594402551651,    -0.18301603198051453,  -0.19542981684207916,
        0.3879885971546173,      -0.2219538390636444,   0.11549852043390274,   -0.0021717497147619724,
        -0.10556972026824951,    0.030264658853411674,  0.16252967715263367,   0.06010117009282112,
        -0.045007310807704926,   0.02435707487165928,   0.12623260915279388,   -0.12688252329826355,
        -0.3306281864643097,     0.06452160328626633,   0.0707000121474266,    -0.04959108680486679,
        -0.2567063570022583,     -0.01878536120057106,  -0.10857286304235458,  -0.01754194125533104,
        -0.0713721290230751,     0.05946013703942299,   -0.1821729987859726,   -0.07293688505887985,
        -0.2778160572052002,     0.17880073189735413,   -0.04669278487563133,  0.05351974070072174,
        -0.23292849957942963,    0.05746332183480263,   0.15462779998779297,   -0.04772235080599785,
        -0.003306782804429531,   0.058290787041187286,  0.05908169597387314,   0.00504430802538991,
        -0.1262340396642685,     0.11612161248922348,   0.25303348898887634,   0.18580256402492523,
        0.09704313427209854,     -0.06087183952331543,  0.19697663187980652,   -0.27528849244117737,
        -0.0837797075510025,     -0.09988483041524887,  -0.20565757155418396,  0.020984146744012833,
        0.031014855951070786,    0.03521743416786194,   -0.05171370506286621,  0.009112107567489147,
        -0.19296088814735413,    -0.19363830983638763,  0.1591167151927948,    -0.02629968523979187,
        -0.1695055067539215,     -0.35807400941848755,  -0.1935291737318039,   -0.17090126872062683,
        -0.35123637318611145,    -0.20035606622695923,  -0.03487539291381836,  0.2650701701641083,
        -0.1588021069765091,     0.32268261909484863,   -0.024521857500076294, -0.11985184997320175,
        0.14826008677482605,     0.194917231798172,     0.07971998304128647,   0.07594677060842514,
        0.007186363451182842,    -0.14641280472278595,  0.053229596465826035,  0.0619836151599884,
        0.003207010915502906,    -0.12729716300964355,  0.13496214151382446,   0.107656329870224,
        -0.16516226530075073,    -0.033881571143865585, -0.11175122112035751,  -0.005806141998618841,
        -0.4765360355377197,     0.11495379358530045,   0.1472187340259552,    0.3781401813030243,
        0.10045770555734634,     -0.1352398842573166,   -0.17544329166412354,  -0.13191302120685577,
        -0.10440415143966675,    0.34598618745803833,   0.09728766977787018,   -0.25583627820014954,
        0.035236816853284836,    0.16205145418643951,   -0.06128586828708649,  0.13735555112361908,
        0.11582338809967041,     -0.10182418674230576,  0.1370954066514969,    0.15048766136169434,
        0.06671152263879776,     -0.1884871870279312,   -0.11004580557346344,  0.24694739282131195,
        -0.008159132674336433,   -0.11668405681848526,  -0.01214478351175785,  0.10379738360643387,
        -0.1626262664794922,     0.09377897530794144,   0.11594484746456146,   -0.19621512293815613,
        0.26271334290504456,     0.04888357222080231,   -0.10103251039981842,  0.33250945806503296,
        0.13565145432949066,     -0.23888370394706726,  -0.13335271179676056,  -0.0076894499361515045,
        0.18256276845932007,     0.3276212215423584,    -0.06567271053791046,  -0.1853761374950409,
        0.08945729583501816,     0.13876311480998993,   0.09976287186145782,   0.07869105041027069,
        -0.1346970647573471,     0.29857659339904785,   0.1329529583454132,    0.11350086331367493,
        0.09112624824047089,     -0.12515446543693542,  -0.07917925715446472,  0.2881546914577484,
        -1.4532661225530319e-05, -0.07712751626968384,  0.21063975989818573,   0.10858846455812454,
        -0.009552721865475178,   0.1629313975572586,    -0.39703384041786194,  0.1904662847518921,
        0.18924959003925323,     -0.09611514210700989,  0.001136621693149209,  -0.1293390840291977,
        -0.019481558352708817,   0.09661063551902771,   -0.17659670114517212,  0.11671938002109528,
        0.15038564801216125,     -0.020016824826598167, -0.20642194151878357,  0.09050136059522629,
        -0.1768183410167694,     -0.2891409397125244,   0.04596589505672455,   -0.004407480824738741,
        0.15323616564273834,     0.16503025591373444,   0.17370983958244324,   0.02883041836321354,
        0.1463884711265564,      0.14786243438720703,   -0.026439940556883812, -0.03113352134823799,
        0.10978181660175323,     0.008928884752094746,  0.24813824892044067,   -0.06918247044086456,
        0.06958142668008804,     0.17475970089435577,   0.04911438003182411,   0.17614248394966125,
        0.19236832857131958,     -0.1425514668226242,   -0.056531358510255814, -0.03680772706866264,
        -0.028677923604846,      -0.11353116482496262,  0.012293893843889236,  -0.05192646384239197,
        0.20331953465938568,     0.09290937334299088,   0.15373043715953827,   0.21684466302394867,
        0.40546831488609314,     -0.23753701150417328,  0.27929359674453735,   -0.07277711480855942,
        0.046813879162073135,    0.06883064657449722,   -0.1033223420381546,   0.15769273042678833,
        0.21685580909252167,     -0.00971329677850008,  0.17375953495502472,   0.027193285524845123,
        -0.09943609684705734,    0.05770351365208626,   0.0868956446647644,    -0.02671697922050953,
        -0.02979189157485962,    0.024517420679330826,  -0.03931192681193352,  -0.35641804337501526,
        -0.10590721666812897,    -0.2118944674730301,   -0.22070199251174927,  0.0941486731171608,
        0.19881175458431244,     0.1815279871225357,    -0.1256905049085617,   -0.0683583989739418,
        0.19080783426761627,     -0.009482398629188538, -0.04374842345714569,  0.08184348791837692,
        0.20070189237594604,     0.039221834391355515,  -0.12251003831624985,  -0.04325549304485321,
        0.03840530663728714,     -0.19840988516807556,  -0.13591833412647247,  0.03073180839419365,
        0.1059495136141777,      -0.10656466335058212,  0.048937033861875534,  -0.1362423598766327,
        -0.04138947278261185,    0.10234509408473969,   0.09793911874294281,   0.1391254961490631,
        -0.0906999260187149,     0.146945983171463,     0.14941848814487457,   0.23930180072784424,
        0.36049938201904297,     0.0239607822149992,    0.08884347230195999,   0.061145078390836716};

class LazyVectorAPITest : public C4Test {
  public:
    static int initialize() {
        // Note: The extension path has to be set before the DataFile is opened,
        // so it will load the extension. That's why this is called in a param of the parent ctor.
        std::once_flag sOnce;
        std::call_once(sOnce, [] {
            if ( const char* path = getenv("LiteCoreExtensionPath") ) {
                sExtensionPath = path;
                litecore::SQLiteDataFile::setExtensionPath(sExtensionPath);
                Log("Registered LiteCore extension path %s", path);
            }
        });
        return 0;
    }

    LazyVectorAPITest() : LazyVectorAPITest(0) {}

    LazyVectorAPITest(int which) : C4Test(which + initialize()) {
        {  // Open words_db
            C4DatabaseConfig2 config = {slice(TempDir())};
            config.flags |= kC4DB_Create;
            auto name = copyFixtureDB(TestFixture::sFixturesDir, "vectors/words_db.cblite2");
            closeDB();
            db         = REQUIRED(c4db_openNamed(name, &config, ERROR_INFO()));
            _wordsColl = Retained(REQUIRED(db->getCollection({"words", "_default"})));
        }
        {  // Create encoded target vector parameter
            Encoder enc;
            enc.beginDict();
            enc.writeKey("target");
            enc.writeData(slice(kWordsTargetVector, 300 * sizeof(float)));
            enc.endDict();
            _encodedTarget = enc.finish();
        }
    }

    using UpdaterFn = function_ref<bool(LazyIndexUpdate*, size_t, fleece::Value)>;

    static bool alwaysUpdate(LazyIndexUpdate*, size_t, fleece::Value) { return true; }

    /// Get the LazyIndex with the given name. Will return null if the index does not exist.
    [[nodiscard]] Retained<LazyIndex> getLazyIndex(std::string_view name) const noexcept {
        auto& store = asInternal(db)->dataFile()->defaultKeyStore();
        try {
            return make_retained<LazyIndex>(store, name);
        } catch ( [[maybe_unused]] std::exception& e ) { return nullptr; }
    }

    void checkQueryReturnsWords(C4Query* query, const std::vector<string>& expectedWords) const {
        auto e = REQUIRED(c4query_run(query, _encodedTarget, ERROR_INFO()));
        REQUIRE(c4queryenum_getRowCount(e, ERROR_INFO()) == expectedWords.size());
        for ( const auto& expectedWord : expectedWords ) {
            REQUIRE(c4queryenum_next(e, ERROR_INFO()));
            FLArrayIterator columns  = e->columns;
            slice           word     = Value(FLArrayIterator_GetValueAt(&columns, 0)).asString();
            float           distance = Value(FLArrayIterator_GetValueAt(&columns, 1)).asFloat();
            CHECK(word == slice(expectedWord));
        }
        CHECK(!c4queryenum_next(e, ERROR_INFO()));
        c4queryenum_release(e);
    }

    void checkQueryReturnsVectors(C4Query* query, int64_t expectedRowCount,
                                  const std::vector<float>& expectedVectors) const {
        auto e = REQUIRED(c4query_run(query, _encodedTarget, ERROR_INFO()));
        REQUIRE(c4queryenum_getRowCount(e, ERROR_INFO()) == expectedRowCount);
        for ( size_t i = 0; i < expectedRowCount; ++i ) {
            REQUIRE(c4queryenum_next(e, ERROR_INFO()));
            FLArrayIterator columns     = e->columns;
            auto            vectorArray = Value(FLArrayIterator_GetValueAt(&columns, 0)).asArray();
            for ( size_t j = 0; j < expectedVectors.size(); j++ ) {
                float vector = vectorArray.get(j).asFloat();
                CHECK(vector == expectedVectors[j]);
            }
        }
        CHECK(!c4queryenum_next(e, ERROR_INFO()));
        c4queryenum_release(e);
    }

    bool createIndex(slice name, slice jsonSpec, C4IndexType type, C4IndexOptions options, C4Error* err) const {
        return c4coll_createIndex(_wordsColl, name, jsonSpec, kC4JSONQuery, type, &options, err);
    }

    bool createVectorIndex(bool lazy, slice expression = R"(['.word'])"_sl, slice name = "words_index"_sl,
                           C4Error* err = ERROR_INFO()) const {
        auto options = vectorOptions(300, 8);
        options.lazy = lazy;
        return createIndex(name, json5(expression), kC4VectorIndex, indexOptions(options), err);
    }

    C4Index* getIndex(slice name = "words_index"_sl, C4Error* err = ERROR_INFO()) const {
        return c4coll_getIndex(_wordsColl, name, err);
    }

    void createVectorDocs(unsigned numberOfDocs) const {
        TransactionHelper t(db);
        constexpr size_t  bufSize = 20;
        char              docID[bufSize];
        for ( unsigned i = 1; i <= numberOfDocs; i++ ) {
            Encoder enc(db->getFleeceSharedKeys());
            enc.beginDict();
            enc.writeKey("num");
            enc.writeInt(i);
            enc.writeKey("type");
            enc.writeString("number");
            enc.endDict();
            snprintf(docID, bufSize, "doc-%03u", i);
            createRev(c4str(docID), kRevID, enc.finish());
        }
    }

    template <typename T>
    void createVectorDoc(unsigned i, T value) {
        TransactionHelper t(db);
        constexpr size_t  bufSize = 20;
        char              docID[bufSize];
        Encoder           enc(db->getFleeceSharedKeys());
        enc.beginDict();
        enc.write("value", value);
        enc.endDict();
        snprintf(docID, bufSize, "doc-%03u", i);
        createRev(_wordsColl, slice(docID), kRevID, enc.finish());
    }

    /// Create a blob with `blobContents`, create a numbered doc and assign the `value` field in the doc as
    /// the blob dictionary.
    void createVectorDocWithBlob(unsigned i, slice blobContents) {
        C4BlobKey blobKey{};
        REQUIRE(c4blob_create(&db->getBlobStore(), blobContents, nullptr, &blobKey, ERROR_INFO()));
        std::stringstream json{};
        json << "{'" << kC4ObjectTypeProperty << "': '" << kC4ObjectType_Blob << "', ";
        json << "digest: '" << blobKey.digestString() << "', length: " << blobContents.size
             << ", content_type: 'text/plain'}";
        auto jsonStr = json5(json.str());
        auto doc     = Doc::fromJSON(slice(jsonStr));
        createVectorDoc(i, doc.root());
    }

    [[nodiscard]] std::vector<float> vectorsForWord(slice word) const {
        const auto  query = REQUIRED(c4query_new2(db, kC4JSONQuery, alloc_slice(json5(R"({
                WHERE: ['=', ['$word'], ['.word']],
                WHAT:  [ ['.vector'] ],
                FROM:  [{'COLLECTION':'words'}],
            })")),
                                                  nullptr, ERROR_INFO()));
        alloc_slice encodedWord{};
        {  // Create encoded target vector parameter
            Encoder enc;
            enc.beginDict();
            enc.writeKey("word");
            enc.writeString(word);
            enc.endDict();
            encodedWord = enc.finish();
        }
        const auto e = REQUIRED(c4query_run(query, encodedWord, ERROR_INFO()));
        REQUIRE(c4queryenum_next(e, ERROR_INFO()));
        const auto vectors    = Value(FLArrayIterator_GetValueAt(&e->columns, 0));
        auto       outVectors = std::vector<float>(300);
        for ( Value v : vectors.asArray() ) { outVectors.push_back(v.asFloat()); }
        c4queryenum_release(e);
        c4query_release(query);
        return outVectors;
    }

    static Value updaterValue(C4IndexUpdater* updater, unsigned i) {
        auto flValue = c4indexupdater_valueAt(updater, i);
        return {flValue};
    }

    static C4VectorIndexOptions c4VectorOptions(const IndexSpec::VectorOptions& options) {
        C4VectorMetricType metric{};
        switch ( options.metric ) {
            case IndexSpec::VectorOptions::DefaultMetric:
                metric = kC4VectorMetricDefault;
                break;
            case IndexSpec::VectorOptions::Euclidean:
                metric = kC4VectorMetricEuclidean;
                break;
            case IndexSpec::VectorOptions::Cosine:
                metric = kC4VectorMetricCosine;
                break;
        }

        C4VectorClusteringType clusteringType{};
        switch ( options.clustering.type ) {
            case IndexSpec::VectorOptions::Flat:
                clusteringType = kC4VectorClusteringFlat;
                break;
            case IndexSpec::VectorOptions::Multi:
                clusteringType = kC4VectorClusteringMulti;
                break;
        }

        C4VectorEncodingType encodingType{};
        switch ( options.encoding.type ) {
            case IndexSpec::VectorOptions::DefaultEncoding:
                encodingType = kC4VectorEncodingDefault;
                break;
            case IndexSpec::VectorOptions::NoEncoding:
                encodingType = kC4VectorEncodingNone;
                break;
            case IndexSpec::VectorOptions::PQ:
                encodingType = kC4VectorEncodingPQ;
                break;
            case IndexSpec::VectorOptions::SQ:
                encodingType = kC4VectorEncodingSQ;
                break;
        }

        return C4VectorIndexOptions{
                options.dimensions,
                metric,
                C4VectorClustering{clusteringType, options.clustering.flat_centroids,
                                   options.clustering.multi_subquantizers, options.clustering.multi_bits},
                C4VectorEncoding{encodingType, options.encoding.pq_subquantizers, options.encoding.bits},
                options.minTrainingSize,
                options.maxTrainingSize,
                options.numProbes,
                options.lazy,
        };
    }

    static IndexSpec::VectorOptions vectorOptions(unsigned dimensions, unsigned centroids) {
        IndexSpec::VectorOptions options(dimensions);
        options.clustering.type           = IndexSpec::VectorOptions::Flat;
        options.clustering.flat_centroids = centroids;
        return options;
    }

    static C4IndexOptions indexOptions(const IndexSpec::VectorOptions& vectorOptions) {
        const auto c4vectorOptions = c4VectorOptions(vectorOptions);
        return C4IndexOptions{"en", false, false, nullptr, c4vectorOptions};
    }

    static inline string   sExtensionPath;
    alloc_slice            _encodedTarget;
    Retained<C4Collection> _wordsColl;
};

TEST_CASE_METHOD(LazyVectorAPITest, "IndexUpdater Finish Incomplete Update", "[API][.VectorSearch]") {
    REQUIRE(createVectorIndex(true));
    auto    index   = REQUIRED(getIndex());
    auto    updater = REQUIRED(c4index_beginUpdate(index, 2, ERROR_INFO()));
    C4Error err{};
    c4indexupdater_finish(updater, &err);
    CHECK(err.code == kC4ErrorUnsupported);

    c4indexupdater_release(updater);
    c4index_release(index);
}

#endif
