#include "postgres.h"
#include "nodes/parsenodes.h"
#include "common/keywords.h"
#include "access/htup_details.h"
#include "catalog/pg_type.h"
#include "parser/parse_type.h"
#include "parser/parse_utilcmd.h"
#include "nodes/pg_list.h"
#include "tcop/utility.h"
#include "utils/guc.h"
#include "utils/elog.h"
#include "utils/syscache.h"

#include "pgsword.h"

static bool isValidChar(const char ch);

// 将 timestamp 替换为 timestamptz
void replaceTimestampToTimestamptz(ColumnDef *colDef) {
    Oid   typOid;
    int32 atttypmod;

    typenameTypeIdAndMod(NULL, colDef->typeName, &typOid, &atttypmod);
    if ( typOid == 1114 ) {
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                    errmsg("QunarSQLAudit: replace \"timestatmp\" to \"timestamptz\", please")));
    }
}

// DB Object
bool isValidName(const char *objName) {
    int i = 1;

    if ( objName == NULL || strlen(objName) <= 0 )
        return false;

    if ( objName[0] >= '0' && objName[0] <= '9' )
        return false;

    while ( objName[i] ) {
        if ( !isValidChar(objName[i]) ) {
            return false;
        }
        ++i;
    }

    return true;
}

static bool isValidChar(const char ch) {
    if (  (ch >= 'a' && ch <= 'z')
        ||
          ch == '_'
        ||
          (ch >= '0' && ch <= '9')
    ) {

        return true;
    }

    return false;
}
