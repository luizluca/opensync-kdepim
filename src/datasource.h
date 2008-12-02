#ifndef KDEPIM_OSYNC_DATASOURCE_H
#define KDEPIM_OSYNC_DATASOURCE_H

#include <qstringlist.h>
#include <opensync/opensync.h>
#include <opensync/opensync-plugin.h>
#include <opensync/opensync-context.h>
#include <opensync/opensync-data.h>
#include <opensync/opensync-helper.h>
#include <opensync/opensync-format.h>
#include <opensync/opensync-merger.h>

/* common parent class and shared code for all KDE Data sources/sinks */
class OSyncDataSource
{
	friend class KCalSharedResource;

	public:
		OSyncDataSource(const char *objtype) : objtype(objtype), hashtable(0) {}
		virtual ~OSyncDataSource();

                const char *getObjType() const { return objtype; }
		
		/* common code for some of the callbacks, should be used by a subclass */
		virtual bool initialize(OSyncPlugin *plugin, OSyncPluginInfo *info, OSyncError **error);

		/* these map to opensync's per-sink callbacks */
		virtual void connect(OSyncPluginInfo *info, OSyncContext *ctx);
		virtual void disconnect(OSyncPluginInfo *info, OSyncContext *ctx) = 0;
		virtual void get_changes(OSyncPluginInfo *info, OSyncContext *ctx) = 0;
		virtual void commit(OSyncPluginInfo *info, OSyncContext *ctx, OSyncChange *chg) = 0;
		virtual void sync_done(OSyncPluginInfo *info, OSyncContext *ctx);

	protected:
		const char *objtype;
		OSyncHashTable *hashtable;
		QStringList categories;

		/* utility functions for subclasses */
		bool report_change(OSyncPluginInfo *info, OSyncContext *ctx, QString uid, QString data, QString hash, OSyncObjFormat *objformat);
		bool report_deleted(OSyncPluginInfo *info, OSyncContext *ctx, OSyncObjFormat *objformat);

		// return true if at least one item in the given list is included in the categories member
		bool has_category(const QStringList &list) const;
};

#endif // KDEPIM_OSYNC_DATASOURCE_H
