/***********************************************************************
KNotes support for OpenSync kdepim-sync plugin
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
/** @file
 *
 * @author Eduardo Pereira Habkost <ehabkost@conectiva.com.br>
 * @author Andrew Baumann <andrewb@cse.unsw.edu.au>
 * @author Martin Koller <kollix@aon.at>
 *
 * This module implements the access to the KDE 3.2 Notes, that are
 * stored on KGlobal::dirs()->saveLocation( "data" , "knotes/" ) + "notes.ics"
 *
 */

#include "knotes.h"
/*An adapted C++ implementation of RSA Data Securities MD5 algorithm.*/
#include <kmdcodec.h>

//--------------------------------------------------------------------------------

void KNotesDataSource::connect(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p)", __PRETTY_FUNCTION__, info, ctx);

	//connect to dcop
	kn_dcop = KApplication::kApplication()->dcopClient();
	if (!kn_dcop) {
		osync_context_report_error(ctx, OSYNC_ERROR_INITIALIZATION, "Unable to make new dcop for knotes");
		osync_trace(TRACE_EXIT_ERROR, "%s: Unable to make new dcop for knotes", __func__);
		return;
	}

	/*if (!kn_dcop->attach()) {
		osync_context_report_error(ctx, OSYNC_ERROR_INITIALIZATION, "Unable to attach dcop for knotes");
		osync_trace(TRACE_EXIT_ERROR, "%s: Unable to attach dcop for knotes", __func__);
		return FALSE;
	}*/

	QString appId = kn_dcop->registerAs("opensync");

	//check knotes running
	QCStringList apps = kn_dcop->registeredApplications();
	if (!apps.contains("knotes")) {
		//start knotes if not running
		knotesWasRunning = false;
		system("knotes");
		system("dcop knotes KNotesIface hideAllNotes");
	} else
		knotesWasRunning = true;

	kn_iface = new KNotesIface_stub("knotes", "KNotesIface");

	OSyncDataSource::connect(sink, info, ctx);
	
	osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

//--------------------------------------------------------------------------------

void KNotesDataSource::disconnect(OSyncObjTypeSink *, OSyncPluginInfo *, OSyncContext *ctx)
{
	osync_trace(TRACE_ENTRY, "%s(%p)", __func__, ctx);

	// FIXME: ugly, but necessary
	if (!knotesWasRunning) {
		system("dcop knotes MainApplication-Interface quit");
	}

	//detach dcop
	/*if (!kn_dcop->detach()) {
		osync_context_report_error(ctx, OSYNC_ERROR_INITIALIZATION, "Unable to detach dcop for knotes");
		osync_trace(TRACE_EXIT_ERROR, "%s: Unable to detach dcop for knotes", __func__);
		return FALSE;
	}*/
	//destroy dcop
	delete kn_iface;
	kn_iface = NULL;
	//delete kn_dcop;
	//kn_dcop = NULL;

	osync_context_report_success(ctx);
	osync_trace(TRACE_EXIT, "%s", __func__);
}

//--------------------------------------------------------------------------------

static QString strip_html(QString input)
{
	osync_trace(TRACE_SENSITIVE, "input is %s\n", (const char*)input.local8Bit());
	QString output = NULL;
	unsigned int i = 0;
	int inbraces = 0;
	for (i = 0; i < input.length(); i++) {
		QCharRef cur = input[i];
		if (cur == '<')
			inbraces = 1;
		if (cur == '>') {
			inbraces = 0;
			continue;
		}
		if (!inbraces)
			output += input[i];
	}
	osync_trace(TRACE_SENSITIVE, "output is %s\n", (const char*)output.stripWhiteSpace().local8Bit());
	return output.stripWhiteSpace();
}

//--------------------------------------------------------------------------------

