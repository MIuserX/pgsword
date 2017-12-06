#ifndef _Qunar_PGSQL_Audit_H
#define _Qunar_PGSQL_Audit_H

#include "postgres.h"
#include "catalog/pg_type.h"

typedef enum {
    Q_OK = 0,
    Q_IS_KEYWORD,
    Q_INVALID_CHAR
} QErrCode;

bool isValidName(const char *objName);
void replaceTimestampToTimestamptz(ColumnDef *colDef);
void replaceJsonToJsonb(ColumnDef *colDef);
void checkRule(CreateStmt *stmt);
const char *getCreateName(Node *parsetree);
void checkDBObjName(const char *name, NodeTag nodeTag);

#endif // _Qunar_PGSQL_Audit_H
