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

#include "nsp-feed.h"
#include "nsp-feed-parser.h"
#include "nsp-feed-list.h"
#include "nsp-feed-item-list.h"
#include "nsp-app.h"

#include <curl/curl.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

static int 
nsp_feed_load_feed_items_callback(void *user_data, int argc, char **argv, char ** azColName)
{
	GList **feed_items = (GList**) user_data;
	NspFeedItem *feed_item = nsp_feed_item_new();
	
	feed_item->id = atoi(argv[0]);
	feed_item->feed_id = atoi(argv[1]);
	feed_item->status = atoi(argv[6]);
	feed_item->title = g_strdup(argv[2]);
	feed_item->link = g_strdup(argv[3]);
	feed_item->description = g_strdup(argv[4]);
	time_t time = (time_t)atol(argv[5]);
	
	feed_item->pubdate = malloc(sizeof(struct tm));
	memcpy( feed_item->pubdate, localtime(&time), sizeof(struct tm));
	
	*feed_items = g_list_prepend(*feed_items, (gpointer) feed_item);
	
	return 0;
}

static int 
nsp_feed_load_feeds_callback(void *user_data, int argc, char **argv, char ** azColName)
{
	GList **feeds = (GList**) user_data;
	NspFeed *f = nsp_feed_new();
	
	f->id = atoi(argv[0]);
	f->title = g_strdup(argv[1]);
	f->url = g_strdup(argv[2]);
	f->site_url = g_strdup(argv[3]);
	f->description = g_strdup(argv[4]);
	f->icon = gdk_pixbuf_new_from_file(g_build_filename( g_get_user_data_dir(), PACKAGE, "/icons/", argv[0], NULL), NULL);
	
	*feeds = g_list_prepend(*feeds, (gpointer) f);
	
	return 0;
}

static gint
nsp_feed_sort_date (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	if ( model == NULL || a == NULL || b == NULL) {
		return 0;
	}
	NspFeedItem *item_a, *item_b;
	time_t t_a, t_b;
	
	gtk_tree_model_get(model, a, ITEM_LIST_COL_ITEM_REF, &item_a, -1);
	gtk_tree_model_get(model, b, ITEM_LIST_COL_ITEM_REF, &item_b, -1);
	
	if ( item_a == NULL || item_b == NULL || item_a->pubdate == NULL || item_b->pubdate == NULL) {
		return 0;
	}
	t_a = mktime(item_a->pubdate);
	t_b = mktime(item_b->pubdate);
	
	return t_a - t_b;
}

NspFeed * 
nsp_feed_new()
{
	NspFeed * feed = malloc(sizeof(NspFeed));
	assert(feed != NULL);
	
	feed->type = NSP_FEED_UNKNOWN;
	feed->items = NULL;
	feed->title = feed->url = feed->site_url = feed->description = NULL;
	feed->id = 0;
	feed->unread_items = 0;
	feed->items_store = nsp_feed_item_list_get_model();
	feed->items_sorter = gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(feed->items_store));
	feed->mutex = g_mutex_new();
	feed->icon = NULL;
	
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (feed->items_sorter), ITEM_LIST_COL_DATE, GTK_SORT_DESCENDING);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE (feed->items_sorter), ITEM_LIST_COL_DATE, (GtkTreeIterCompareFunc)nsp_feed_sort_date, NULL, NULL);
	
	
	return feed;
}

GtkTreeModel *
nsp_feed_get_items_model (NspFeed *feed)
{
	return feed->items_sorter;
}

NspFeed * 
nsp_feed_new_from_url(const char *url)
{
	NspFeed * feed = nsp_feed_new();
	feed->url = (char*) url;
	
	if ( nsp_feed_update_items(feed) ) {
		nsp_feed_free(feed);
		return NULL;
	}
	
	return feed;
}

void
nsp_feed_free (NspFeed *feed)
{
	if ( feed == NULL)
		return;
	
	nsp_feed_clear_items(feed);
	
	free(feed->title);
	free(feed->url);
	free(feed->site_url);
	free(feed->description);
	g_mutex_free(feed->mutex);
}

int
nsp_feed_clear_items(NspFeed *feed)
{
	assert(feed != NULL);
	while (feed->items != NULL) {
		nsp_feed_item_free((NspFeedItem*) feed->items->data);
		feed->items = feed->items->next;
	}
	
	return 0;
}

