#pragma once
#include <sqlite3.h>
#include <string>

std::string nowIso();
void check(int rc, sqlite3* db);
