/**
 * @author Andrew Baumann <andrewb@cse.unsw.edu.au>
 * changed to 0.40 API by Martin Koller <kollix@aon.at>
 * some additional 0.40 API updates by Chris Frey <cdfrey@foursquare.net>
 */

#include <stdlib.h>
#include <kapplication.h>
#include <qfile.h>

#include "datasource.h"

extern "C"
{

static void connect_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, void *userdata)
{
  osync_trace(TRACE_ENTRY, "%s(%p, %p, %p)", __PRETTY_FUNCTION__, sink, userdata, info, ctx);
  OSyncDataSource *obj = static_cast<OSyncDataSource *>(userdata);
  obj->connect(sink, info, ctx);
  osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

//--------------------------------------------------------------------------------

static void disconnect_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, void *userdata)
{
  osync_trace(TRACE_ENTRY, "%s(%p, %p, %p, %p)", __PRETTY_FUNCTION__, sink, userdata, info, ctx);
  OSyncDataSource *obj = static_cast<OSyncDataSource *>(userdata);
  obj->disconnect(sink, info, ctx);
  osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

//--------------------------------------------------------------------------------

static void get_changes_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info,
                                OSyncContext *ctx, osync_bool slow_sync,void *userdata)
{
  osync_trace(TRACE_ENTRY, "%s(%p, %p, %p, %p)", __PRETTY_FUNCTION__, sink, userdata, info, ctx);
  OSyncDataSource *obj = static_cast<OSyncDataSource *>(userdata);
  obj->get_changes(sink, info, ctx, slow_sync);
  osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

//--------------------------------------------------------------------------------

static void commit_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info,
                           OSyncContext *ctx, OSyncChange *chg, void *userdata)
{
  osync_trace(TRACE_ENTRY, "%s(%p, %p, %p, %p, %p)", __PRETTY_FUNCTION__, sink, userdata, info, ctx, chg);
  OSyncDataSource *obj = static_cast<OSyncDataSource *>(userdata);
  obj->commit(sink, info, ctx, chg);
  osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

//--------------------------------------------------------------------------------

static void sync_done_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, void *userdata)
{
  osync_trace(TRACE_ENTRY, "%s(%p, %p, %p, %p)", __PRETTY_FUNCTION__, sink, userdata, info, ctx);
  OSyncDataSource *obj = static_cast<OSyncDataSource *>(userdata);
  obj->sync_done(sink, info, ctx);
  osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

} // extern "C"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

bool OSyncDataSource::initialize(OSyncPlugin *plugin, OSyncPluginInfo *info, OSyncError **)
{
  osync_trace(TRACE_ENTRY, "%s(%p, %p)", __PRETTY_FUNCTION__, plugin, info);

  OSyncObjTypeSink *sink = osync_plugin_info_find_objtype(info, objtype);

  if ( !sink )
  {
    // this objtype is not enabled, but this is not an error
    osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
    return true;
  }

  osync_objtype_sink_set_connect_func(sink, connect_wrapper);
  osync_objtype_sink_set_disconnect_func(sink, disconnect_wrapper);
  osync_objtype_sink_set_get_changes_func(sink, get_changes_wrapper);
  osync_objtype_sink_set_commit_func(sink, commit_wrapper);
  osync_objtype_sink_set_sync_done_func(sink, sync_done_wrapper);

  osync_objtype_sink_set_userdata(sink, this);

  // Request a state_db from the framework.
  osync_objtype_sink_enable_state_db(sink, TRUE);

  // Request a hashtable from the framework
  osync_objtype_sink_enable_hashtable(sink, TRUE);

  // NOTE: advanced options are per plugin; currently we read the FilterCategory
  // for each Resource (later this could be separated by Resource via a different name, etc.)
  // read advanced options
  OSyncPluginConfig *config = osync_plugin_info_get_config(info);
  if ( config )
  {
    OSyncList *entry = osync_plugin_config_get_advancedoptions(config);
    for (; entry; entry = entry->next)
    {
      OSyncPluginAdvancedOption *option = static_cast<OSyncPluginAdvancedOption*>(entry->data);

      if ( strcmp(osync_plugin_advancedoption_get_name(option), "FilterCategory") == 0 )
        categories.append(QString::fromUtf8(osync_plugin_advancedoption_get_value(option)));
    }
  }

  osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
  return true;
}

//--------------------------------------------------------------------------------

void OSyncDataSource::connect(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx)
{
  osync_trace(TRACE_ENTRY, "%s(%p, %p)", __PRETTY_FUNCTION__, info, ctx);

  // Detection mechanismn if this is the first sync
  OSyncError *error = NULL;
  osync_bool statematch = FALSE;

  OSyncSinkStateDB *state_db = osync_objtype_sink_get_state_db(sink);

  if ( !osync_sink_state_equal(state_db, "done", "true", &statematch, &error) )
  {
    osync_context_report_osyncerror(ctx, error);
    osync_trace(TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print(&error));
    osync_error_unref(&error);
    return;
  }

  if ( !statematch )
  {
    osync_trace(TRACE_INTERNAL, "Setting slow-sync for %s", objtype);
    osync_context_report_slowsync(ctx);
  }
  osync_context_report_success(ctx);

  osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

//--------------------------------------------------------------------------------

void OSyncDataSource::sync_done(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx)
{
  osync_trace(TRACE_ENTRY, "%s(%p, %p)", __PRETTY_FUNCTION__, info, ctx);

  // Detection mechanismn if this is the first sync
  OSyncError *error = NULL;

  OSyncSinkStateDB *state_db = osync_objtype_sink_get_state_db(sink);

  if ( !osync_sink_state_set(state_db, "done", "true", &error) )
  {
    osync_context_report_osyncerror(ctx, error);
    osync_trace(TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print(&error));
    osync_error_unref(&error);
    return;
  }
  osync_context_report_success(ctx);

  osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

//--------------------------------------------------------------------------------

bool OSyncDataSource::report_change(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx,
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

  OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable(sink);
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

bool OSyncDataSource::report_deleted(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, OSyncObjFormat *objformat)
{
  osync_trace(TRACE_ENTRY, "%s(%p, %p, %p)", __PRETTY_FUNCTION__, info, ctx, objformat);

  OSyncError *error = NULL;
  OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable(sink);
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