int
nsp_feed_update_items(NspFeed *feed)
{
	NspNetData *data;
	xmlDoc *xml_doc;
	
	data = nsp_net_new();
	if ( nsp_net_load_url(feed->url, data) ) {
		g_warning("ERROR: %s\n", data->error);
		nsp_net_free(data);
		return 1;
	}
	
	xml_doc =  xmlReadMemory(data->content, data->size, NULL, NULL, 0);
	if ( xml_doc == NULL ) {
		g_warning("Error parsing xml!\n");
		nsp_net_free(data);
		return 1;
	}
	
	g_mutex_lock(feed->mutex);
	
	nsp_feed_clear_items(feed);
	nsp_feed_parse(xml_doc, feed);
	
	xmlFreeDoc(xml_doc);
	nsp_net_free(data);
	
	
	if ( feed->id != 0 ) {
		nsp_feed_save_to_db(feed);
		nsp_feed_load_items_from_db(feed);
	}
	g_mutex_unlock(feed->mutex);
	
	return 0;
}

void
nsp_feed_update_model(NspFeed *feed) 
{
	GtkTreeIter iter;
	NspFeedItem *item;
	GList *items = feed->items;
	
	gtk_tree_store_clear(GTK_TREE_STORE(feed->items_store));
	while( items != NULL ) {
		item = (NspFeedItem*) items->data;
		gtk_tree_store_append (GTK_TREE_STORE(feed->items_store), &iter, NULL);
		
		nsp_feed_item_list_update_iter(iter, GTK_TREE_STORE(feed->items_store), item);
		
		items = items->next;
	}
}

void
nsp_feed_update_unread_count(NspFeed *feed) 
{
	NspDb *db = nsp_db_get();
	char *error = NULL;
	int stat;
	int count;
	
	char *query = sqlite3_mprintf("SELECT COUNT(1) FROM nsp_feed f LEFT JOIN nsp_feed_item fi ON fi.feed_id = f.id WHERE f.id = %i AND fi.status = 1", feed->id);
	
	stat = sqlite3_exec(db->db, query, nsp_db_atom_int, &count, &error);
	sqlite3_free(query);
	
	if ( stat != SQLITE_OK ) {
		if ( error == NULL) {
			g_warning("Error: %s\n", sqlite3_errmsg(db->db));
		} else {
			g_warning("Error: %s\n", error);
			sqlite3_free(error);
		}
		
		return;
	}
	
	feed->unread_items = count;
}

int
nsp_feed_load_items_from_db(NspFeed *feed)
{
	NspDb *db = nsp_db_get();
	char *error = NULL;
	int stat;
	
	char *query = sqlite3_mprintf("SELECT id, feed_id, title, url, description, date, status FROM nsp_feed_item WHERE feed_id=%i ORDER BY id DESC", feed->id);
	
	nsp_feed_clear_items(feed);
	
	stat = sqlite3_exec(db->db, query, nsp_feed_load_feed_items_callback, &(feed->items), &error);
	sqlite3_free(query);
	
	if ( stat != SQLITE_OK ) {
		if ( error == NULL) {
			g_warning("Error: %s\n", sqlite3_errmsg(db->db));
		} else {
			g_warning("Error: %s\n", error);
			sqlite3_free(error);
		}
		
		return 1;
	}
	
	nsp_feed_update_unread_count(feed);
	
	return 0;
}

GList * 
nsp_feed_load_feeds_from_db()
{
	NspDb *db = nsp_db_get();
	char *error = NULL;
	GList *feed_list = NULL;
	int stat;
	
	stat = sqlite3_exec(db->db, "SELECT id, title, url, site_url, description FROM nsp_feed ORDER BY id DESC", nsp_feed_load_feeds_callback, &feed_list, &error);
	if ( stat != SQLITE_OK ) {
		if ( error == NULL) {
			g_warning("Error: %s\n", sqlite3_errmsg(db->db));
		} else {
			g_warning("Error: %s\n", error);
			sqlite3_free(error);
		}
		
		return NULL;
	}
	
	return feed_list;
}


GList * 
nsp_feed_load_feeds_with_items_from_db()
{
	GList *feeds = nsp_feed_load_feeds_from_db();
	GList *tmp = feeds;
	NspFeed *feed = NULL;
	
	while ( tmp != NULL ) {
		feed = (NspFeed *) tmp->data;
		nsp_feed_load_items_from_db(feed);
		tmp = tmp->next;
	}
	
	return feeds;
}

