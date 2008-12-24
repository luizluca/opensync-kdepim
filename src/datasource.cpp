/**
 * @author Andrew Baumann <andrewb@cse.unsw.edu.au>
 * changed to 0.40 API by Martin Koller <m.koller@surfeu.at>
 */

#include <stdlib.h>
#include <kapplication.h>
#include <qfile.h>

#include "datasource.h"

extern "C"
{
/* NB: the userdata passed to the sink callbacks is not the sink's userdata, as
 * you might expect, but instead the plugin's userdata, which in our case is the
 * KdePluginImplementation object pointer. So we need to get the sink object from
 * osync_objtype_sink_get_userdata(osync_plugin_info_get_sink(info))
 */

static void connect_wrapper(void *userdata, OSyncPluginInfo *info, OSyncContext *ctx)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p, %p)", __PRETTY_FUNCTION__, userdata, info, ctx);
	OSyncDataSource *obj = (OSyncDataSource *)osync_objtype_sink_get_userdata(osync_plugin_info_get_sink(info));
	obj->connect(info, ctx);
	osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

//--------------------------------------------------------------------------------

static void disconnect_wrapper(void *userdata, OSyncPluginInfo *info, OSyncContext *ctx)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p, %p)", __PRETTY_FUNCTION__, userdata, info, ctx);
	OSyncDataSource *obj = (OSyncDataSource *)osync_objtype_sink_get_userdata(osync_plugin_info_get_sink(info));
	obj->disconnect(info, ctx);
	osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

//--------------------------------------------------------------------------------

static void get_changes_wrapper(void *userdata, OSyncPluginInfo *info, OSyncContext *ctx)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p, %p)", __PRETTY_FUNCTION__, userdata, info, ctx);
	OSyncDataSource *obj = (OSyncDataSource *)osync_objtype_sink_get_userdata(osync_plugin_info_get_sink(info));
	obj->get_changes(info, ctx);
	osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

//--------------------------------------------------------------------------------

static void commit_wrapper(void *userdata, OSyncPluginInfo *info, OSyncContext *ctx, OSyncChange *chg)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p, %p, %p)", __PRETTY_FUNCTION__, userdata, info, ctx, chg);
	OSyncDataSource *obj = (OSyncDataSource *)osync_objtype_sink_get_userdata(osync_plugin_info_get_sink(info));
	obj->commit(info, ctx, chg);
	osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

//--------------------------------------------------------------------------------

static void sync_done_wrapper(void *userdata, OSyncPluginInfo *info, OSyncContext *ctx)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p, %p)", __PRETTY_FUNCTION__, userdata, info, ctx);
	OSyncDataSource *obj = (OSyncDataSource *)osync_objtype_sink_get_userdata(osync_plugin_info_get_sink(info));
	obj->sync_done(info, ctx);
	osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

} // extern "C"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

bool OSyncDataSource::initialize(OSyncPlugin *plugin, OSyncPluginInfo *info, OSyncError **error)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p)", __PRETTY_FUNCTION__, plugin, info);

	OSyncObjTypeSink *sink = osync_plugin_info_find_objtype(info, objtype);

	if (sink == NULL) {
		// this objtype is not enabled, but this is not an error
		osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
		return true;
	}

	OSyncObjTypeSinkFunctions functions;
	memset(&functions, 0, sizeof(functions));
	functions.connect     = connect_wrapper;
	functions.disconnect  = disconnect_wrapper;
	functions.get_changes = get_changes_wrapper;
	functions.commit      = commit_wrapper;
	functions.sync_done   = sync_done_wrapper;

	osync_objtype_sink_set_functions(sink, functions, this);

	const char *configdir = osync_plugin_info_get_configdir(info);
	QString tablepath = QString("%1/hashtable.db").arg(configdir);
	hashtable = osync_hashtable_new(QFile::encodeName(tablepath), objtype, error);
	if (hashtable == NULL) {
		osync_trace(TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print(error));
		return false;
	}
        if ( !osync_hashtable_load(hashtable, error) )
        {
		osync_trace(TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print(error));
		return false;
        }

	osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
	return true;
}

//--------------------------------------------------------------------------------

void OSyncDataSource::connect(OSyncPluginInfo *info, OSyncContext *ctx)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p)", __PRETTY_FUNCTION__, info, ctx);

	//Detection mechanismn if this is the first sync
	QString anchorpath = QString("%1/anchor.db").arg(osync_plugin_info_get_configdir(info));
	if (!osync_anchor_compare(anchorpath, objtype, "true")) {
		osync_trace(TRACE_INTERNAL, "Setting slow-sync for %s", objtype);
		OSyncObjTypeSink *sink = osync_plugin_info_find_objtype(info, objtype);
		osync_objtype_sink_set_slowsync(sink, TRUE);
	}
	osync_context_report_success(ctx);

	osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

