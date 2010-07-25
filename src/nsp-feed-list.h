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

#ifndef __NSP_FEED_LIST_H__
#define __NSP_FEED_LIST_H__ 1

#include <gtk/gtk.h>
#include "nsp-feed.h"


typedef void (NspCallback)(void*);

typedef struct _NspFeedList NspFeedList;

struct _NspFeedList {
	GtkTreeModel *list_model;
	GtkWidget *list_view;
	
	NspCallback *on_select;
};

NspFeedList * nsp_feed_list_new();

void nsp_feed_list_add(NspFeedList *list, NspFeed *feed);

#endif /* __NSP_FEED_LIST_VIEW_H__ */