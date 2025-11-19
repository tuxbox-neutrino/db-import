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
