/*
 *   Copyright 2005 by Aaron Seigo <aseigo@kde.org>
 *   Copyright 2007 by Riccardo Iaconelli <riccardo@kde.org>
 *   Copyright 2008 by Ménard Alexis <darktears31@gmail.com>
 *   Copyright (c) 2009 Chani Armitage <chani@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "applet.h"
#include "private/applet_p.h"

#include "config-plasma.h"

#include <cmath>
#include <limits>

#include <QFile>
#include <QList>
#include <QUiLoader>

#include <kaction.h>
#include <kactioncollection.h>
#include <kcoreauthorized.h>
#include <kcolorscheme.h>
#include <kdesktopfile.h>
#include <kplugininfo.h>
#include <klocalizedstring.h>
#include <kservice.h>
#include <kservicetypetrader.h>
#include <kshortcut.h>
#include <kwindowsystem.h>

#if !PLASMA_NO_KUTILS
#include <kcmoduleinfo.h>
#include <kcmoduleproxy.h>
#else
#include <kcmodule.h>
#endif

#include "configloader.h"
#include "containment.h"
#include "corona.h"
#include "package.h"
#include "plasma.h"
#include "scripting/appletscript.h"
#include "paintutils.h"
#include "abstractdialogmanager.h"
#include "pluginloader.h"

#include "private/associatedapplicationmanager_p.h"
#include "private/containment_p.h"


namespace Plasma
{

Applet::Applet(const KPluginInfo &info, QObject *parent, uint appletId)
    :  QObject(parent),
       d(new AppletPrivate(KService::Ptr(), &info, appletId, this))
{
    // WARNING: do not access config() OR globalConfig() in this method!
    //          that requires a scene, which is not available at this point
    d->init();
    d->setupPackage();
}

Applet::Applet(QObject *parent, const QString &serviceID, uint appletId)
    :  QObject(parent),
       d(new AppletPrivate(KService::serviceByStorageId(serviceID), 0, appletId, this))
{
    // WARNING: do not access config() OR globalConfig() in this method!
    //          that requires a scene, which is not available at this point
    d->init();
    d->setupPackage();
}

Applet::Applet(QObject *parentObject, const QVariantList &args)
    :  QObject(0),
       d(new AppletPrivate(
             KService::serviceByStorageId(args.count() > 0 ? args[0].toString() : QString()), 0,
             args.count() > 1 ? args[1].toInt() : 0, this))
{
    setParent(parentObject);

    // WARNING: do not access config() OR globalConfig() in this method!
    //          that requires a scene, which is not available at this point
    d->init();
    d->setupPackage();
}

Applet::Applet(const QString &packagePath, uint appletId)
    : QObject(0),
      d(new AppletPrivate(KService::Ptr(new KService(packagePath + "/metadata.desktop")), 0, appletId, this))
{
    d->init(packagePath);
    d->setupPackage();
}

Applet::~Applet()
{
    //let people know that i will die
    emit appletDeleted(this);

    // clean up our config dialog, if any
    delete KConfigDialog::exists(d->configDialogId());
    delete d;
}

void Applet::init()
{
    if (d->script) {
        if (!d->script->init() && !d->failed) {
            setLaunchErrorMessage(i18n("Script initialization failed"));
        }
    }
}

uint Applet::id() const
{
    return d->appletId;
}

void Applet::save(KConfigGroup &g) const
{
    if (d->transient || !d->appletDescription.isValid()) {
        return;
    }

    KConfigGroup group = g;
    if (!group.isValid()) {
        group = *d->mainConfigGroup();
    }

    //kDebug() << "saving" << pluginName() << "to" << group.name();
    // we call the dptr member directly for locked since isImmutable()
    // also checks kiosk and parent containers
    group.writeEntry("immutability", (int)d->immutability);
    group.writeEntry("plugin", d->appletDescription.pluginName());

    if (!d->started) {
        return;
    }

    KConfigGroup appletConfigGroup(&group, "Configuration");
    saveState(appletConfigGroup);

    if (d->configLoader) {
        // we're saving so we know its changed, we don't need or want the configChanged
        // signal bubbling up at this point due to that
        disconnect(d->configLoader, SIGNAL(configChanged()), this, SLOT(propagateConfigChanged()));
        d->configLoader->writeConfig();
        connect(d->configLoader, SIGNAL(configChanged()), this, SLOT(propagateConfigChanged()));
    }
}

void Applet::restore(KConfigGroup &group)
{

    setImmutability((ImmutabilityType)group.readEntry("immutability", (int)Mutable));

    KConfigGroup shortcutConfig(&group, "Shortcuts");
    QString shortcutText = shortcutConfig.readEntryUntranslated("global", QString());
    if (!shortcutText.isEmpty()) {
        setGlobalShortcut(KShortcut(shortcutText));
        /*
#ifndef NDEBUG
        kDebug() << "got global shortcut for" << name() << "of" << QKeySequence(shortcutText);
#endif
#ifndef NDEBUG
        kDebug() << "set to" << d->activationAction->objectName()
#endif
                 << d->activationAction->globalShortcut().primary();
                 */
    }

    // local shortcut, if any
    //TODO: implement; the shortcut will need to be registered with the containment
    /*
#include "accessmanager.h"
#include "private/plasmoidservice_p.h"
#include "authorizationmanager.h"
#include "authorizationmanager.h"
    shortcutText = shortcutConfig.readEntryUntranslated("local", QString());
    if (!shortcutText.isEmpty()) {
        //TODO: implement; the shortcut
    }
    */
}

