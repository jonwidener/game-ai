#ifndef DB_H_
#define DB_H_

int init_db();
int setup_db();
int exec_query(char *sql);
int close_db();
static int callback(void *NotUsed, int argc, char **argv, char **azColName);

#endif
