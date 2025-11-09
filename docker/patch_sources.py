from pathlib import Path
import textwrap
import os

def replace_or_fail(text, old_variants, new, path):
    variants = (old_variants,) if isinstance(old_variants, str) else tuple(old_variants)
    for variant in variants:
        if variant in text:
            return text.replace(variant, new, 1)
    raise SystemExit(f"pattern not found in {path}")

root = Path(__file__).resolve().parent.parent

mv_file = root / 'src' / 'mv2mariadb.cpp'
sql_file = root / 'src' / 'sql.cpp'
types_file = root / 'src' / 'types.h'
helpers_file = root / 'src' / 'common' / 'helpers.cpp'
makefile = root / 'Makefile'

helpers_text = helpers_file.read_text()
if '#include <limits>' not in helpers_text:
    helpers_text = replace_or_fail(helpers_text,
        '#include <fstream>\n#include <sstream>\n#include <iomanip>\n#include <ctime>\n',
        '#include <fstream>\n#include <sstream>\n#include <iomanip>\n#include <limits>\n#include <ctime>\n',
        helpers_file
    )
    helpers_file.write_text(helpers_text)

mv_text = mv_file.read_text()
if 'g_mysqlHost' not in mv_text:
    mv_text = replace_or_fail(mv_text,
        '#include <iomanip>\n\n#include "sql.h"\n',
        '#include <iomanip>\n#include <sstream>\n\n#include "sql.h"\n',
        mv_file
    )
    mv_text = replace_or_fail(mv_text,
        'string\t\t\t g_passwordFile;',
        'string\t\t\t g_passwordFile;\nstring\t\t\t g_mysqlHost;',
        mv_file
    )
    mv_text = replace_or_fail(mv_text,
        'g_settings.videoDb_TableVersion\t\t= configFile.getString("videoDb_TableVersion",\t"version");\n\tVIDEO_DB_TMP_1\t\t\t\t= g_settings.videoDbTmp1;\n',
        'g_settings.videoDb_TableVersion\t\t= configFile.getString("videoDb_TableVersion",\t"version");\n\tg_settings.mysqlHost\t\t= configFile.getString("mysqlHost", "127.0.0.1");\n\tVIDEO_DB_TMP_1\t\t\t\t= g_settings.videoDbTmp1;\n',
        mv_file
    )
    mv_text = replace_or_fail(mv_text,
        '\tconfigFile.setString("passwordFile",\t\t g_settings.passwordFile);\n\n\t/* server list */',
        '\tconfigFile.setString("passwordFile",\t\t g_settings.passwordFile);\n\tconfigFile.setString("mysqlHost",\t\t g_settings.mysqlHost);\n\n\t/* server list */',
        mv_file
    )
    mv_text = replace_or_fail(mv_text,
        'g_passwordFile = path0 + "/" + g_settings.passwordFile;\n\n\tcsql = new CSql();',
        'g_passwordFile = path0 + "/" + g_settings.passwordFile;\n\tg_mysqlHost = g_settings.mysqlHost;\n\n\tcsql = new CSql();',
        mv_file
    )
    mv_file.write_text(mv_text)

sql_text = sql_file.read_text()
if 'g_mysqlHost' not in sql_text:
    sql_text = replace_or_fail(sql_text,
        [
            'extern string\t\t\t g_passwordFile;\n\nextern void myExit(int val);\n',
            'extern string\t\t\t g_passwordFile;\nextern void myExit(int val);\n'
        ],
        'extern string\t\t\t g_passwordFile;\nextern string\t\t\t g_mysqlHost;\nextern void myExit(int val);\n',
        sql_file
    )
    sql_text = replace_or_fail(sql_text,
        [
            '\tif (!mysql_real_connect(mysqlCon, "127.0.0.1", sqlUser.c_str(), sqlPW.c_str(), NULL, 3306, NULL, flags))\n',
            'if (!mysql_real_connect(mysqlCon, "127.0.0.1", sqlUser.c_str(), sqlPW.c_str(), NULL, 3306, NULL, flags))\n'
        ],
        '\tconst char* host = g_mysqlHost.empty() ? "127.0.0.1" : g_mysqlHost.c_str();\n\tif (!mysql_real_connect(mysqlCon, host, sqlUser.c_str(), sqlPW.c_str(), NULL, 3306, NULL, flags))\n',
        sql_file
    )
    sql_file.write_text(sql_text)

