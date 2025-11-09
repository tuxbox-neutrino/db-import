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
