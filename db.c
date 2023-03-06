#include <stdio.h>
#include <sqlite3.h>
#include "db.h"

sqlite3 *db;

static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
   int i;
   for(i = 0; i<argc; i++) {
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}

int init_db() {
	int rc = sqlite3_open("chess.db", &db);

	if (rc) {
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
	} else {
		fprintf(stderr, "Opened database successfully\n");
	}

	rc = setup_db();

  return rc;
}

int setup_db() {
	char *sql = "CREATE TABLE IF NOT EXISTS `chess_positions` (" \
							"serial char(70) NOT NULL," \
							"move varchar(7) NOT NULL," \
							"wins int(11) NOT NULL DEFAULT 0," \
							"losses int(11) NOT NULL DEFAULT 0," \
							"draws int(11) NOT NULL DEFAULT 0," \
							"CONSTRAINT serial_move PRIMARY KEY (serial,move)" \
							");";

	int rc = exec_query(sql);
   
  return rc;
}

int exec_query(char *sql) {
	char *zErrMsg = 0;
	int rc;
	
	rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
	
	if( rc != SQLITE_OK ){
  	fprintf(stderr, "Error running SQL: %s\n Error message: %s\n", sql, zErrMsg);
  	sqlite3_free(zErrMsg);
  }

	return rc;
}

int close_db() {
	sqlite3_close(db);

  return 0;
}
