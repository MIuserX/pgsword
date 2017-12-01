/* -------------------------------------------------------------------------
 *
 * qaudit.c
 *
 * Copyright (c) 2017-2017, Qunar DBA Group
 *
 * IDENTIFICATION
 *      contrib/qaudit/qaudit.c
 *
 * -------------------------------------------------------------------------
 */
#include "postgres.h"
//#include "nodes/parsenodes.h"
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

PG_MODULE_MAGIC;

static bool qaudit_enabled = false;
static bool qaudit_test = false;

static ProcessUtility_hook_type prev_ProcessUtility_hook = NULL;

typedef struct ConstrList {
    bool   is_not_null;
    bool   is_primary_key;
    bool   is_unique;
    bool   has_default;
    char  *default_str;
} ConstrList;

void _PG_init(void);
void _PG_fini(void);
void dispCreateStmt(CreateStmt *stmt);
void initConstrList(ConstrList *clist);
void getConstrList(ConstrList *cListStruct, List *cons);
int  isKeyword(const char *str);
void checkRule(CreateStmt *stmt);

static void my_process_utility(PlannedStmt *pstmt,
                               const char *queryString, ProcessUtilityContext context,
                               ParamListInfo params,
                               QueryEnvironment *queryEnv,
                               DestReceiver *dest, char *completionTag);


static void my_process_utility(PlannedStmt *pstmt,
                               const char *queryString, ProcessUtilityContext context,
                               ParamListInfo params,
                               QueryEnvironment *queryEnv,
                               DestReceiver *dest, char *completionTag)
{
    List       *stmts;
    ListCell   *l;
    Node *parsetree = pstmt->utilityStmt;
    bool  isTopLevel = (context == PROCESS_UTILITY_TOPLEVEL);

    if ( !qaudit_enabled )
        goto NOT_ENABLED;

    // 自己加入的逻辑
    if ( pstmt && nodeTag(parsetree) == T_CreateStmt ) {
        //elog(LOG, "QAudit found a CREATE TABLE stmt");
        //ereport(NOTICE, 
        //            (errcode(ERRCODE_SUCCESSFUL_COMPLETION), 
        //                errmsg_internal("QAudit found a CREATE TABLE stmt")));
    
        // transformCreateStmt 并不会对传入的
        // parsetree 的内容做任何修改，
        // 这个保证在 transformCreateStmt 函数实现中
        // 有描述，
        // 如果因为这个保证没实现导致本函数失败，
        // 那一定是源码出了 BUG．
        /* Run parse analysis ... */
        stmts = transformCreateStmt((CreateStmt *) parsetree,
                                    queryString);
        
        /* ... and do it */
        foreach(l, stmts) {
            Node *stmt = (Node *) lfirst(l);
            if ( IsA(stmt, CreateStmt) ) {
                dispCreateStmt((CreateStmt *) stmt);
                checkRule((CreateStmt *) stmt);
            }
        }
    }

NOT_ENABLED:
    // 执行 pg 原有逻辑
    if (prev_ProcessUtility_hook) {
        prev_ProcessUtility_hook(pstmt,
                                 queryString, context,
                                 params,
                                 queryEnv,
                                 dest, completionTag);

    } else {
        standard_ProcessUtility(pstmt,
                                queryString, context,
                                params,
                                queryEnv,
                                dest, completionTag);
    }
}

