#ifndef _Qunar_SQL_Audit_TOOLS_H
#define _Qunar_SQL_Audit_TOOLS_H

#include "postgres.h"

typedef struct colInfo {
    Oid          atttypid;
    int32        atttypmod;
    const char  *atttypname;
} ColInfo;

ColInfo  getColName(ColumnDef *colDef);
void     finishAudit(void);

#endif