void Applet::setLaunchErrorMessage(const QString &message)
{
    if (message == d->launchErrorMessage) {
        return;
    }

    d->failed = true;
    d->launchErrorMessage = message;
}

void Applet::saveState(KConfigGroup &group) const
{
    if (d->script) {
        emit d->script->saveState(group);
    }

    if (group.config()->name() != config().config()->name()) {
        // we're being saved to a different file!
        // let's just copy the current values in our configuration over
        KConfigGroup c = config();
        c.copyTo(&group);
    }
}

KConfigGroup Applet::config() const
{
    if (d->transient) {
        return KConfigGroup(KSharedConfig::openConfig(), "PlasmaTransientsConfig");
    }

    if (d->isContainment) {
        return *(d->mainConfigGroup());
    }

    return KConfigGroup(d->mainConfigGroup(), "Configuration");
}

KConfigGroup Applet::globalConfig() const
{
    KConfigGroup globalAppletConfig;
    QString group = isContainment() ? "ContainmentGlobals" : "AppletGlobals";

    Containment *cont = containment();
    Corona *corona = 0;
    if (cont) {
        corona = cont->corona();
    }
    if (corona) {
        KSharedConfig::Ptr coronaConfig = corona->config();
        globalAppletConfig = KConfigGroup(coronaConfig, group);
    } else {
        globalAppletConfig = KConfigGroup(KSharedConfig::openConfig(), group);
    }

    return KConfigGroup(&globalAppletConfig, d->globalName());
}

void Applet::destroy()
{
    if (immutability() != Mutable || d->transient || !d->started) {
        return; //don't double delete
    }

    d->transient = true;
    //FIXME: an animation on leave if !isContainment() would be good again .. which should be handled by the containment class
    d->cleanUpAndDelete();
}

bool Applet::destroyed() const
{
    return d->transient;
}

ConfigLoader *Applet::configScheme() const
{
    return d->configLoader;
}

Package Applet::package() const
{
    return d->package ? *d->package : Package();
}

void Applet::updateConstraints(Plasma::Constraints constraints)
{
    d->scheduleConstraintsUpdate(constraints);
}

void Applet::constraintsEvent(Plasma::Constraints constraints)
{
    //NOTE: do NOT put any code in here that reacts to constraints updates
    //      as it will not get called for any applet that reimplements constraintsEvent
    //      without calling the Applet:: version as well, which it shouldn't need to.
    //      INSTEAD put such code into flushPendingConstraintsEvents
    Q_UNUSED(constraints)
    //kDebug() << constraints << "constraints are FormFactor: " << formFactor()
    //         << ", Location: " << location();
    if (d->script) {
        d->script->constraintsEvent(constraints);
    }
}

