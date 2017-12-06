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
#include "pg_config.h"

#include "rule.h"
#include "tools.h"

static bool isValidChar(const char ch);

// 将 timestamp 替换为 timestamptz
void replaceTimestampToTimestamptz(ColumnDef *colDef) {
    Oid    typOid;
    int32  atttypmod;

    typenameTypeIdAndMod(NULL, colDef->typeName, &typOid, &atttypmod);
    if ( typOid == 1114 ) {
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                    errmsg("QunarSQLAudit: replace \"timestatmp\" to \"timestamptz\", please")));
    }
}

// 将 json 替换为 jsonb
void replaceJsonToJsonb(ColumnDef *colDef) {
    Oid    typOid;
    int32  atttypmod;

    typenameTypeIdAndMod(NULL, colDef->typeName, &typOid, &atttypmod);
    // json 和 jsonb 的 oid 定义都在
    // src/include/catalog/pg_type.h
#if PG_VERSION_NUM == 100001
    if ( typOid == 114 ) {
#endif
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                    errmsg("QunarSQLAudit: replace \"json\" to \"jsonb\", please")));
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

/*void checker(PlannedStmt *pstmt) {
    Node *parsetree = pstmt->utilityStmt;
    char  mymsg[MYMSG_SIZE] = { 0 };

    switch ( nodeTag(parsetree) ) {
        case T_CreatedbStmt:
            snprintf(mymsg, MYMSG_SIZE, "%s\"%s\"",
                    "\b\b\b\b\b\b\b\b\bQunar PGSQL Auditor: dbname=",
                    ((CreatedbStmt *)parsetree)->dbname);
            ereport(NOTICE,
                        (errcode(ERRCODE_SUCCESSFUL_COMPLETION),
                            errmsg("%s", mymsg)));
            break;

        case T_CreateSchemaStmt:
            snprintf(mymsg, MYMSG_SIZE, "%s\"%s\"",
                    "\b\b\b\b\b\b\b\b\bQunar PGSQL Auditor: schemaname=",
                    ((CreateSchemaStmt *)parsetree)->schemaname);
            ereport(NOTICE,
                        (errcode(ERRCODE_SUCCESSFUL_COMPLETION),
                            errmsg("%s", mymsg)));
            break;

        case T_CreateStmt:
            checkRule((CreateStmt *)parsetree);
            break;

        default:
            break;
    }
}*/


