#include "postgres.h"
#include "catalog/pg_type.h"     // Form_pg_type
#include "access/htup_details.h" // GETSTRUCT
#include "parser/parse_type.h"   //
#include "utils/syscache.h"      // HeapTuple
#include "utils/elog.h"          // ereport()

#include "tools.h"

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