QString Applet::title() const
{
    if (!d->customTitle.isEmpty()) {
        return d->customTitle;
    }

    if (d->isContainment) {
        const Containment *c = qobject_cast<const Containment*>(this);
        if (c && c->d->isPanelContainment()) {
            return i18n("Panel");
        } else if (!d->appletDescription.isValid()) {
            return i18n("Unknown");
        } else {
            return d->appletDescription.name();
        }
    } else if (!d->appletDescription.isValid()) {
        return i18n("Unknown Widget");
    }

    return d->appletDescription.name();
}

void Applet::setTitle(const QString &title) const
{
    d->customTitle = title;
}

QString Applet::icon() const
{
    if (!d->appletDescription.isValid()) {
        return QString();
    }

    return d->appletDescription.icon();
}

KPluginInfo Applet::pluginInfo() const
{
    return d->appletDescription;
}

ImmutabilityType Applet::immutability() const
{
    // if this object is itself system immutable, then just return that; it's the most
    // restrictive setting possible and will override anything that might be happening above it
    // in the Corona->Containment->Applet hierarchy
    if (d->transient || (d->mainConfig && d->mainConfig->isImmutable())) {
        return SystemImmutable;
    }

    //Returning the more strict immutability between the applet immutability, Containment and Corona
    ImmutabilityType upperImmutability = Mutable;
    Containment *cont = d->isContainment ? 0 : containment();

    if (cont) {
        upperImmutability = cont->immutability();
    }

    if (upperImmutability != Mutable) {
        // it's either system or user immutable, and we already check for local system immutability,
        // so upperImmutability is guaranteed to be as or more severe as this object's immutability
        return upperImmutability;
    } else {
        return d->immutability;
    }
}

void Applet::setImmutability(const ImmutabilityType immutable)
{
    if (d->immutability == immutable || immutable == Plasma::SystemImmutable) {
        // we do not store system immutability in d->immutability since that gets saved
        // out to the config file; instead, we check with
        // the config group itself for this information at all times. this differs from
        // corona, where SystemImmutability is stored in d->immutability.
        return;
    }

    d->immutability = immutable;
    updateConstraints(ImmutableConstraint);
}

QString Applet::launchErrorMessage() const
{
    return d->launchErrorMessage;
}

bool Applet::failedToLaunch() const
{
    return d->failed;
}

bool Applet::configurationRequired() const
{
    return d->needsConfig;
}

void Applet::setConfigurationRequired(bool needsConfig, const QString &reason)
{
    if (d->needsConfig == needsConfig) {
        return;
    }

    d->needsConfig = needsConfig;
    d->showConfigurationRequiredMessage(needsConfig, reason);
}

ItemStatus Applet::status() const
{
    return d->itemStatus;
}

void Applet::setStatus(const ItemStatus status)
{
    d->itemStatus = status;
    emit statusChanged(status);
}

