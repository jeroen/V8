library(V8)
library(jsonlite)
stopifnot(packageVersion("V8") > "0.4.1")

ct <- v8()
ct$source("https://raw.githubusercontent.com/kripken/sql.js/master/js/sql.js")

ct$eval('
var db = new SQL.Database()
db.run("CREATE TABLE hello (person char, age int);")
db.run("INSERT INTO hello VALUES (\'jerry\', 34);")
db.run("INSERT INTO hello VALUES (\'mary\', 27);")
data = db.export();
')

# Hack to get the data via json
data <- ct$get("data")
bin <- as.raw(unname(unlist(head(data$buffer, -1))))
tmp <- tempfile()
writeBin(bin, tmp)

# Load into RSQLite
library(RSQLite)
con <- dbConnect(RSQLite::SQLite(), tmp)
dbListTables(con)
res <- dbSendQuery(con, "SELECT * FROM hello")
dbFetch(res)
dbClearResult(res)
dbDisconnect(con)