int
nsp_feed_save_to_db(NspFeed *feed)
{
	NspDb *db = nsp_db_get();
	NspFeedItem *tmp = NULL;
	GList *tmp_list = NULL;
	char *error = NULL;
	char *feed_id = g_strdup_printf("%i", feed->id);
	int stat;
	
	char *query = sqlite3_mprintf("INSERT %s INTO nsp_feed (id, title, url, site_url, description) VALUES (%s, '%q', '%q', '%q', '%q')", ((feed->id==0) ? "" : "OR REPLACE " ), ((feed->id==0) ? "NULL" : feed_id),feed->title, feed->url, feed->site_url, feed->description);
	
	nsp_db_transaction_begin(db);
	
	stat = sqlite3_exec(db->db, query, NULL, NULL, &error);
	sqlite3_free(query);
	g_free(feed_id);
	
	if ( stat != SQLITE_OK ) {
		if ( error == NULL) {
			g_warning("Error: %s\n", sqlite3_errmsg(db->db));
		} else {
			g_warning("Error: %s\n", error);
			sqlite3_free(error);
		}
	
		nsp_db_transaction_end(db);
	
		return 1;
	}
	
	feed->id = sqlite3_last_insert_rowid(db->db);
	
	tmp_list = feed->items;
	while ( tmp_list != NULL ) {
		tmp = (NspFeedItem *) tmp_list->data;
		tmp->feed_id = feed->id;
		
		if (nsp_feed_item_save_to_db(tmp)) {
			nsp_db_transaction_end(db);
			return 1;
		}
		
		tmp_list = tmp_list->next;
	}
	
	
	nsp_db_transaction_end(db);
	
	return 0;
	
}

static void 
nsp_feed_delete_item_from_db(void *user_data)
{
	int *feed_item_id = (int*)user_data;
	NspDb *db = nsp_db_get();
	char *query = NULL;
	char *error = NULL;
	int stat;
	assert(feed_item_id != NULL);
	
	query = sqlite3_mprintf(
			"DELETE FROM nsp_feed_item WHERE id = %i", 
			*feed_item_id
		);
	
	stat = sqlite3_exec(db->db, query, NULL, NULL, &error);
	sqlite3_free(query);
	
	nsp_db_transaction_end(db);
	
	if ( stat != SQLITE_OK ) {
		if ( error == NULL) {
			g_warning("Error: %s\n", sqlite3_errmsg(db->db));
		} else {
			g_warning("Error: %s\n", error);
			sqlite3_free(error);
		}
	}
}

int 
nsp_feed_delete_item(NspFeed *feed, NspFeedItem *feed_item)
{	
	NspFeedItem *tmp_feed_item = NULL;
	NspApp *app = nsp_app_get();
	GtkTreeIter iter;
	gboolean valid;
	int *feed_item_id = malloc(sizeof(int));
	*feed_item_id = feed_item->id;
	
	feed_item->status |= NSP_FEED_ITEM_DELETED;
	nsp_jobs_queue(app->jobs, nsp_job_new( (NspCallback*)nsp_feed_delete_item_from_db, feed_item_id ));
	
	g_mutex_lock(feed->mutex);
	valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL(feed->items_store), &iter);
	while (valid)
	{
		gtk_tree_model_get (GTK_TREE_MODEL(feed->items_store), &iter,
				ITEM_LIST_COL_ITEM_REF, &tmp_feed_item,
				-1
			);
		
		if ( tmp_feed_item != NULL && tmp_feed_item == feed_item ) {
			break;
		}
		
		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL(feed->items_store), &iter);
	}
	
	if ( valid ) {
		gtk_tree_store_remove(feed->items_store, &iter);
	}
	g_mutex_unlock(feed->mutex);
	
	return 0;
}

static size_t 
nsp_feed_download_icon_cb (char *buffer, size_t size, size_t nitems, void *data)
{
	GdkPixbufLoader *loader = (GdkPixbufLoader *) data;
	if (gdk_pixbuf_loader_write (loader, (guchar *) buffer, size * nitems, NULL))
		return size * nitems;
	return 0;
}

