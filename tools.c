#include "postgres.h"
#include "common/keywords.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"     // Form_pg_type
#include "access/htup_details.h" // GETSTRUCT
#include "parser/parse_type.h"   //
#include "utils/syscache.h"      // HeapTuple
#include "utils/elog.h"          // ereport()
#include "nodes/plannodes.h"
#include "nodes/nodes.h"
#include "nodes/parsenodes.h"

#include "tools.h"

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
                break;
            default:
                break;
        }
    }
}

ColInfo getColName (ColumnDef *colDef) {
    Oid            atttypid;
    int32          atttypmod;
    HeapTuple      tuple;
    Form_pg_type   typForm;
    const char    *typName = NULL;
    ColInfo        colInfo;

    typenameTypeIdAndMod(NULL, colDef->typeName, &atttypid, &atttypmod);
    colInfo.atttypid = atttypid;
    colInfo.atttypmod = atttypmod;

    tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(atttypid));
    if ( HeapTupleIsValid(tuple) ) {
        typForm = (Form_pg_type) GETSTRUCT(tuple);
        colInfo.atttypname = typName = typForm->typname.data;
    }
    ReleaseSysCache(tuple);

    return colInfo;
}

/*
 * 退出 SQL 审核过程，输出审核失败和成功的信息
 *
 *   根据测试，ereport(ERROR 会输出 "ERROR:  " 8个字符前缀，
 * 后面跟着我们自己可定制的信息．
 *   我们现在想要的是不要用户看到 "ERROR:  " 这个前缀，有两种
 * 思路：
 *   一是：控制 pg 输出的信息，直接让它不要输出这个前缀，最好是
 *        能输出我们定制的前缀，但完成这个功能的时间成本比较高,
 *        先不使用这种实现．
 *   二是：欺骗用户的眼睛，我们在 "ERROR:  " 这个前缀后面补8个
 *        '\b' 字符，删除这个前缀．而且已经验证，多余的'\b'字符
 *        也不会删除到上一行的内容．这个思路实现时间成本低，目前先
 *        采用这种实现方式．
 *
 */
void finishAudit() {
    char mymsg[512] = { 0 };

    snprintf(mymsg, 512, "\b\b\b\b\b\b\b\b%s",
             "QunarPGSQLAudit:  AUDIT OK");

    ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                    errmsg("%s", mymsg)));
}

void disp_VariableSetStmt(VariableSetStmt *stmt, char *mymsg) {
    snprintf(mymsg,
             MYMSG_SIZE,
             "%s %s, is_local = %s",
             "\b\b\b\b\b\b\b\b\bswordtest: SET",
             stmt->name,
             stmt->is_local ? "True" : "False");

    ereport(NOTICE,
            (errcode(ERRCODE_SUCCESSFUL_COMPLETION),
             errmsg("%s", mymsg)));
}

/* dispStmt - 打印出我们关注的 Stmt 的信息
 *
 *
 * PlannedStmt 结构体: src/include/nodes/plannodes.h
 * Node 结构体:        src/include/nodes/nodes.h
 * nodeTag 函数:       src/include/nodes/parsenodes.h
 * T_CreateStmt 常量:  src/include/nodes/nodes.h
 * CreatedbStmt 常量:  src/include/nodes/parsenodes.h
 */

void dispStmt(PlannedStmt *pstmt) {
    Node *parsetree = pstmt->utilityStmt;
    char  mymsg[MYMSG_SIZE] = { 0 };

    switch ( nodeTag(parsetree) ) {
        case T_CreatedbStmt:
            snprintf(mymsg, MYMSG_SIZE, "%s\"%s\"",
                    "\b\b\b\b\b\b\b\b\bPGSword: dbname=",
                    ((CreatedbStmt *)parsetree)->dbname);
            ereport(NOTICE,
                        (errcode(ERRCODE_SUCCESSFUL_COMPLETION),
                            errmsg("%s", mymsg)));
            break;

        case T_CreateSchemaStmt:
            snprintf(mymsg, MYMSG_SIZE, "%s\"%s\"",
                    "\b\b\b\b\b\b\b\b\bPGSword: schemaname=",
                    ((CreateSchemaStmt *)parsetree)->schemaname);
            ereport(NOTICE,
                        (errcode(ERRCODE_SUCCESSFUL_COMPLETION),
                            errmsg("%s", mymsg)));
            break;

        case T_CreateStmt:
            dispCreateStmt((CreateStmt *)parsetree);
            break;

        case T_VariableSetStmt:
            disp_VariableSetStmt((VariableSetStmt *)parsetree, mymsg);
            break;

        case T_DeleteStmt:
            snprintf(mymsg, MYMSG_SIZE,
                     "\b\b\b\b\b\b\b\b\bPGSword: DELETE stmt");
            ereport(NOTICE,
                (errcode(ERRCODE_SUCCESSFUL_COMPLETION),
                    errmsg("%s", mymsg)));
            break;

        default:
            break;
    }
}

/*
 *
 * CreateStmt struct:
 * ListCell struct:
 * Form_pg_type struct:
 * HeapTuple struct:
 * List struct:
 * ereport func:
 * NOTICE macro:
 * errcode func:
 * errmsg func:
 * ERRCODE_SUCCESSFUL_COMPLETION macro:
 * ColumnDef struct:
 * Oid type:
 * int32 type:
 * typenameTypeIdAndMod func:
 * SearchSysCache1 func:
 * TYPEOID macro:
 * ObjectIdGetDatum func:
 * HeapTupleIsValid func:
 * GETSTRUCT macro:
 * ReleaseSysCache func:
 * NameListToString func:
 * FuncCall struct:
 * TypeCase struct:
 */
void dispCreateStmt(CreateStmt *stmt) {
    ListCell    *listptr;
    ListCell    *lc;
    char        *typname = NULL;
    Form_pg_type typeForm;
    HeapTuple    tuple;
    char         msg[1024] = { 0 };
    char         default_info[512] = { 0 };
    int          msg_pos = 0;
    int          msgNBytes = 0;
    ConstrList   constrList;
    List        *argList = NULL;

    if ( !stmt && !(stmt->relation)) {
        return ;
    }

    if ( stmt->relation->relname ) {
        snprintf(msg, 512, "my_process_utility:\n \
                            table name = \"%s\"",
                            stmt->relation->relname);

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
        initConstrList( &constrList );
        if ( colDef->constraints ) {
            getConstrList( &constrList, colDef->constraints );
        }

        if ( constrList.has_default ) {
            snprintf(default_info, 512, "DEFAULT");
            if ( colDef->raw_default != NULL ) {
                snprintf(default_info + 7,
                         505,
                         "(raw, %s)",
                         NameListToString(((FuncCall *)colDef->raw_default)->funcname)
                        );
            }

            argList = ((FuncCall *)colDef->raw_default)->args;
            foreach(lc, argList)
            {
                Node *aNode = lfirst(lc);
                snprintf(default_info + strlen(default_info),
                         512 - strlen(default_info),
                         "args %d typ %d",
                         nodeTag(aNode),
                         nodeTag(((TypeCast *)aNode)->arg)
                        );
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