//--------------------------------------------------------------------------------

void OSyncDataSource::sync_done(OSyncPluginInfo *info, OSyncContext *ctx)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p)", __PRETTY_FUNCTION__, info, ctx);

	//Detection mechanismn if this is the first sync
	QString anchorpath = QString("%1/anchor.db").arg(osync_plugin_info_get_configdir(info));
	osync_anchor_update(anchorpath, objtype, "true");
	osync_context_report_success(ctx);

	OSyncError *error = NULL;
        if ( hashtable && !osync_hashtable_save(hashtable, &error) )
        {
          osync_context_report_osyncerror(ctx, error);
          osync_trace(TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print(&error));
          osync_error_unref(&error);
        }

	osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

//--------------------------------------------------------------------------------

bool OSyncDataSource::report_change(OSyncPluginInfo *info, OSyncContext *ctx,
                                    QString uid, QString data, QString hash, OSyncObjFormat *objformat)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p, %s, (data), (hash), %p)", __PRETTY_FUNCTION__,
                    info, ctx, static_cast<const char*>(uid.utf8()), objformat);

	OSyncError *error = NULL;

	OSyncChange *change = osync_change_new(&error);
	if (!change)
        {
          osync_context_report_osyncerror(ctx, error);
          osync_trace(TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print(&error));
          osync_error_unref(&error);
          return false;
        }

	// Use the hash table to check if the object needs to be reported
	osync_change_set_uid(change, uid.utf8());
	osync_change_set_hash(change, hash.utf8());

	OSyncChangeType changetype = osync_hashtable_get_changetype(hashtable, change);
	osync_change_set_changetype(change, changetype);

	// Update change in hashtable ... otherwise it gets deleted!
	osync_hashtable_update_change(hashtable, change);

	if ( changetype != OSYNC_CHANGE_TYPE_UNMODIFIED )
        {
          char *data_str = strdup((const char *)data.utf8());

          osync_trace(TRACE_SENSITIVE,"Data:\n%s", data_str);

          OSyncData *odata = osync_data_new(data_str, strlen(data_str), objformat, &error);
          if (!odata)
          {
            osync_context_report_osyncerror(ctx, error);
            osync_trace(TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print(&error));
            osync_error_unref(&error);
            osync_change_unref(change);
            return false;
          }

          osync_data_set_objtype(odata, objtype);

          osync_change_set_data(change, odata);
          osync_data_unref(odata);

          osync_context_report_change(ctx, change);
          osync_change_unref(change);
        }

	osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
	return true;
}

//--------------------------------------------------------------------------------

bool OSyncDataSource::report_deleted(OSyncPluginInfo *info, OSyncContext *ctx, OSyncObjFormat *objformat)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p, %p)", __PRETTY_FUNCTION__, info, ctx, objformat);

	OSyncError *error = NULL;
	OSyncList *u, *uids  = osync_hashtable_get_deleted(hashtable);
	OSyncChange *change = NULL;

	for (u=uids; u; u = u->next) {
		char *uid = (char *) u->data;
		osync_trace(TRACE_INTERNAL, "going to delete entry with uid: %s", uid);
	
		change = osync_change_new(&error);
		if (!change)
			goto error;

		osync_change_set_changetype(change, OSYNC_CHANGE_TYPE_DELETED);
		osync_change_set_uid(change, uid);

		OSyncData *data = osync_data_new(NULL, 0, objformat, &error);
		if (!data)
			goto error_free_change;

		osync_data_set_objtype(data, objtype);
		osync_change_set_data(change, data);
		osync_data_unref(data);

		osync_context_report_change(ctx, change);
		osync_hashtable_update_change(hashtable, change);

		osync_change_unref(change);
	}
	osync_list_free(uids);
	osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
	return true;

error_free_change:
	osync_change_unref(change);
error:

	osync_context_report_osyncerror(ctx, error);
	osync_trace(TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print(&error));
	osync_error_unref(&error);
	return false;
}

//--------------------------------------------------------------------------------

OSyncDataSource::~OSyncDataSource()
{
	if (hashtable)
		osync_hashtable_unref(hashtable);
}

//--------------------------------------------------------------------------------

bool OSyncDataSource::has_category(const QStringList &list) const
{
	if ( categories.isEmpty() ) return true;  // no filter defined -> match all

	for (QStringList::const_iterator it = list.begin(); it != list.end(); ++it ) {
		if ( categories.contains(*it) ) return true;
	}
	return false; // not found
}

//--------------------------------------------------------------------------------