void Applet::flushPendingConstraintsEvents()
{
    if (d->pendingConstraints == NoConstraint) {
        return;
    }

    if (d->constraintsTimer.isActive()) {
        d->constraintsTimer.stop();
    }

    //kDebug() << "fushing constraints: " << d->pendingConstraints << "!!!!!!!!!!!!!!!!!!!!!!!!!!!";
    Plasma::Constraints c = d->pendingConstraints;
    d->pendingConstraints = NoConstraint;

    if (c & Plasma::StartupCompletedConstraint) {
        //common actions
        bool unlocked = immutability() == Mutable;
        QAction *closeApplet = d->actions->action("remove");
        if (closeApplet) {
            closeApplet->setEnabled(unlocked);
            closeApplet->setVisible(unlocked);
            connect(closeApplet, SIGNAL(triggered(bool)), this, SLOT(destroy()), Qt::UniqueConnection);
        }

        QAction *configAction = d->actions->action("configure");
        if (configAction) {
            /*if (d->isContainment) {
                connect(configAction, SIGNAL(triggered(bool)), this, SLOT(requestConfiguration()), Qt::UniqueConnection);
            } else {
                connect(configAction, SIGNAL(triggered(bool)), this, SLOT(showConfigurationInterface()), Qt::UniqueConnection);
            }*/

            if (d->hasConfigurationInterface) {
                bool canConfig = unlocked || KAuthorized::authorize("plasma/allow_configure_when_locked");
                configAction->setVisible(canConfig);
                configAction->setEnabled(canConfig);
            }
        }

        QAction *runAssociatedApplication = d->actions->action("run associated application");
        if (runAssociatedApplication) {
            connect(runAssociatedApplication, SIGNAL(triggered(bool)), this, SLOT(runAssociatedApplication()), Qt::UniqueConnection);
        }

        d->updateShortcuts();
        Containment *cont = containment();
        Corona *corona = 0;
        if (cont) {
            corona = cont->corona();
        }
        if (corona) {
            connect(corona, SIGNAL(shortcutsChanged()), this, SLOT(updateShortcuts()), Qt::UniqueConnection);
        }
    }

    if (c & Plasma::ImmutableConstraint) {
        bool unlocked = immutability() == Mutable;
        QAction *action = d->actions->action("remove");
        if (action) {
            action->setVisible(unlocked);
            action->setEnabled(unlocked);
        }

        action = d->actions->action("configure");
        if (action && d->hasConfigurationInterface) {
            bool canConfig = unlocked || KAuthorized::authorize("plasma/allow_configure_when_locked");
            action->setVisible(canConfig);
            action->setEnabled(canConfig);
        }

        emit immutabilityChanged(immutability());
    }

    // now take care of constraints in special subclass: Contaiment
    Containment* containment = qobject_cast<Plasma::Containment*>(this);
    if (d->isContainment && containment) {
        containment->d->containmentConstraintsEvent(c);
    }

    // pass the constraint on to the actual subclass
    constraintsEvent(c);

    if (c & StartupCompletedConstraint) {
        // start up is done, we can now go do a mod timer
        if (d->modificationsTimer) {
            if (d->modificationsTimer->isActive()) {
                d->modificationsTimer->stop();
            }
        } else {
            d->modificationsTimer = new QBasicTimer;
        }
    }
}

QList<QAction*> Applet::contextualActions()
{
    //kDebug() << "empty context actions";
    return d->script ? d->script->contextualActions() : QList<QAction*>();
}

KActionCollection *Applet::actions() const
{
    return d->actions;
}

FormFactor Applet::formFactor() const
{
    Containment *c = containment();
    QObject *pw = qobject_cast<QObject *>(parent());
    Plasma::Applet *parentApplet = qobject_cast<Plasma::Applet *>(pw);
    //assumption: this loop is usually is -really- short or doesn't run at all
    while (!parentApplet && pw && pw->parent()) {
        pw = pw->parent();
        parentApplet = qobject_cast<Plasma::Applet *>(pw);
    }

    return c ? c->d->formFactor : Plasma::Planar;
}

Containment *Applet::containment() const
{
    if (d->isContainment) {
        Containment *c = qobject_cast<Containment*>(const_cast<Applet*>(this));
        if (c) {
            return c;
        }
    }

    QObject *parent = this->parent();
    Containment *c = 0;

    while (parent) {
        Containment *possibleC = dynamic_cast<Containment*>(parent);
        if (possibleC && possibleC->Applet::d->isContainment) {
            c = possibleC;
            break;
        }
        parent = parent->parent();
    }

    if (!c) {
        //if the applet is an offscreen widget its parentItem will be 0, while its parent
        //will be its parentWidget, so here we check the QObject hierarchy.
        QObject *objParent = this->parent();
        while (objParent) {
            Containment *possibleC = qobject_cast<Containment*>(objParent);
            if (possibleC && possibleC->Applet::d->isContainment) {
                c = possibleC;
                break;
            }
            objParent = objParent->parent();
        }
    }

    return c;
}

void Applet::setGlobalShortcut(const KShortcut &shortcut)
{
    if (!d->activationAction) {
        d->activationAction = new KAction(this);
        d->activationAction->setText(i18n("Activate %1 Widget", title()));
        d->activationAction->setObjectName(QString("activate widget %1").arg(id())); // NO I18N
        connect(d->activationAction, SIGNAL(triggered()), this, SIGNAL(activate()));
        connect(d->activationAction, SIGNAL(globalShortcutChanged(QKeySequence)),
                this, SLOT(globalShortcutChanged()));
    } else if (d->activationAction->globalShortcut() == shortcut) {
        return;
    }

    //kDebug() << "before" << shortcut.primary() << d->activationAction->globalShortcut().primary();
    d->activationAction->setGlobalShortcut(
        shortcut,
        KAction::ShortcutTypes(KAction::ActiveShortcut | KAction::DefaultShortcut),
        KAction::NoAutoloading);
    d->globalShortcutChanged();
}

