/***********************************************************************
KCalendar OSyncDataSource class
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
 * @author Eduardo Pereira Habkost <ehabkost@conectiva.com.br>
 * @author Andrew Baumann <andrewb@cse.unsw.edu.au>
 */

#include <libkcal/calendarresources.h>
#include <libkcal/incidence.h>

#include "datasource.h"

class KCalSharedResource
{
	public:
		KCalSharedResource() : calendar(0), refcount(0) { }
		bool open(OSyncContext *ctx);
		bool close(OSyncContext *ctx);
		bool get_event_changes(OSyncDataSource *dsobj, OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx);
		bool get_todo_changes(OSyncDataSource *dsobj, OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx);
		bool commit(OSyncDataSource *dsobj, OSyncContext *ctx, OSyncChange *chg);

	private:
		KCal::CalendarResources *calendar;
		int refcount;

		bool report_incidence(OSyncDataSource *dsobj, OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx,
                                      KCal::Incidence *e, OSyncObjFormat *objformat);
};

//--------------------------------------------------------------------------------

class KCalEventDataSource : public OSyncDataSource
{
	public:
		KCalEventDataSource(KCalSharedResource *kcal) : OSyncDataSource("event"), kcal(kcal) {};
		virtual ~KCalEventDataSource() {};

		virtual void connect(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx);
		virtual void disconnect(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx);
		virtual void get_changes(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, osync_bool slow_sync);
		virtual void commit(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, OSyncChange *chg);

	private:
		KCalSharedResource *kcal;
};

//--------------------------------------------------------------------------------

class KCalTodoDataSource : public OSyncDataSource
{
	public:
		KCalTodoDataSource(KCalSharedResource *kcal) : OSyncDataSource("todo"), kcal(kcal) {};
		virtual ~KCalTodoDataSource() {};

		virtual void connect(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx);
		virtual void disconnect(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx);
		virtual void get_changes(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, osync_bool slow_sync);
		virtual void commit(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, OSyncChange *chg);

	private:
		KCalSharedResource *kcal;
};

//--------------------------------------------------------------------------------
