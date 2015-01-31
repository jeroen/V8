library(RSQLite)
library(V8)

tmp <- tempfile()
con <- dbConnect(SQLite(), tmp)
dbWriteTable(con, "USArrests", USArrests)
buf <- readBin(tmp, raw(), file.info(tmp)$size)
fetch(dbSendQuery(con, "SELECT * FROM USArrests"))

# In JavaScript
ct <- new_context()
ct$source("~/workspace/sql.js/js/sql.js")

ct$assign("buf", I(buf))
ct$eval('
var uInt8Array = new ArrayBuffer(buf);
var db = new SQL.Database(uInt8Array);
db.exec("select * from USArrests")
')

dbDisconnect(con)
