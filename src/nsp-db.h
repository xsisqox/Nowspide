/*
 * Copyright © 2009-2010 Siyan Panayotov <xsisqox@gmail.com>
 *
 * This file is part of Nowspide.
 *
 * Nowspide is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nowspide is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nowspide.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <sqlite3.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "nsp-feed.h"
#include "config.h"

#ifndef __NSP_DB_H__
#define __NSP_DB_H__ 1

typedef struct _NspDb NspDb;

struct _NspDb {
	sqlite3 *db;
	int transaction_started;
	GMutex *mutex;
};


NspDb * nsp_db_get();
void	nsp_db_close();

void	nsp_db_transaction_begin(NspDb *nsp_db);
void	nsp_db_transaction_end(NspDb *nsp_db);

int		nsp_db_atom_int(void *user_data, int argc, char **argv, char ** azColName);

#endif /* __NSP_DB_H__ */
