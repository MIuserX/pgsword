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

// 主键检测