void dispCreateStmt(CreateStmt *stmt) {
    ListCell    *listptr;
    char        *typname = NULL;
    Form_pg_type typeForm;
    HeapTuple    tuple;
    char         msg[1024] = { 0 };
    char         default_info[512] = { 0 };
    int          msg_pos = 0;
    int          msgNBytes = 0;
    ConstrList   constrList;

    if ( !stmt && !(stmt->relation)) {
        return ;
    }

    if ( stmt->relation->relname ) {
        snprintf(msg, 512, "my_process_utility:\n \
                            table name = \"%s\"\n \
                            inhRelations = %s", 
                            stmt->relation->relname,
                            stmt->inhRelations == NIL ? "NIL" : "NON_NIL");

        ereport(NOTICE, 
                    (errcode(ERRCODE_SUCCESSFUL_COMPLETION), 
                        errmsg("%s", msg)));
    }

    foreach(listptr, stmt->tableElts) 
    {
        ColumnDef *colDef = lfirst(listptr);

        Oid atttypid;
        int32 atttypmod;
        typenameTypeIdAndMod(NULL, colDef->typeName, &atttypid, &atttypmod);
        
        tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(atttypid));
        if ( HeapTupleIsValid(tuple) ) {
            typeForm = (Form_pg_type) GETSTRUCT(tuple);
            typname = typeForm->typname.data;
        }
        ReleaseSysCache(tuple);
        
        // 这个循环尝试从 colDef 中取出 column 的约束
        if ( colDef->constraints ) {
            initConstrList( &constrList );
            getConstrList( &constrList, colDef->constraints );
        }

        if ( constrList.has_default ) {
            snprintf(default_info, 512, "DEFAULT");
            if ( constrList.default_str != NULL ) {
                //snprintf(default_info, 505, constrList.default_str);
            }
        }

        msgNBytes = snprintf(msg + msg_pos, 
                                1024 - msg_pos,
                                "colname \"%s\", typname \"%s\", typoid %d %s %s %s %s\n         ",
                                colDef->colname,
                                typname == NULL ? "unkown" : typname,
                                atttypid,
                                constrList.is_primary_key ? "PRIMARY KEY" : "",
                                constrList.is_unique ? "UNIQUE" : "",
                                constrList.is_not_null ? "NOT NULL" : "",
                                constrList.has_default ? default_info : "");

        msg_pos += msgNBytes;
                                
    }

    ereport(NOTICE, 
                (errcode(ERRCODE_SUCCESSFUL_COMPLETION), 
                    errmsg("%s", msg)));
}

void initConstrList(ConstrList *clist) {
    clist->is_primary_key = false;
    clist->is_unique   = false;
    clist->is_not_null = false;
    clist->has_default = false;
    clist->default_str = NULL;
}

void getConstrList(ConstrList *cListStruct, List *cons) {
    ListCell *clist;

    foreach(clist, cons)
    {
        Constraint *con = (Constraint *) lfirst(clist);

        switch ( con->contype ) {
            case CONSTR_PRIMARY:
                cListStruct->is_primary_key = true;
                break;  
            case CONSTR_UNIQUE:
                cListStruct->is_unique = true;
                break;  
            case CONSTR_NOTNULL:
                cListStruct->is_not_null = true;
                break;  
            case CONSTR_DEFAULT:
                cListStruct->has_default = true;
                cListStruct->default_str = con->cooked_expr;
                break;
            default:
                break;
        }
    }
}

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
                        errmsg("QAudit: PostgreSQL keyword \"%s\" cannot be table name", 
                                stmt->relation->relname)));
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

        typenameTypeIdAndMod(NULL, colDef->typeName, &atttypid, &atttypmod);

        // rule: 使用 timestamptz 替代 timestamp
        //`if ( atttypid == 1114 ) {
        //`    ereport(ERROR, 
        //`            (errcode(ERRCODE_INTERNAL_ERROR),
        //`                errmsg("QunarSQLAudit: replace \"timestatmp\" to \"timestamptz\", please")));
        //`}
        replaceTimestampToTimestamptz(colDef);

        // 获取 type name
        tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(atttypid));
        if ( HeapTupleIsValid(tuple) ) {
            typeForm = (Form_pg_type) GETSTRUCT(tuple);
            typname = typeForm->typname.data;
        }
        ReleaseSysCache(tuple);
        /*
          ereport(NOTICE, 
                      (errcode(ERRCODE_SUCCESSFUL_COMPLETION), 
                          errmsg("my_process_utility: colname is \"%s\" ,\n 
                                  typeName->typeOid is %u ,\n 
                                  type name is \"%s\"", 
                              colDef->colname, atttypid, 
                              typname == NULL ? "unkown" : typname)));
        */

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
                  ) ) {
            
            ereport(ERROR, 
                        (errcode(ERRCODE_INTERNAL_ERROR),
                            errmsg("QunarSQLAudit: the type must be smallserial, serial or bigserial which column has PRIMARY KEY constraint")));
        }
    }
    
}

int isKeyword(const char *str) {
    const ScanKeyword *scanKeywords = NULL;

    if ( str ) {
        scanKeywords = ScanKeywordLookup(str,
                                            ScanKeywords,
                                            NumScanKeywords);

        if ( scanKeywords ) {
            return 1;
        }
    }

    return 0;
}

void _PG_init(void) {

    DefineCustomBoolVariable("QAudit.enabled",
                             "是否开启 QAudit 功能",
                             NULL,
                             &qaudit_enabled,
                             false,
                             PGC_SUSET,
                             0,
                             NULL,
                             NULL,
                             NULL);
 
    prev_ProcessUtility_hook = ProcessUtility_hook;
    ProcessUtility_hook = my_process_utility;
}

void _PG_fini(void) {
    ProcessUtility_hook = prev_ProcessUtility_hook;
}
