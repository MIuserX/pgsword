/* -------------------------------------------------------------------------
 *
 * pgsword.c
 *
 * Copyright (c) 2017-2017, Qunar DBA Group
 *
 * IDENTIFICATION
 *      contrib/pgsword/pgsword.c
 *
 * -------------------------------------------------------------------------
 */
#include "postgres.h"
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

#include "rule.h"
#include "tools.h"

PG_MODULE_MAGIC;

static bool pgsword_enabled = false;
static bool qaudit_test = false;

static ProcessUtility_hook_type prev_ProcessUtility_hook = NULL;

void _PG_init(void);
void _PG_fini(void);
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

    if ( !pgsword_enabled )
        goto NOT_ENABLED;

    // 自己加入的逻辑
    if ( pstmt ) {

        switch ( nodeTag(parsetree) ) {
            /* create tablespace */
            case T_CreateTableSpaceStmt:
                ereport(NOTICE,
                    (errcode(ERRCODE_SUCCESSFUL_COMPLETION),
                        errmsg("QunarSQLAudit: found a CREATE TABLESPACE stmt")));
                dispStmt(pstmt);
                checkDBObjName(getCreateName(parsetree), T_CreateTableSpaceStmt);
                break;

            /* create database */
            case T_CreatedbStmt:
                ereport(NOTICE,
                    (errcode(ERRCODE_SUCCESSFUL_COMPLETION),
                        errmsg("QunarSQLAudit: found a CREATE DATABASE stmt")));
                dispStmt(pstmt);
                checkDBObjName(getCreateName(parsetree), T_CreatedbStmt);
                break;

            /* create schema */
            case T_CreateSchemaStmt:
                ereport(NOTICE,
                    (errcode(ERRCODE_SUCCESSFUL_COMPLETION),
                        errmsg("QunarSQLAudit: found a CREATE SCHEMA stmt")));
                dispStmt(pstmt);
                checkDBObjName(getCreateName(parsetree), T_CreateSchemaStmt);
                break;

            /* create table */
            case T_CreateStmt:
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
                break;

            /* create view */
            case T_ViewStmt:
                ereport(NOTICE,
                    (errcode(ERRCODE_SUCCESSFUL_COMPLETION),
                        errmsg("QunarSQLAudit: found a CREATE VIEW stmt")));
                dispStmt(pstmt);
                checkDBObjName(getCreateName(parsetree), T_ViewStmt);
                break;

            /* create indnx */
            case T_IndexStmt:
                ereport(NOTICE,
                    (errcode(ERRCODE_SUCCESSFUL_COMPLETION),
                        errmsg("QunarSQLAudit: found a CREATE INDEX stmt")));
                dispStmt(pstmt);
                checkDBObjName(getCreateName(parsetree), T_IndexStmt);
                break;

            /*case T_CreateTrigStmt:

                break;*/

            default:
                break;
        }
    }

    finishAudit();

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


void _PG_init(void) {

    DefineCustomBoolVariable("pgsword.enabled",
                             "是否开启 QAudit 功能",
                             NULL,
                             &pgsword_enabled,
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
