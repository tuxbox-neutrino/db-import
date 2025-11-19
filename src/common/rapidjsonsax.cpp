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