void KNotesDataSource::get_changes(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, osync_bool slow_sync)
{
	osync_trace(TRACE_ENTRY, "%s(%p)", __func__, ctx);
	QMap <KNoteID_t,QString> fNotes;
	KMD5 hash_value;
	OSyncError *error = NULL;

	fNotes = kn_iface->notes();
	if (kn_iface->status() != DCOPStub::CallSucceeded) {
		osync_context_report_error(ctx, OSYNC_ERROR_GENERIC, "Unable to get changed notes");
		osync_trace(TRACE_EXIT_ERROR, "%s: Unable to get changed notes", __func__);
		return;
	}

	OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable(sink);
	if (slow_sync) {
		osync_trace(TRACE_INTERNAL, "Got slow-sync, resetting hashtable");
		if (!osync_hashtable_slowsync(hashtable, &error)) {
			osync_context_report_osyncerror(ctx, error);
			osync_trace(TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print(&error));
			return;
		}
	}

	OSyncFormatEnv *formatenv = osync_plugin_info_get_format_env(info);
	OSyncObjFormat *objformat = osync_format_env_find_objformat(formatenv, "memo");

	QMap<KNoteID_t,QString>::ConstIterator i;
	for (i = fNotes.begin(); i != fNotes.end(); i++) {
		osync_trace(TRACE_INTERNAL, "reporting notes %s\n", static_cast<const char*>(i.key().utf8()));

		QString uid = i.key();
		QString data = i.data() + '\n' + strip_html(kn_iface->text(i.key()));
		hash_value.update(data.utf8());
		QString hash = hash_value.base64Digest();

		if ( !report_change(sink, info, ctx, uid, data, hash, objformat) ) {
			osync_context_report_error(ctx, OSYNC_ERROR_GENERIC, "Failed to get changes");
			osync_trace(TRACE_EXIT_ERROR, "%s", __PRETTY_FUNCTION__);
			return;
		}

		hash_value.reset();
	}

	if (!report_deleted(sink, info, ctx, objformat)) {
		osync_context_report_error(ctx, OSYNC_ERROR_GENERIC, "Failed detecting deleted changes.");
		osync_trace(TRACE_EXIT_ERROR, "%s", __func__);
		return;
	}

	osync_context_report_success(ctx);
	osync_trace(TRACE_EXIT, "%s", __func__);
}

//--------------------------------------------------------------------------------

void KNotesDataSource::commit(OSyncObjTypeSink *sink, OSyncPluginInfo *, OSyncContext *ctx, OSyncChange *chg)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p)", __func__, ctx, chg);
	OSyncChangeType type = osync_change_get_changetype(chg);

	OSyncData *odata = osync_change_get_data(chg);

	char *cdata;
	unsigned int data_size = 0;
	osync_data_get_data(odata, &cdata, &data_size);

	QString uid = QString::fromUtf8(osync_change_get_uid(chg));

	KMD5 hash_value;

	if (type != OSYNC_CHANGE_TYPE_DELETED) {
                QString data = QString::fromUtf8(cdata);
                QString summary = data.section('\n', 0, 0);  // first line
                QString body = data.section('\n', 1);  // rest

		QString hash;
		switch (type) {
			case OSYNC_CHANGE_TYPE_ADDED: {
				uid = kn_iface->newNote(summary, body);
				if (kn_iface->status() != DCOPStub::CallSucceeded) {
					osync_context_report_error(ctx, OSYNC_ERROR_GENERIC, "Unable to add new note");
					osync_trace(TRACE_EXIT_ERROR, "%s: Unable to add new note", __func__);
					return;
				}

				kn_iface->hideNote(uid);
				if (kn_iface->status() != DCOPStub::CallSucceeded)
					osync_trace(TRACE_INTERNAL, "ERROR: Unable to hide note");
				hash_value.update(data);
				hash = hash_value.base64Digest();
				osync_change_set_uid(chg, uid);
				osync_change_set_hash(chg, hash);
				break;
			}
			case OSYNC_CHANGE_TYPE_MODIFIED: {
				kn_iface->setName(uid, summary);
				if (kn_iface->status() != DCOPStub::CallSucceeded) {
					osync_context_report_error(ctx, OSYNC_ERROR_GENERIC, "Unable to set name");
					osync_trace(TRACE_EXIT_ERROR, "%s: Unable to set name", __func__);
					return;
				}

				kn_iface->setText(uid, body);
				if (kn_iface->status() != DCOPStub::CallSucceeded) {
					osync_context_report_error(ctx, OSYNC_ERROR_GENERIC, "Unable to set text");
					osync_trace(TRACE_EXIT_ERROR, "%s: Unable to set text", __func__);
					return;
				}
				hash_value.update(data);
				hash = hash_value.base64Digest();
				osync_change_set_hash(chg, hash);
				break;
			}
			default: {
				osync_context_report_error(ctx, OSYNC_ERROR_NOT_SUPPORTED, "Invalid change type");
				osync_trace(TRACE_EXIT_ERROR, "%s: Invalid change type", __func__);
				return;
			}
		}
	} else {
		system("dcop knotes KNotesIface hideAllNotes");
		QString asdasd = "dcop knotes KNotesIface killNote " + uid + " true";
		system((const char*)asdasd.local8Bit());
		/*kn_iface->killNote(uid, true);
		if (kn_iface->status() != DCOPStub::CallSucceeded) {
			osync_context_report_error(ctx, OSYNC_ERROR_GENERIC, "Unable to delete note");
			osync_trace(TRACE_EXIT_ERROR, "%s: Unable to delete note", __func__);
			return false;
		}*/
	}

	OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable(sink);
	osync_hashtable_update_change(hashtable, chg);
	osync_context_report_success(ctx);
	osync_trace(TRACE_EXIT, "%s", __func__);
}

//--------------------------------------------------------------------------------
