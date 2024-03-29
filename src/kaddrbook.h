/***********************************************************************
MultiSync Plugin for KDE 3.x
Copyright (C) 2004 Stewart Heitmann <sheitmann@users.sourceforge.net>
 
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

#ifndef KADDRBOOK_H
#define KADDRBOOK_H

#include <kabc/resource.h>

#include "datasource.h"

class KContactDataSource : public OSyncDataSource
{
	public:
		KContactDataSource() : OSyncDataSource("contact"), addressbookptr(0), modified(false), ticket(0) {};
		virtual ~KContactDataSource() {};

		virtual void connect(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx);
		virtual void disconnect(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx);
		virtual void get_changes(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, osync_bool slow_sync);
		virtual void commit(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, OSyncChange *chg);

	private:
		QString calc_hash(KABC::Addressee &e);

                KABC::AddressBook* addressbookptr;
                bool modified;  // set when needed to save addressbook back
                KABC::Ticket *ticket;
};

#endif

