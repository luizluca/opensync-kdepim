LINK_DIRECTORIES( ${OPENSYNC_LIBRARY_DIRS} )
INCLUDE_DIRECTORIES( ${CMAKE_CURRENT_BINARY_DIR} ${OPENSYNC_INCLUDE_DIRS} ${KDE3_INCLUDE_DIR} ${KDEPIM3_INCLUDE_DIR} ${QT_INCLUDE_DIR} )

# kdepim-sync sources
SET( kdepim_sync_LIB_SRCS
kdepim_sync.cpp
datasource.cpp
kaddrbook.cpp
kcal.cpp
knotes.cpp
)

ADD_DEFINITIONS( -DKDEPIM_LIBDIR="${OPENSYNC_PLUGINDIR}" )

KDE3_ADD_DCOP_STUBS( kdepim_sync_LIB_SRCS KNotesIface.h )

OPENSYNC_PLUGIN_ADD( kdepim-sync ${kdepim_sync_LIB_SRCS} )

TARGET_LINK_LIBRARIES( kdepim-sync ${OPENSYNC_LIBRARIES} ${KDE3_LIBRARIES} ${KDEPIM3_KABC_LIBRARIES} ${QT_LIBRARIES} ${KDEPIM3_KCAL_LIBRARIES} )

# Install plugins
OPENSYNC_PLUGIN_INSTALL( kdepim-sync )

# Install config template
OPENSYNC_PLUGIN_CONFIG( kdepim-sync )

# install capabilities file
OPENSYNC_PLUGIN_CAPABILITIES( kdepim-sync-capabilities.xml )

# install description file
OPENSYNC_PLUGIN_DESCRIPTIONS( kdepim-sync-description.xml )