types_text = types_file.read_text()
if 'mysqlHost' not in types_text:
    types_text = replace_or_fail(types_text,
        'string videoDb_TableInfo;\n\tstring videoDb_TableVersion;\n\n\t/* download server */\n',
        'string videoDb_TableInfo;\n\tstring videoDb_TableVersion;\n\tstring mysqlHost;\n\n\t/* download server */\n',
        types_file
    )
    types_file.write_text(types_text)

make_text = makefile.read_text()
if '\t$(quiet)$(LD) $(PROG_OBJS) $(LDFLAGS) -o $@' not in make_text:
    make_text = replace_or_fail(make_text,
        ['\t$(quiet)$(LD) $(LDFLAGS) $(PROG_OBJS) -o $@', '$(quiet)$(LD) $(LDFLAGS) $(PROG_OBJS) -o $@'],
        '\t$(quiet)$(LD) $(PROG_OBJS) $(LDFLAGS) -o $@',
        makefile
    )
    makefile.write_text(make_text)

rapidjson_h = root / 'src' / 'common' / 'rapidjsonsax.h'
rapidjson_h_content = textwrap.dedent("""\
#ifndef __RAPIDJSONSAX_H__
#define __RAPIDJSONSAX_H__

#include <string>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <rapidjson/reader.h>
#include <rapidjson/filereadstream.h>

using namespace std;
using namespace rapidjson;

class CRapidJsonSAX
{
private:
    size_t fileStreamBufSize;
    typedef void callbackParserFunc_t(int, string, int, CRapidJsonSAX*);

    struct parseHandler {
        int type;
        string data;
        callbackParserFunc_t* cb;
        CRapidJsonSAX* owner;

        parseHandler() : type(), data(), cb(nullptr), owner(nullptr) {}

        void init(callbackParserFunc_t* callback, CRapidJsonSAX* self) {
            cb = callback;
            owner = self;
        }

        void emit(int t, const string& value) {
            type = t;
            data = value;
            if (cb)
                cb(type, data, CRapidJsonSAX::parser_Work, owner);
        }

        bool Null() { emit(CRapidJsonSAX::type_Null, ""); return true; }
        bool Bool(bool b) { emit(CRapidJsonSAX::type_Bool, b ? "true" : "false"); return true; }
        bool Int(int i) { emit(CRapidJsonSAX::type_Int, to_string(i)); return true; }
        bool Uint(unsigned u) { emit(CRapidJsonSAX::type_Uint, to_string(u)); return true; }
        bool Int64(int64_t i) { emit(CRapidJsonSAX::type_Int64, to_string(i)); return true; }
        bool Uint64(uint64_t u) { emit(CRapidJsonSAX::type_Uint64, to_string(u)); return true; }
        bool Double(double d) { emit(CRapidJsonSAX::type_Double, to_string(d)); return true; }
        bool RawNumber(const char* str, SizeType length, bool) { emit(CRapidJsonSAX::type_Number, string(str, length)); return true; }
        bool String(const char* str, SizeType length, bool) { emit(CRapidJsonSAX::type_String, string(str, length)); return true; }
        bool StartObject() { emit(CRapidJsonSAX::type_StartObject, ""); return true; }
        bool Key(const char* str, SizeType length, bool) { emit(CRapidJsonSAX::type_Key, string(str, length)); return true; }
        bool EndObject(SizeType memberCount) { emit(CRapidJsonSAX::type_EndObject, to_string(memberCount)); return true; }
        bool StartArray() { emit(CRapidJsonSAX::type_StartArray, ""); return true; }
        bool EndArray(SizeType elementCount) { emit(CRapidJsonSAX::type_EndArray, to_string(elementCount)); return true; }
    };

    void Init();
    void parseStreamInternal(void* stream, bool isFileStream, callbackParserFunc_t* callback);

public:
    enum { parser_Start, parser_Work, parser_Stop };
    enum {
        type_Null,
        type_Bool,
        type_Int,
        type_Uint,
        type_Int64,
        type_Uint64,
        type_Double,
        type_Number,
        type_String,
        type_Key,
        type_StartObject,
        type_EndObject,
        type_StartArray,
        type_EndArray,
        type_None
    };

    CRapidJsonSAX();
    ~CRapidJsonSAX();

    string getTypeStr(int type);
    void parseFile(string file, callbackParserFunc_t* callback);
    void parseString(string json, callbackParserFunc_t* callback);
    void setFileStreamBufSize(size_t size) { fileStreamBufSize = size; }
};

#endif // __RAPIDJSONSAX_H__
""").strip() + "\n"
rapidjson_h.write_text(rapidjson_h_content)

