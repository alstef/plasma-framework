# find_package(KActivities QUIET CONFIG)
# set_package_properties(KActivities PROPERTIES DESCRIPTION "Library and infrastructure for using Activities from applications"
#                        URL "https://projects.kde.org/kactivities"
#                        TYPE REQUIRED
#                        PURPOSE "Provides the ability to access activities from QML."
#                       )

set(plasmaextracomponents_SRCS
    appbackgroundprovider.cpp
    #resourceinstance.cpp
    plasmaextracomponentsplugin.cpp
    fallbackcomponent.cpp
    )

add_library(plasmaextracomponentsplugin SHARED ${plasmaextracomponents_SRCS})

target_link_libraries(plasmaextracomponentsplugin
        Qt5::Quick
        Qt5::Qml
        ${KACTIVITIES_LIBRARY}
        KF5::Service
        KF5::Plasma)


install(TARGETS plasmaextracomponentsplugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/plasma/extras)

install(DIRECTORY qml/ DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/plasma/extras)


# The platform specific stuff, overwrites a copy of the desktop one
# it does install some files on top of the old ones, has to be done
# file by file since if some component from the generic set is more
# recent than the specifc ones, it wouldn't be overwritten

#install(TARGETS plasmaextracomponentsplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/platformimports/touch/org/kde/plasma/extras)
#install(FILES qml/qmldir DESTINATION ${KDE_INSTALL_PLUGINDIR}/platformimports/touch/org/kde/plasma/extras)

# install(FILES qml/App.qml DESTINATION ${KDE_INSTALL_PLUGINDIR}/platformimports/touch/org/kde/plasma/extras)
# install(FILES qml/Heading.qml DESTINATION ${KDE_INSTALL_PLUGINDIR}/platformimports/touch/org/kde/plasma/extras)
# install(FILES qml/Paragraph.qml DESTINATION ${KDE_INSTALL_PLUGINDIR}/platformimports/touch/org/kde/plasma/extras)
# install(FILES qml/Title.qml DESTINATION ${KDE_INSTALL_PLUGINDIR}/platformimports/touch/org/kde/plasma/extras)
# install(DIRECTORY qml/animations/ DESTINATION ${KDE_INSTALL_PLUGINDIR}/platformimports/touch/org/kde/plasma/extras)

#install(DIRECTORY qml/animations/ DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/plasma/extras)

#install platform overrides -- enable as we use this feature for plasmaextras
#install(DIRECTORY platformcomponents/touch/ DESTINATION ${KDE_INSTALL_PLUGINDIR}/platformimports/touch/org/kde/plasma/extras)
