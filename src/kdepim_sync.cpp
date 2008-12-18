/***********************************************************************
Actual implementation of the KDE PIM OpenSync plugin
Copyright (C) 2004 Conectiva S. A.
 
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation;
 
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) AND AUTHOR(S) BE LIABLE FOR ANY
CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES 
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN 
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF 
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 
ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PATENTS, 
COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS, RELATING TO USE OF THIS 
SOFTWARE IS DISCLAIMED.
*************************************************************************/
/**
 * @autor Eduardo Pereira Habkost <ehabkost@conectiva.com.br>
 * edit Matthias Jahn <jahn.matthias@freenet.de>
 * changed to 0.40 API by Martin Koller <m.koller@surfeu.at>
 */

#include <libkcal/resourcecalendar.h>
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>

#include "kaddrbook.h"
#include "kcal.h"
#include "knotes.h"

#include <string.h>

#include <opensync/opensync-plugin.h>
#include <opensync/opensync-version.h>

class KdePluginImplementation
{
	public:
		KdePluginImplementation() : application(0), newApplication(false)
		{
			KAboutData aboutData(
			    "libopensync-kdepim-plugin",         // internal program name
			    "OpenSync-KDE3-plugin",              // displayable program name.
			    "0.4",                               // version string
			    "OpenSync KDEPIM plugin",            // short porgram description
			    KAboutData::License_GPL,             // license type
			    "(c) 2005, Eduardo Pereira Habkost," // copyright statement
			    "(c) 2008, Martin Koller",           // copyright statement
			    0,                                   // any free form text
			    "http://www.opensync.org",           // program home page address
			    "http://www.opensync.org/newticket"  // bug report email address
			);

			KCmdLineArgs::init( &aboutData );
			if ( kapp ) {
				application = kapp;
				newApplication = false;
			} else {
				application = new KApplication( true, true );
				newApplication = true;
			}

			kaddrbook  = new KContactDataSource();
			kcal_event = new KCalEventDataSource(&kcal);
			kcal_todo  = new KCalTodoDataSource(&kcal);
			knotes     = new KNotesDataSource();
		}

		bool initialize(OSyncPlugin *plugin, OSyncPluginInfo *info, OSyncError **error)
		{
			osync_trace(TRACE_ENTRY, "%s(%p, %p)", __PRETTY_FUNCTION__, plugin, info);

			if (!kaddrbook->initialize(plugin, info, error))
				goto error;

			if (!kcal_event->initialize(plugin, info, error))
				goto error;

			if (!kcal_todo->initialize(plugin, info, error))
				goto error;

			if (!knotes->initialize(plugin, info, error))
				goto error;

      /*
      // get the config
      OSyncPluginConfig *config = osync_plugin_info_get_config(info);
      if (!config) {
        osync_error_set(error, OSYNC_ERROR_GENERIC, "Unable to get config.");
        goto error;
      }

      // Process plugin specific advanced options 
      OSyncList *optslist = osync_plugin_config_get_advancedoptions(config);
      for (; optslist; optslist = optslist->next) {
        OSyncPluginAdvancedOption *option = optslist->data;

        const char *val = osync_plugin_advancedoption_get_value(option);
        const char *name = osync_plugin_advancedoption_get_name(option);

        if (!strcmp(name,"category")) {
          }
      }

          */


			osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
			return true;

		error:
			osync_trace(TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print(error));
			return false;
		}

		virtual ~KdePluginImplementation()
		{
			delete kaddrbook;
			delete kcal_event;
			delete kcal_todo;
			delete knotes;

			if ( newApplication ) {
				delete application;
				application = 0;
			}
		}
	private:
		KContactDataSource *kaddrbook;
		KCalSharedResource kcal;
		KCalEventDataSource *kcal_event;
		KCalTodoDataSource *kcal_todo;
		KNotesDataSource *knotes;

		KApplication *application;
		bool newApplication;
};

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

extern "C"
{

// create actual plugin implementation
void *kde_initialize(OSyncPlugin *plugin, OSyncPluginInfo *info, OSyncError **error)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p, %p)", __func__, plugin, info, error);

	KdePluginImplementation *impl_object = new KdePluginImplementation;

	if ( !impl_object->initialize(plugin, info, error) )
		return 0;

	/* Return the created object to the sync engine */
	osync_trace(TRACE_EXIT, "%s: %p", __func__, impl_object);
	return impl_object;
}

//--------------------------------------------------------------------------------
/* Here we actually tell opensync which sinks are available. For this plugin, we
 * go through and enable all the sinks */

osync_bool kde_discover(void *userdata, OSyncPluginInfo *info, OSyncError **error)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p, %p)", __func__, userdata, info, error);

	int num_objtypes = osync_plugin_info_num_objtypes(info);
	for (int n = 0; n < num_objtypes; n++)
		osync_objtype_sink_set_available(osync_plugin_info_nth_objtype(info, n), TRUE);

        // set information about the peer (KDE itself)
        {
          OSyncVersion *version = osync_version_new(error);
          osync_version_set_plugin(version, "kdepim-sync");

          // this is the KDE version the plugin was implemented for and there will be
          // no other 3.x version as KDE4 already exists
          osync_version_set_softwareversion(version, "3.5");
          osync_plugin_info_set_version(info, version);
          osync_version_unref(version);
        }

	osync_trace(TRACE_EXIT, "%s", __func__);
	return TRUE;
}

//--------------------------------------------------------------------------------

void kde_finalize(void *userdata)
{
	osync_trace(TRACE_ENTRY, "%s(%p)", __func__, userdata);
	KdePluginImplementation *impl_object = (KdePluginImplementation *)userdata;
	delete impl_object;
	osync_trace(TRACE_EXIT, "%s", __func__);
}

//--------------------------------------------------------------------------------

osync_bool get_sync_info(OSyncPluginEnv *env, OSyncError **error)
{
	osync_trace(TRACE_ENTRY, "%s(%p)", __func__, env);

	OSyncPlugin *plugin = osync_plugin_new(error);
	if (!plugin)
		goto error;

	osync_plugin_set_name(plugin, "kdepim-sync");
	osync_plugin_set_longname(plugin, "KDE3 PIM Synchronization");
	osync_plugin_set_description(plugin,
            "Synchronization with the KDE 3.5 Personal Information Management (PIM) suite");
	osync_plugin_set_config_type(plugin, OSYNC_PLUGIN_OPTIONAL_CONFIGURATION);
	osync_plugin_set_start_type(plugin, OSYNC_START_TYPE_PROCESS);

	osync_plugin_set_initialize(plugin, kde_initialize);
	osync_plugin_set_finalize(plugin, kde_finalize);
	osync_plugin_set_discover(plugin, kde_discover);

	osync_plugin_env_register_plugin(env, plugin);
	osync_plugin_unref(plugin);

	osync_trace(TRACE_EXIT, "%s", __func__);
	return TRUE;

error:
	osync_trace(TRACE_EXIT_ERROR, "%s: Unable to register: %s", __func__, osync_error_print(error));
	return FALSE;
}

//--------------------------------------------------------------------------------

int get_version(void)
{
	return 1;
}

//--------------------------------------------------------------------------------

}// extern "C"