KShortcut Applet::globalShortcut() const
{
    if (d->activationAction) {
        return d->activationAction->globalShortcut();
    }

    return KShortcut();
}

Location Applet::location() const
{
    Containment *c = containment();
    return c ? c->d->location : Plasma::Desktop;
}

bool Applet::hasConfigurationInterface() const
{
    return d->hasConfigurationInterface;
}

void Applet::setHasConfigurationInterface(bool hasInterface)
{
    if (hasInterface == d->hasConfigurationInterface) {
        return;
    }

    QAction *configAction = d->actions->action("configure");
    if (configAction) {
        bool enable = hasInterface;
        if (enable) {
            const bool unlocked = immutability() == Mutable;
            enable = unlocked || KAuthorized::authorize("plasma/allow_configure_when_locked");
        }
        configAction->setEnabled(enable);
    }

    d->hasConfigurationInterface = hasInterface;
}

bool Applet::isUserConfiguring() const
{
    return KConfigDialog::exists(d->configDialogId());
}

void Applet::showConfigurationInterface()
{
    if (!hasConfigurationInterface()) {
        return;
    }

    if (immutability() != Mutable && !KAuthorized::authorize("plasma/allow_configure_when_locked")) {
        return;
    }

    KConfigDialog *dlg = KConfigDialog::exists(d->configDialogId());

    if (dlg) {
        KWindowSystem::setOnDesktop(dlg->winId(), KWindowSystem::currentDesktop());
        dlg->show();
        KWindowSystem::activateWindow(dlg->winId());
        return;
    }

    if (d->package) {
        KConfigDialog *dialog = 0;

        const QString uiFile = d->package->filePath("mainconfigui");
        KDesktopFile df(d->package->path() + "/metadata.desktop");
        const QStringList kcmPlugins = df.desktopGroup().readEntry("X-Plasma-ConfigPlugins", QStringList());
        if (!uiFile.isEmpty() || !kcmPlugins.isEmpty()) {
            KConfigSkeleton *configLoader = d->configLoader ? d->configLoader : new KConfigSkeleton(0);
            dialog = new AppletConfigDialog(0, d->configDialogId(), configLoader);

            if (!d->configLoader) {
                // delete the temporary when this dialog is done
                configLoader->setParent(dialog);
            }

            dialog->setWindowTitle(d->configWindowTitle());
            dialog->setAttribute(Qt::WA_DeleteOnClose, true);
            bool hasPages = false;

            QFile f(uiFile);
            QUiLoader loader;
            QWidget *w = loader.load(&f);
            if (w) {
                dialog->addPage(w, i18n("Settings"), icon(), i18n("%1 Settings", title()));
                hasPages = true;
            }

            foreach (const QString &kcm, kcmPlugins) {
#if !PLASMA_NO_KUTILS
                KCModuleProxy *module = new KCModuleProxy(kcm);
                if (module->realModule()) {
                    //preemptively load modules to prevent save() crashing on some kcms, like powerdevil ones
                    module->load();
                    connect(module, SIGNAL(changed(bool)), dialog, SLOT(settingsModified(bool)));
                    connect(dialog, SIGNAL(okClicked()),
                            module->realModule(), SLOT(save()));
                    connect(dialog, SIGNAL(applyClicked()),
                            module->realModule(), SLOT(save()));
                    dialog->addPage(module, module->moduleInfo().moduleName(), module->moduleInfo().icon());
                    hasPages = true;
                } else {
                    delete module;
                }
#else
                KService::Ptr service = KService::serviceByStorageId(kcm);
                if (service) {
                    QString error;
                    KCModule *module = service->createInstance<KCModule>(dialog, QVariantList(), &error);
                    if (module) {
                        module->load();
                        connect(module, SIGNAL(changed(bool)), dialog, SLOT(settingsModified(bool)));
                        connect(dialog, SIGNAL(okClicked()),
                                module, SLOT(save()));
                        connect(dialog, SIGNAL(applyClicked()), 
                                module, SLOT(save()));
                        dialog->addPage(module, service->name(), service->icon());
                        hasPages = true;
                    } else {
#ifndef NDEBUG
                        kDebug() << "failed to load kcm" << kcm << "for" << title();
#endif
                    }
                }
#endif
            }

            if (hasPages) {
                d->addGlobalShortcutsPage(dialog);
                dialog->show();
            } else {
                delete dialog;
                dialog = 0;
            }
        }

        if (!dialog && d->script) {
            d->script->showConfigurationInterface();
        }
    } else if (d->script) {
        d->script->showConfigurationInterface();
    } else {
        KConfigDialog *dialog = d->generateGenericConfigDialog();
        d->addStandardConfigurationPages(dialog);
        showConfigurationInterface(dialog);
    }

    emit releaseVisualFocus();
}