void checkRule(CreateStmt *stmt) {
    ListCell        *listptr;
    char            *typname = NULL;
    Form_pg_type    typeForm;
    HeapTuple       tuple;
    Oid             atttypid;
    int32           atttypmod;
    ConstrList      constrList;

    if ( !stmt && !(stmt->relation)) {
        return ;
    }

    // 检查 table name 是否是 pg keywords
    if ( isKeyword(stmt->relation->relname) ) {
        ereport(ERROR,
                    (errcode(ERRCODE_INTERNAL_ERROR),
                        errmsg("QunarSQLAudit: PostgreSQL keyword \"%s\" cannot be table name",
                                stmt->relation->relname)));
    }

    // rule:
    if ( !isValidName(stmt->relation->relname) ) {
        ereport(ERROR,
                    (errcode(ERRCODE_INTERNAL_ERROR),
                        errmsg("QunarSQLAudit: 表名只能由 小写字母(a-z)，数字(0-9)，下划线(_) 构成")));
    }

    // 检查 column name 是否 pg keywords
    foreach(listptr, stmt->tableElts)
    {
        ColumnDef *colDef = lfirst(listptr);

        initConstrList( &constrList );

        // rule: PG keywords 不能作为 column name
        if ( isKeyword(colDef->colname) ) {
            ereport(ERROR,
                    (errcode(ERRCODE_INTERNAL_ERROR),
                        errmsg("QunarSQLAudit: PostgreSQL keyword \"%s\" cannot be column name",
                               colDef->colname)));
        }

        if ( !isValidName(colDef->colname) ) {
            ereport(ERROR,
                    (errcode(ERRCODE_INTERNAL_ERROR),
                        errmsg("QunarSQLAudit: 列名只能由 小写字母(a-z)，数字(0-9)，下划线(_) 构成")));
        }

        typenameTypeIdAndMod(NULL, colDef->typeName, &atttypid, &atttypmod);

        // rule: 建议用 timestamptz 替代 timestamp
        replaceTimestampToTimestamptz(colDef);

        // rule: 建议用 jsonb 替代 json
        replaceJsonToJsonb(colDef);

        // 获取 type name
        tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(atttypid));
        if ( HeapTupleIsValid(tuple) ) {
            typeForm = (Form_pg_type) GETSTRUCT(tuple);
            typname = typeForm->typname.data;
        }
        ReleaseSysCache(tuple);

        // rule: 必须有一个名为 "id" 的 primary key
        getConstrList( &constrList, colDef->constraints );
        if ( !strcmp(colDef->colname, "id") && !constrList.is_primary_key ) {
            // 名字为 "id" ，但不为主键
            ereport(ERROR,
                        (errcode(ERRCODE_INTERNAL_ERROR),
                            errmsg("QunarSQLAudit: \"id\" must be PRIMARY KEY with type smallserial, serial or bigserial")));
        }
        else if ( strcmp(colDef->colname, "id") && constrList.is_primary_key ) {
            // 名字不为 "id" ，但是为主键
            ereport(ERROR,
                        (errcode(ERRCODE_INTERNAL_ERROR),
                            errmsg("QunarSQLAudit: the name must be \"id\" which column has PRIMARY KEY constraint")));
        }
        else if ( !strcmp(colDef->colname, "id")
                &&
                  constrList.is_primary_key
                &&
                  (
                   strcmp("int2", typname) &&
                   strcmp("int4", typname) &&
                   strcmp("int8", typname)
                  )
                ) {

            ereport(ERROR,
                        (errcode(ERRCODE_INTERNAL_ERROR),
                            errmsg("QunarSQLAudit: the type must be smallserial, serial or bigserial which column has PRIMARY KEY constraint")));
        }
    }
}

void checkDBObjName(const char *name, NodeTag nodeTag) {
    QErrCode qerrcode = Q_OK;
    char     mymsg[MYMSG_SIZE] = { 0 };

    // 检查 table name 是否是 pg keywords
    if ( isKeyword(name) ) {
        qerrcode = Q_IS_KEYWORD;
    }
    // rule:
    else if ( !isValidName(name) ) {
        qerrcode = Q_INVALID_CHAR;
    }

    if ( qerrcode != Q_OK ) {
        switch ( qerrcode ) {
            case Q_IS_KEYWORD:
                snprintf(mymsg,
                         MYMSG_SIZE,
                         "%s \"%s\" %s ",
                         "\b\b\b\b\b\b\b\bQunar PGSQL Auditor: PostgreSQL keyword",
                         name,
                         "cannot be");

                switch ( nodeTag ) {
                        /* create tablespace */
                        case T_CreateTableSpaceStmt:
                            strncat(mymsg, "tablespace name", MYMSG_SIZE-strlen(mymsg)-1);
                            break;

                        /* create database */
                        case T_CreatedbStmt:
                            strncat(mymsg, "database name", MYMSG_SIZE-strlen(mymsg)-1);
                            break;

                        /* create schema */
                        case T_CreateSchemaStmt:
                            strncat(mymsg, "schema name", MYMSG_SIZE-strlen(mymsg)-1);
                            break;

                        /* create table
                        case T_CreateStmt:
                            // transformCreateStmt 并不会对传入的
                            // parsetree 的内容做任何修改，
                            // 这个保证在 transformCreateStmt 函数实现中
                            // 有描述，
                            // 如果因为这个保证没实现导致本函数失败，
                            // 那一定是源码出了 BUG．
                            // Run parse analysis ...
                            stmts = transformCreateStmt((CreateStmt *) parsetree,
                                                        queryString);

                            // ... and do it
                            foreach(l, stmts) {
                                Node *stmt = (Node *) lfirst(l);
                                if ( IsA(stmt, CreateStmt) ) {
                                    dispCreateStmt((CreateStmt *) stmt);
                                    checkRule((CreateStmt *) stmt);
                                }
                            }
                            break;*/

                        /* create indnx */
                        case T_IndexStmt:
                            strncat(mymsg, "index name", MYMSG_SIZE-strlen(mymsg)-1);
                            break;

                        /* create view */
                        case T_ViewStmt:
                            strncat(mymsg, "view name", MYMSG_SIZE-strlen(mymsg)-1);
                            break;

                        default:
                            strncat(mymsg, "DB Object name", MYMSG_SIZE-strlen(mymsg)-1);
                            break;
                }

                break;

            case Q_INVALID_CHAR:
                snprintf(mymsg,
                         MYMSG_SIZE,
                         "%s%s",
                         "\b\b\b\b\b\b\b\b",
                         "Qunar PGSQL Auditor: 表名只能由 小写字母(a-z)，数字(0-9)，下划线(_) 构成");
                break;

            default:
                snprintf(mymsg,
                         MYMSG_SIZE,
                         "\b\b\b\b\b\b\b\bQunar PGSQL Auditor: invalid DB Object name ");
                break;
        }

        ereport(ERROR,
                    (errcode(ERRCODE_INTERNAL_ERROR),
                        errmsg("%s", mymsg)));
    }
}

