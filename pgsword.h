#ifndef _Qunar_SQL_Audit_H
#define _Qunar_SQL_Audit_H

typedef struct ConstrList {
    bool   is_not_null;
    bool   is_primary_key;
    bool   is_unique;
    bool   has_default;
    char  *default_str;
} ConstrList;

bool isValidName(const char *objName);
void replaceTimestampToTimestamptz(ColumnDef *coldef);

#endif