void Applet::showConfigurationInterface(QWidget *widget)
{
    if (!containment() || !containment()->corona() ||
        !containment()->corona()->dialogManager()) {
        widget->show();
        return;
    }

    QMetaObject::invokeMethod(containment()->corona()->dialogManager(), "showDialog", Q_ARG(QWidget *, widget), Q_ARG(Plasma::Applet *, this));
}

void Applet::configChanged()
{
    if (d->script) {
        if (d->configLoader) {
            d->configLoader->readConfig();
        }
        d->script->configChanged();
    }
}

void Applet::createConfigurationInterface(KConfigDialog *parent)
{
    Q_UNUSED(parent)
    // virtual method reimplemented by subclasses.
    // do not put anything here ...
}

void Applet::setAssociatedApplication(const QString &string)
{
    AssociatedApplicationManager::self()->setApplication(this, string);

    QAction *runAssociatedApplication = d->actions->action("run associated application");
    if (runAssociatedApplication) {
        bool valid = AssociatedApplicationManager::self()->appletHasValidAssociatedApplication(this);
        runAssociatedApplication->setVisible(valid);
        runAssociatedApplication->setEnabled(valid);
    }
}

void Applet::setAssociatedApplicationUrls(const QList<QUrl> &urls)
{
    AssociatedApplicationManager::self()->setUrls(this, urls);

    QAction *runAssociatedApplication = d->actions->action("run associated application");
    if (runAssociatedApplication) {
        bool valid = AssociatedApplicationManager::self()->appletHasValidAssociatedApplication(this);
        runAssociatedApplication->setVisible(valid);
        runAssociatedApplication->setEnabled(valid);
    }
}

QString Applet::associatedApplication() const
{
    return AssociatedApplicationManager::self()->application(this);
}

QList<QUrl> Applet::associatedApplicationUrls() const
{
    return AssociatedApplicationManager::self()->urls(this);
}

void Applet::runAssociatedApplication()
{
    AssociatedApplicationManager::self()->run(this);
}

bool Applet::hasValidAssociatedApplication() const
{
    return AssociatedApplicationManager::self()->appletHasValidAssociatedApplication(this);
}

Applet *Applet::loadPlasmoid(const QString &path, uint appletId)
{
    if (QFile::exists(path + "/metadata.desktop")) {
        KService service(path + "/metadata.desktop");
        const QStringList &types = service.serviceTypes();

        if (types.contains("Plasma/Containment")) {
            return new Containment(path, appletId);
        } else {
            return new Applet(path, appletId);
        }
    }

    return 0;
}

void Applet::timerEvent(QTimerEvent *event)
{
    if (d->transient) {
        d->constraintsTimer.stop();
        if (d->modificationsTimer) {
            d->modificationsTimer->stop();
        }
        return;
    }

    if (event->timerId() == d->constraintsTimer.timerId()) {
        d->constraintsTimer.stop();

        // Don't flushPendingConstraints if we're just starting up
        // flushPendingConstraints will be called by Corona
        if(!(d->pendingConstraints & Plasma::StartupCompletedConstraint)) {
            flushPendingConstraintsEvents();
        }
    } else if (d->modificationsTimer && event->timerId() == d->modificationsTimer->timerId()) {
        d->modificationsTimer->stop();
        // invalid group, will result in save using the default group
        KConfigGroup cg;

        save(cg);
        emit configNeedsSaving();
    }
}

bool Applet::isContainment() const
{
    return d->isContainment;
}

} // Plasma namespace

#include "moc_applet.cpp"
