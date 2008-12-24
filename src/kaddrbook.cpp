/***********************************************************************
KAddressbook support for OpenSync kdepim-sync plugin
Copyright (C) 2004 Conectiva S. A.
Copyright (C) 2005 Armin Bauer

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
 * @author Eduardo Pereira Habkost <ehabkost@conectiva.com.br>
 * @author Armin Bauer <armin.bauer@opensync.org>
 * @author Andrew Baumann <andrewb@cse.unsw.edu.au>
 */

#include "kaddrbook.h"
#include <kapplication.h>
#include <kabc/vcardconverter.h>
#include <kabc/stdaddressbook.h>
#include <dcopclient.h>

/** Calculate the hash value for an Addressee.
 * Should be called before returning/writing the
 * data, because the revision of the Addressee
 * can be changed.
 */
QString KContactDataSource::calc_hash(KABC::Addressee &e)
{
	//Get the revision date of the KDE addressbook entry.
	QDateTime revdate = e.revision();
	if ( !revdate.isValid() ) {
                // if no revision is available, always return the same 0-time stamp
          	// to avoid that 2 calls deliver different times which would be treated as changed entry
		revdate.setTime_t(0);
		e.setRevision(revdate);
	}

	return revdate.toString(Qt::ISODate);
}

//--------------------------------------------------------------------------------

void KContactDataSource::connect(OSyncPluginInfo *info, OSyncContext *ctx)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p)", __PRETTY_FUNCTION__, info, ctx);

	// get a handle to the standard KDE addressbook
	addressbookptr = KABC::StdAddressBook::self(false);  // load synchronously
	KABC::StdAddressBook::setAutomaticSave(false);  // only when modified
	modified = false;

	ticket = addressbookptr->requestSaveTicket();
	if ( !ticket ) {
		osync_context_report_error(ctx, OSYNC_ERROR_NOT_SUPPORTED, "Unable to get save ticket");
		osync_trace(TRACE_EXIT_ERROR, "%s: Unable to get save ticket", __PRETTY_FUNCTION__);
		return;
	}

	OSyncDataSource::connect(info, ctx);

	osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

//--------------------------------------------------------------------------------

void KContactDataSource::disconnect(OSyncPluginInfo *info, OSyncContext *ctx)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p)", __PRETTY_FUNCTION__, info, ctx);

	if ( modified ) {
		if ( !addressbookptr->save(ticket) ) {
			osync_context_report_error(ctx, OSYNC_ERROR_NOT_SUPPORTED, "Unable to use ticket");
			osync_trace(TRACE_EXIT_ERROR, "%s: Unable to save", __PRETTY_FUNCTION__);
			return;
		}
	}
	else {
		addressbookptr->releaseSaveTicket(ticket);
	}

	ticket = 0;

	osync_context_report_success(ctx);
	osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
	return;
}

//--------------------------------------------------------------------------------

void KContactDataSource::get_changes(OSyncPluginInfo *info, OSyncContext *ctx)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p)", __PRETTY_FUNCTION__, info, ctx);

	OSyncError *error = NULL;

	OSyncObjTypeSink *sink = osync_plugin_info_find_objtype(info, objtype);
	if (osync_objtype_sink_get_slowsync(sink)) {
		osync_trace(TRACE_INTERNAL, "Got slow-sync, resetting hashtable");
		if (!osync_hashtable_slowsync(hashtable, &error)) {
			osync_context_report_osyncerror(ctx, error);
			osync_trace(TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print(&error));
			return;

		}
	}

	OSyncFormatEnv *formatenv = osync_plugin_info_get_format_env(info);
	OSyncObjFormat *objformat = osync_format_env_find_objformat(formatenv, "vcard30");

	KABC::VCardConverter converter;
	for (KABC::AddressBook::Iterator it=addressbookptr->begin(); it!=addressbookptr->end(); it++ ) {

		if ( ! has_category((*it).categories()) )
			continue;

		// Convert the VCARD data into a string
		// only vcard3.0 exports Categories
		QString data = converter.createVCard(*it, KABC::VCardConverter::v3_0);

		if (!report_change(info, ctx, it->uid(), data, calc_hash(*it), objformat)) {

			osync_context_report_error(ctx, OSYNC_ERROR_GENERIC, "Failed to get changes");
			osync_trace(TRACE_EXIT_ERROR, "%s", __PRETTY_FUNCTION__);
			return;
		}
	}

	if (!report_deleted(info, ctx, objformat)) {
		osync_context_report_error(ctx, OSYNC_ERROR_GENERIC, "Failed detecting deleted changes.");
		osync_trace(TRACE_EXIT_ERROR, "%s", __PRETTY_FUNCTION__);
		return;
	}

	osync_context_report_success(ctx);
	osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

