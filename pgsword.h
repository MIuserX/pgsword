#ifndef _Qunar_SQL_Audit_H
#define _Qunar_SQL_Audit_H

#include "postgres.h"
#include "catalog/pg_type.h"

typedef struct ConstrList {
    bool   is_not_null;
    bool   is_primary_key;
    bool   is_unique;
    bool   has_default;
    char  *default_str;
} ConstrList;

bool isValidName(const char *objName);
void replaceTimestampToTimestamptz(ColumnDef *colDef);
void replaceJsonToJsonb(ColumnDef *colDef);

#endif