rapidjson_cpp = root / 'src' / 'common' / 'rapidjsonsax.cpp'
rapidjson_cpp_content = textwrap.dedent("""\
#include <cassert>
#include <exception>

#include "rapidjsonsax.h"

CRapidJsonSAX::CRapidJsonSAX()
{
    Init();
}

void CRapidJsonSAX::Init()
{
    fileStreamBufSize = 65536; /* 64KB */
}

CRapidJsonSAX::~CRapidJsonSAX()
{
}

string CRapidJsonSAX::getTypeStr(int type)
{
    string ret;
    switch (type) {
        case CRapidJsonSAX::type_Null: ret = "Null"; break;
        case CRapidJsonSAX::type_Bool: ret = "Bool"; break;
        case CRapidJsonSAX::type_Int: ret = "Int"; break;
        case CRapidJsonSAX::type_Uint: ret = "Uint"; break;
        case CRapidJsonSAX::type_Int64: ret = "Int64"; break;
        case CRapidJsonSAX::type_Uint64: ret = "Uint64"; break;
        case CRapidJsonSAX::type_Double: ret = "Double"; break;
        case CRapidJsonSAX::type_Number: ret = "Number"; break;
        case CRapidJsonSAX::type_String: ret = "String"; break;
        case CRapidJsonSAX::type_Key: ret = "Key"; break;
        case CRapidJsonSAX::type_StartObject: ret = "StartObject"; break;
        case CRapidJsonSAX::type_EndObject: ret = "EndObject"; break;
        case CRapidJsonSAX::type_StartArray: ret = "StartArray"; break;
        case CRapidJsonSAX::type_EndArray: ret = "EndArray"; break;
        default: ret = "Unknown"; break;
    }
    return ret;
}

void CRapidJsonSAX::parseStreamInternal(void* stream, bool isFileStream, callbackParserFunc_t* callback)
{
    FileReadStream* fs = static_cast<FileReadStream*>(stream);
    StringStream* ss = static_cast<StringStream*>(stream);
    parseHandler handler;
    handler.init(callback, this);

    Reader reader;
    callback(type_None, "", parser_Start, this);

    if (isFileStream)
        reader.Parse<kParseDefaultFlags>(*fs, handler);
    else
        reader.Parse<kParseDefaultFlags>(*ss, handler);

    callback(type_None, "", parser_Stop, this);
}

void CRapidJsonSAX::parseFile(string file, callbackParserFunc_t* callback)
{
    try {
        FILE* f = fopen(file.c_str(), "rb");
        assert((f != NULL));
        char* buf = new char[fileStreamBufSize];
        FileReadStream ss(f, buf, fileStreamBufSize);
        parseStreamInternal(static_cast<void*>(&ss), true, callback);
        fclose(f);
        delete [] buf;
        return;
    }
    catch (exception const& e) {
        cerr << e.what() << endl;
    }
}

void CRapidJsonSAX::parseString(string json, callbackParserFunc_t* callback)
{
    StringStream ss(json.c_str());
    parseStreamInternal(static_cast<void*>(&ss), false, callback);
}
""").strip() + "\n"
rapidjson_cpp.write_text(rapidjson_cpp_content)