//--------------------------------------------------------------------------------

void KContactDataSource::commit(OSyncPluginInfo *, OSyncContext *ctx, OSyncChange *chg)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p)", __PRETTY_FUNCTION__, ctx, chg);
	KABC::VCardConverter converter;

	// convert VCARD string from obj->comp into an Addresse object.
	OSyncData *odata = osync_change_get_data(chg);

	char *data;
	//size_t data_size;
	// osync_data_get_data requires an unsigned int which is not compatible with size_t on 64bit machines
	unsigned int data_size = 0;

	osync_data_get_data(odata, &data, &data_size);

	QString uid = QString::fromUtf8(osync_change_get_uid(chg));

	switch ( osync_change_get_changetype(chg) )
        {
		case OSYNC_CHANGE_TYPE_ADDED:
		case OSYNC_CHANGE_TYPE_MODIFIED: {
			KABC::Addressee addressee = converter.parseVCard(QString::fromUtf8(data));

			// if we run with a configured category filter, but the received added vcard does
			// not contain that category, add the filter-categories so that the address will be
			// found again on the next sync
			if ( ! has_category(addressee.categories()) ) {
				for (QStringList::const_iterator it = categories.begin(); it != categories.end(); ++it )
					addressee.insertCategory(*it);
			}

			// ensure it has the correct UID
			addressee.setUid(uid);

			// replace the current addressbook entry (if any) with the new one
                        // this changes the revision inside the KDE-addressbook
			addressbookptr->insertAddressee(addressee);

			modified = true;
			osync_trace(TRACE_INTERNAL, "KDE ADDRESSBOOK ENTRY UPDATED (UID=%s)", (const char *)uid.utf8());

                        // read out the set addressee to get the new revision
			KABC::Addressee addresseeNew = addressbookptr->findByUid(uid);

			QString hash = calc_hash(addresseeNew);
			osync_change_set_hash(chg, hash.utf8());
			break;
		}
		case OSYNC_CHANGE_TYPE_DELETED: {
			if (uid.isEmpty()) {
				osync_context_report_error(ctx, OSYNC_ERROR_FILE_NOT_FOUND, "Trying to delete entry with empty UID");
				osync_trace(TRACE_EXIT_ERROR, "%s: Trying to delete but uid is empty", __PRETTY_FUNCTION__);
				return;
			}

			//find addressbook entry with matching UID and delete it
			KABC::Addressee addressee = addressbookptr->findByUid(uid);
			if(!addressee.isEmpty()) {
				addressbookptr->removeAddressee(addressee);
				modified = true;
				osync_trace(TRACE_INTERNAL, "KDE ADDRESSBOOK ENTRY DELETED (UID=%s)", (const char*)uid.utf8());
			}

			break;
		}
		default: {
			osync_context_report_error(ctx, OSYNC_ERROR_NOT_SUPPORTED, "Operation not supported");
			osync_trace(TRACE_EXIT_ERROR, "%s: Operation not supported", __PRETTY_FUNCTION__);
			return;
		}
	}

	osync_hashtable_update_change(hashtable, chg);

	osync_context_report_success(ctx);
	osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}

//--------------------------------------------------------------------------------