int
nsp_feed_update_icon(NspFeed *feed)
{
	NspNetData *data;
	char *icon_url = NULL;
	char *icon_path = NULL;
	GRegex *regex;
	GMatchInfo *match_info;
	CURL *curl;
	CURLcode result;
	GdkPixbufLoader *loader = NULL;
	GError *error = NULL;
	char *feed_id_string = NULL;
	
	/* Download web page */
	data = nsp_net_new();
	if ( nsp_net_load_url(feed->site_url, data) ) {
		g_warning("ERROR: %s\n", data->error);
		nsp_net_free(data);
		return 1;
	}
	
	/* Find <link> tag reffering to the icon */
	regex = g_regex_new ("<link[^>]*?rel=[\"'](icon|shortcut icon)[\"'][^>]*?href=[\"'](?<href>.*?)[\"'][^>]*?>", 0, 0, NULL);
	g_regex_match (regex, data->content, 0, &match_info);
	
	while (g_match_info_matches (match_info)) {
		gchar *word = g_match_info_fetch_named (match_info, "href");
		if ( !g_str_has_prefix(word, "http") ) {
			icon_url = g_strdup_printf("%s/%s", feed->site_url, word);
			g_free(word);
			break;
		}
		g_free (word);
		g_match_info_next (match_info, NULL);
	}
	g_match_info_free (match_info);
	g_regex_unref (regex);
	nsp_net_free(data);
	
	/* If no image is found - use home url + /favicon.ico */
	if ( icon_url == NULL ) {
		regex = g_regex_new ("^(?<hostname>https?://[^/]*)", 0, 0, NULL);
		g_regex_match (regex, feed->site_url, 0, &match_info);
		
		if (g_match_info_matches (match_info)) {
			gchar *word = g_match_info_fetch_named (match_info, "hostname");
			icon_url = g_strdup_printf("%s/favicon.ico", word);
			g_free (word);
		}
		
		g_match_info_free (match_info);
		g_regex_unref (regex);
	}
	
	/* Store the image to a GtkPixbufLoader */
	loader = gdk_pixbuf_loader_new();
	curl = curl_easy_init();
	curl_easy_setopt (curl, CURLOPT_URL, icon_url);
	curl_easy_setopt (curl, CURLOPT_HEADER, 0);
	curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, nsp_feed_download_icon_cb );
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, loader);
	curl_easy_setopt (curl, CURLOPT_TIMEOUT, 60);
	curl_easy_setopt (curl, CURLOPT_NOSIGNAL, 1);
	
	result = curl_easy_perform (curl);
	curl_easy_cleanup(curl);
	g_free(icon_url);
	
	if ( result != 0) {
		return 1;
	}
	
	gdk_pixbuf_loader_close (loader, NULL);
	feed->icon = gdk_pixbuf_loader_get_pixbuf (loader);
	
	if ( !GDK_IS_PIXBUF(feed->icon) ) {
		feed->icon = NULL;
		return 1;
	}
	
	/* Resize and save the image */
	if (gdk_pixbuf_get_width (feed->icon) != 16 || gdk_pixbuf_get_height (feed->icon) != 16) {
		GdkPixbuf *old = feed->icon;
		feed->icon = gdk_pixbuf_scale_simple (old, 16, 16, GDK_INTERP_BILINEAR);
		g_object_unref (G_OBJECT (old));
	}
	
	feed_id_string = malloc(sizeof(char)*9);
	g_snprintf (feed_id_string, 9, "%i", feed->id);
	
	icon_path = g_build_filename( g_get_user_data_dir(), PACKAGE, "/icons", NULL);
	g_mkdir_with_parents(icon_path, 0700);
	
	icon_path = g_build_filename( icon_path, "/", feed_id_string, NULL);
	gdk_pixbuf_save(feed->icon, icon_path, "png", &error, NULL);
	
	if ( error != NULL ) {
		g_warning("ERROR: %s\n", error->message);
		g_error_free(error);
	}
	g_free(icon_path);
	free(feed_id_string);
	
	return 0;
}

void
nsp_feed_read_all(NspFeed *feed)
{
	NspDb *db = nsp_db_get();
	char *query = NULL;
	char *error = NULL;
	int stat;
	assert(feed != NULL);
	
	query = sqlite3_mprintf(
			"UPDATE nsp_feed_item SET status = 0 WHERE feed_id = %i", 
			feed->id
		);
	
	stat = sqlite3_exec(db->db, query, NULL, NULL, &error);
	sqlite3_free(query);
	
	nsp_db_transaction_end(db);
	
	if ( stat != SQLITE_OK ) {
		if ( error == NULL) {
			g_warning("Error: %s\n", sqlite3_errmsg(db->db));
		} else {
			g_warning("Error: %s\n", error);
			sqlite3_free(error);
		}
	}
}

int
nsp_feed_delete(NspFeed *feed)
{
	NspDb *db = nsp_db_get();
	char *error = NULL;
	int stat;
	char *query;
	
	/* Delete related feed items */
	query = sqlite3_mprintf("DELETE FROM nsp_feed_item WHERE feed_id=%i", feed->id);
	
	nsp_feed_clear_items(feed);
	
	g_mutex_lock(feed->mutex);
	
	stat = sqlite3_exec(db->db, query, nsp_feed_load_feed_items_callback, &(feed->items), &error);
	sqlite3_free(query);
	
	if ( stat != SQLITE_OK ) {
		if ( error == NULL) {
			g_warning("Error: %s\n", sqlite3_errmsg(db->db));
		} else {
			g_warning("Error: %s\n", error);
			sqlite3_free(error);
		}
		
		g_mutex_unlock(feed->mutex);
		return 1;
	}
	
	/* Delete the feed */
	query = sqlite3_mprintf("DELETE FROM nsp_feed WHERE id=%i", feed->id);
	
	stat = sqlite3_exec(db->db, query, nsp_feed_load_feed_items_callback, &(feed->items), &error);
	sqlite3_free(query);
	
	if ( stat != SQLITE_OK ) {
		if ( error == NULL) {
			g_warning("Error: %s\n", sqlite3_errmsg(db->db));
		} else {
			g_warning("Error: %s\n", error);
			sqlite3_free(error);
		}
		
		g_mutex_unlock(feed->mutex);
		return 1;
	}
	
	g_mutex_unlock(feed->mutex);
	
	return 0;
}

