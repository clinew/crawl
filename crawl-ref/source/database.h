/**
 * @file
 * database.h
**/


#ifndef DATABASE_H
#define DATABASE_H

#include "externs.h"
#include <list>

#ifdef DB_NDBM
extern "C" {
#   include <ndbm.h>
}
#elif defined(DB_DBH)
extern "C" {
#   define DB_DBM_HSEARCH 1
#   include <db.h>
}
#elif defined(USE_SQLITE_DBM)
#   include "sqldbm.h"
#else
#   error DBM interfaces unavailable!
#endif

#define DPTR_COERCE char *

void databaseSystemInit();
void databaseSystemShutdown();

typedef bool (*db_find_filter)(std::string key, std::string body);

std::string getQuoteString(const std::string &key);
std::string getLongDescription(const std::string &key);

std::vector<std::string> getLongDescKeysByRegex(const std::string &regex,
                                                db_find_filter filter = NULL);
std::vector<std::string> getLongDescBodiesByRegex(const std::string &regex,
                                                  db_find_filter filter = NULL);

std::string getGameStartDescription(const std::string &key);

std::string getShoutString(const std::string &monst,
                           const std::string &suffix = "");
std::string getSpeakString(const std::string &key);
std::string getRandNameString(const std::string &itemtype,
                              const std::string &suffix = "");
std::string getHelpString(const std::string &topic);
std::string getMiscString(const std::string &misc,
                          const std::string &suffix = "");
std::string getHintString(const std::string &key);

std::vector<std::string> getAllFAQKeys(void);
std::string getFAQ_Question(const std::string &key);
std::string getFAQ_Answer(const std::string &question);
#endif