/* getCreateName - 从 CreateXXXStmt 结构体中取出所创建对象的名字
 *
 */
const char *getCreateName(Node *parsetree) {
    const char *unknown = "unknown";

    switch ( nodeTag(parsetree) ) {
        /* tablespace */
        case T_CreateTableSpaceStmt:
            return ((CreateTableSpaceStmt *)parsetree)->tablespacename;
            break;

        /* database */
        case T_CreatedbStmt:
            return ((CreatedbStmt *)parsetree)->dbname;
            break;

        /* schema */
        case T_CreateSchemaStmt:
            return ((CreateSchemaStmt *)parsetree)->schemaname;
            break;

        /* table */
        //case T_CreateStmt:
            // transformCreateStmt 并不会对传入的
            // parsetree 的内容做任何修改，
            // 这个保证在 transformCreateStmt 函数实现中
            // 有描述，
            // 如果因为这个保证没实现导致本函数失败，
            // 那一定是源码出了 BUG．
            /* Run parse analysis ... */
            //stmts = transformCreateStmt((CreateStmt *) parsetree,
            //                            queryString);

            /* ... and do it */
            /*foreach(l, stmts) {
                Node *stmt = (Node *) lfirst(l);
                if ( IsA(stmt, CreateStmt) ) {
                    dispCreateStmt((CreateStmt *) stmt);
                    checkRule((CreateStmt *) stmt);
                }
            }
            break;*/

        /* view
         *
         * src/include/nodes/parsenodes.h
         * typedef struct ViewStmt
         * {
         *     NodeTag     type;
         *     RangeVar   *view;           // the view to be created
         *     List       *aliases;        // target column names
         *     Node       *query;          // the SELECT query (as a raw parse tree)
         *     bool        replace;        // replace an existing view?
         *     List       *options;        // options from WITH clause
         *     ViewCheckOption withCheckOption;    // WITH CHECK OPTION
         * } ViewStmt;
         *
         * src/
         *
         *
         *
         *
         *
         * 代码保证：
         *   若 parsetree->view 为 NULL 或不正确的值，下面的代码会导致
         * 程序崩溃．
         *   那如何保证这代码不会崩溃呢？
         *   view 里存储了要创建的 view 的名字,所以 view 一定不会为 NULL,
         * 不然,pg 程序也走不到这一步,语法分析就会报错了.
         *
         */
        case T_ViewStmt:
            return ((ViewStmt *)parsetree)->view->relname;
            break;

        /* indnx */
        case T_IndexStmt:
            return ((IndexStmt *)parsetree)->idxname;
            break;

        default:
            return unknown;
            break;
    }
}
