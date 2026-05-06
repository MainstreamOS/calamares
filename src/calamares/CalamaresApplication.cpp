/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */
#include "CalamaresApplication.h"

#include "CalamaresConfig.h"
#include "CalamaresVersionX.h"
#include "CalamaresWindow.h"
#include "progresstree/ProgressTreeView.h"

#include "Branding.h"
#include "JobQueue.h"
#include "Settings.h"
#include "ViewManager.h"
#include "locale/TranslationsModel.h"
#include "modulesystem/ModuleManager.h"
#include "utils/Dirs.h"
#include "utils/Gui.h"
#include "utils/Logger.h"
#include "utils/System.h"
#ifdef WITH_QML
#include "utils/Qml.h"
#endif
#include "utils/Retranslator.h"
#include "viewpages/ViewStep.h"

#include <QAbstractItemView>
#include <QChildEvent>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPolygon>
#include <QRegion>
#include <QScreen>
#include <QTimer>
#include <QWidget>

/// @brief Convenience for "are the settings in debug mode"
static bool
isDebug()
{
    return Calamares::Settings::instance() && Calamares::Settings::instance()->debugMode();
}

namespace
{

constexpr auto* PopupFrameProperty = "calamaresRoundedComboPopup";
constexpr auto* PopupMovingProperty = "calamaresRoundedComboPopupMoving";
constexpr int PopupRadius = 16;
constexpr int PopupPadding = 8;
constexpr int PopupGap = 4;
const QColor PopupBackground( 0x2b, 0x2a, 0x2a );

}  // namespace

class ComboBoxPopupStyler : public QObject
{
public:
    explicit ComboBoxPopupStyler( QObject* parent )
        : QObject( parent )
    {
    }

protected:
    bool eventFilter( QObject* obj, QEvent* ev ) override
    {
        if ( ev->type() == QEvent::ChildAdded )
        {
            if ( auto* combo = qobject_cast< QComboBox* >( obj ) )
            {
                QObject* added = static_cast< QChildEvent* >( ev )->child();
                if ( added && added->isWidgetType() )
                {
                    // QEvent::ChildAdded fires from the QObject base
                    // constructor, *before* QComboBoxPrivateContainer's own
                    // constructor finishes. At this point metaObject() reports
                    // the base class ("QWidget") and qobject_cast<QFrame*>
                    // returns null — so casting here would always miss the
                    // popup container. Defer one event-loop tick; by then the
                    // subclass is fully constructed and the cast succeeds.
                    QPointer< QObject > safeAdded( added );
                    QPointer< QComboBox > safeCombo( combo );
                    QTimer::singleShot( 0, this, [ this, safeAdded, safeCombo ]() {
                        if ( !safeAdded || !safeCombo )
                        {
                            return;
                        }
                        auto* frame = qobject_cast< QFrame* >( safeAdded.data() );
                        if ( frame && !frame->property( PopupFrameProperty ).toBool() )
                        {
                            setupPopupFrame( safeCombo.data(), frame );
                        }
                    } );
                }
            }

            return QObject::eventFilter( obj, ev );
        }

        auto* frame = qobject_cast< QFrame* >( obj );
        if ( !frame || !frame->property( PopupFrameProperty ).toBool() )
        {
            return QObject::eventFilter( obj, ev );
        }

        if ( ev->type() == QEvent::Show )
        {
            if ( auto* combo = qobject_cast< QComboBox* >( frame->parent() ) )
            {
                positionPopup( combo, frame );
            }
            applyRoundedMask( frame );
            return QObject::eventFilter( obj, ev );
        }

        if ( ev->type() == QEvent::Resize )
        {
            applyRoundedMask( frame );
            return QObject::eventFilter( obj, ev );
        }

        if ( ev->type() == QEvent::Paint )
        {
            // WA_TranslucentBackground (set in setupPopupFrame) initialises
            // the backing store to (0,0,0,0); paintEvent only needs to draw
            // the rounded surface. Pixels outside the path stay transparent,
            // and on every compositor we target the desktop shows through.
            QPainter p( frame );
            p.setRenderHint( QPainter::Antialiasing, true );
            p.setPen( Qt::NoPen );
            p.setBrush( PopupBackground );
            QPainterPath path;
            path.addRoundedRect( QRectF( frame->rect() ), PopupRadius, PopupRadius );
            p.drawPath( path );
            return true;
        }

        return QObject::eventFilter( obj, ev );
    }

private:
    void setupPopupFrame( QComboBox* combo, QFrame* frame )
    {
        frame->setProperty( PopupFrameProperty, true );
        frame->setAttribute( Qt::WA_TranslucentBackground, true );
        frame->setFrameShape( QFrame::NoFrame );
        frame->installEventFilter( this );

        stylePopupView( combo );
        applyRoundedMask( frame );
    }

    void stylePopupView( QComboBox* combo )
    {
        auto* view = combo ? combo->view() : nullptr;
        if ( !view )
        {
            return;
        }

        view->setAttribute( Qt::WA_TranslucentBackground, true );
        view->setFrameShape( QFrame::NoFrame );
        view->viewport()->setAttribute( Qt::WA_TranslucentBackground, true );
        view->setContentsMargins( PopupPadding, PopupPadding, PopupPadding, PopupPadding );

        // Force the popup's icon size to match the combo button's. Without
        // this, modules whose model returns large native pixmaps (e.g. the
        // partition module's "Select storage device" combo, whose model
        // ships ~48px disk icons) render the popup rows at native pixmap
        // size while the combo button renders them at QComboBox::iconSize.
        // The mismatch made the storage-device dropdown rows several times
        // taller than the rows of any other combo. Snapping the view's
        // iconSize to the combo's keeps both ends visually consistent.
        const QSize comboIconSize = combo->iconSize();
        if ( comboIconSize.isValid() && !comboIconSize.isEmpty() )
        {
            view->setIconSize( comboIconSize );
        }
    }

    void positionPopup( QComboBox* combo, QWidget* popup )
    {
        if ( popup->property( PopupMovingProperty ).toBool() )
        {
            return;
        }

        popup->setProperty( PopupMovingProperty, true );
        const QPoint anchor = combo->mapToGlobal( QPoint( 0, combo->height() + PopupGap ) );
        popup->setGeometry( anchor.x(), anchor.y(), combo->width(), popup->height() );
        popup->setProperty( PopupMovingProperty, false );
    }

    // setMask() defines the popup window's bounding shape. On compositors
    // that don't honor WA_TranslucentBackground at all this is the only
    // thing that prevents the rectangular bounding box from showing as a
    // dark slab behind the painted rounded surface.
    //
    // On compositors that *do* honor translucency (Hyprland included),
    // the visible boundary comes from the antialiased rounded path painted
    // in paintEvent — and we want the mask to be *just slightly larger*
    // than that path so the polygon mask edge never cuts into the AA
    // curve. Using exactly PopupRadius made the polygon segments sit
    // right on the curve, perceptible as squared-off lighter tips at
    // each corner. Inflating by a couple of pixels pushes those segments
    // outside the visible AA edge and lets the smooth painted curve
    // define the corner instead.
    void applyRoundedMask( QWidget* frame )
    {
        const QRect r = frame->rect();
        if ( r.isEmpty() )
        {
            return;
        }

        constexpr qreal MaskOvershoot = 2.0;
        QPainterPath path;
        path.addRoundedRect( QRectF( r ).adjusted( -MaskOvershoot, -MaskOvershoot, MaskOvershoot, MaskOvershoot ),
                             PopupRadius + MaskOvershoot,
                             PopupRadius + MaskOvershoot );
        const QRegion region( path.toFillPolygon().toPolygon() );
        if ( frame->mask() != region )
        {
            frame->setMask( region );
        }
    }
};

CalamaresApplication::CalamaresApplication( int& argc, char* argv[] )
    : QApplication( argc, argv )
    , m_mainwindow( nullptr )
    , m_moduleManager( nullptr )
{
    // Setting the organization name makes the default cache
    // directory -- where Calamares stores logs, for instance --
    // <org>/<app>/, so we end up with ~/.cache/Calamares/calamares/
    // which is excessively squidly.
    //
    // setOrganizationName( QStringLiteral( CALAMARES_ORGANIZATION_NAME ) );
    setOrganizationDomain( QStringLiteral( CALAMARES_ORGANIZATION_DOMAIN ) );
    setApplicationName( QStringLiteral( CALAMARES_APPLICATION_NAME ) );
    setApplicationVersion( QStringLiteral( CALAMARES_VERSION ) );

    QFont f = font();
    Calamares::setDefaultFontSize( f.pointSize() );

    // QComboBox popup styling — installed app-wide so every combo in any
    // module / dialog picks up the dots-hyprland-style frameless surface.
    installEventFilter( new ComboBoxPopupStyler( this ) );
}

void
CalamaresApplication::init()
{
    Logger::setupLogfile();
    cDebug() << "Calamares version:" << CALAMARES_VERSION;
    cDebug() << Logger::SubEntry << "Using Qt version:" << qVersion();
    cDebug() << Logger::SubEntry << "Build type:" << CMAKE_BUILD_TYPE;
#ifdef WITH_PYBIND11
    cDebug() << Logger::SubEntry << "Using PyBind11";
#endif
#ifdef WITH_BOOST_PYTHON
    cDebug() << Logger::SubEntry << "Using Boost Python";
#endif
    cDebug() << Logger::SubEntry << "Using settings:" << Calamares::Settings::instance()->path();
    cDebug() << Logger::SubEntry << "Using log file:" << Logger::logFile();
    cDebug() << Logger::SubEntry << "Languages:" << Calamares::Locale::availableLanguages();

    if ( !Calamares::Settings::instance() )
    {
        cError() << "Must create Calamares::Settings before the application.";
        ::exit( 1 );
    }
    initQmlPath();
    initBranding();

    Calamares::installTranslator();

    setQuitOnLastWindowClosed( false );
    setWindowIcon( QIcon( Calamares::Branding::instance()->imagePath( Calamares::Branding::ProductIcon ) ) );

    cDebug() << Logger::SubEntry << "STARTUP: initSettings, initQmlPath, initBranding done";

    initModuleManager();  //also shows main window

    cDebug() << Logger::SubEntry << "STARTUP: initModuleManager: module init started";
}

CalamaresApplication::~CalamaresApplication()
{
    Logger::CDebug( Logger::LOGVERBOSE ) << "Shutting down Calamares...";
    Logger::CDebug( Logger::LOGVERBOSE ) << Logger::SubEntry << "Finished shutdown.";
}

CalamaresApplication*
CalamaresApplication::instance()
{
    return qobject_cast< CalamaresApplication* >( QApplication::instance() );
}

CalamaresWindow*
CalamaresApplication::mainWindow()
{
    return m_mainwindow;
}

static QStringList
brandingFileCandidates( bool assumeBuilddir, const QString& brandingFilename )
{
    QStringList brandingPaths;
    if ( Calamares::isAppDataDirOverridden() )
    {
        brandingPaths << Calamares::appDataDir().absoluteFilePath( brandingFilename );
    }
    else
    {
        if ( assumeBuilddir )
        {
            brandingPaths << ( QDir::currentPath() + QStringLiteral( "/src/" ) + brandingFilename );
        }
        if ( Calamares::haveExtraDirs() )
        {
            for ( auto s : Calamares::extraDataDirs() )
            {
                brandingPaths << ( s + brandingFilename );
            }
        }
        brandingPaths << QDir( CMAKE_INSTALL_FULL_SYSCONFDIR "/calamares/" ).absoluteFilePath( brandingFilename );
        brandingPaths << Calamares::appDataDir().absoluteFilePath( brandingFilename );
    }

    return brandingPaths;
}

void
CalamaresApplication::initQmlPath()
{
#ifdef WITH_QML
    if ( !Calamares::initQmlModulesDir() )
    {
        ::exit( EXIT_FAILURE );
    }
#endif
}

void
CalamaresApplication::initBranding()
{
    QString brandingComponentName = Calamares::Settings::instance()->brandingComponentName();
    if ( brandingComponentName.simplified().isEmpty() )
    {
        cError() << "FATAL: branding component not set in settings.conf";
        ::exit( EXIT_FAILURE );
    }

    QString brandingDescriptorSubpath = QString( "branding/%1/branding.desc" ).arg( brandingComponentName );
    QStringList brandingFileCandidatesByPriority = brandingFileCandidates( isDebug(), brandingDescriptorSubpath );

    QFileInfo brandingFile;
    bool found = false;

    foreach ( const QString& path, brandingFileCandidatesByPriority )
    {
        QFileInfo pathFi( path );
        if ( pathFi.exists() && pathFi.isReadable() )
        {
            brandingFile = pathFi;
            found = true;
            break;
        }
    }

    if ( !found || !brandingFile.exists() || !brandingFile.isReadable() )
    {
        cError() << "Cowardly refusing to continue startup without branding."
                 << Logger::DebugList( brandingFileCandidatesByPriority );
        if ( Calamares::isAppDataDirOverridden() )
        {
            cError() << "FATAL: explicitly configured application data directory is missing" << brandingComponentName;
        }
        else
        {
            cError() << "FATAL: none of the expected branding descriptor file paths exist.";
        }
        ::exit( EXIT_FAILURE );
    }

    new Calamares::Branding( brandingFile.absoluteFilePath(), this, devicePixelRatio() );
}

void
CalamaresApplication::initModuleManager()
{
    m_moduleManager = new Calamares::ModuleManager( Calamares::Settings::instance()->modulesSearchPaths(), this );
    connect( m_moduleManager, &Calamares::ModuleManager::initDone, this, &CalamaresApplication::initView );
    m_moduleManager->init();
}

/** @brief centers the widget @p w on (a) screen
 *
 * This tries to duplicate the (deprecated) qApp->desktop()->availableGeometry()
 * placement by iterating over screens and putting Calamares in the first
 * one where it fits; this is *generally* the primary screen.
 *
 * With debugging, it would look something like this (2 screens attached,
 * primary at +1080+240 because I have a very strange X setup). Before
 * being mapped, the Calamares window is at +0+0 but does have a size.
 * The first screen's geometry includes the offset from the origin in
 * screen coordinates.
 *
 *  Proposed window size: 1024 520
 *  Window QRect(0,0 1024x520)
 *  Screen QRect(1080,240 2560x1440)
 *  Moving QPoint(1848,700)
 *  Screen QRect(0,0 1080x1920)
 *
 */
static void
centerWindowOnScreen( QWidget* w )
{
    QList< QScreen* > screens = qApp->screens();
    QPoint windowCenter = w->rect().center();
    QSize windowSize = w->rect().size();

    for ( const auto* screen : screens )
    {
        QSize screenSize = screen->availableGeometry().size();
        if ( ( screenSize.width() >= windowSize.width() ) && ( screenSize.height() >= windowSize.height() ) )
        {
            w->move( screen->availableGeometry().center() - windowCenter );
            break;
        }
    }
}

void
CalamaresApplication::initView()
{
    cDebug() << "STARTUP: initModuleManager: all modules init done";
    initJobQueue();
    cDebug() << "STARTUP: initJobQueue done";

    m_mainwindow = new CalamaresWindow();  //also creates ViewManager

    connect( m_moduleManager, &Calamares::ModuleManager::modulesLoaded, this, &CalamaresApplication::initViewSteps );
    connect( m_moduleManager, &Calamares::ModuleManager::modulesFailed, this, &CalamaresApplication::initFailed );

    QTimer::singleShot( 0, m_moduleManager, &Calamares::ModuleManager::loadModules );

    if ( Calamares::Branding::instance() && Calamares::Branding::instance()->windowPlacementCentered() )
    {
        centerWindowOnScreen( m_mainwindow );
    }
    cDebug() << "STARTUP: CalamaresWindow created; loadModules started";
}

void
CalamaresApplication::initViewSteps()
{
    cDebug() << "STARTUP: loadModules for all modules done";
    m_moduleManager->checkRequirements();
    if ( Calamares::Branding::instance()->windowMaximize() )
    {
        m_mainwindow->setWindowFlag( Qt::FramelessWindowHint );
        m_mainwindow->showMaximized();
    }
    else
    {
        m_mainwindow->show();
    }

    cDebug() << "STARTUP: Window now visible and ProgressTreeView populated";
    cDebug() << Logger::SubEntry << Calamares::ViewManager::instance()->viewSteps().count() << "view steps loaded.";
    Calamares::ViewManager::instance()->onInitComplete();
}

void
CalamaresApplication::initFailed( const QStringList& l )
{
    cError() << "STARTUP: failed modules are" << l;
    m_mainwindow->show();
}

void
CalamaresApplication::initJobQueue()
{
    Calamares::JobQueue* jobQueue = new Calamares::JobQueue( this );
    new Calamares::System( Calamares::Settings::instance()->doChroot(), this );
    Calamares::Branding::instance()->setGlobals( jobQueue->globalStorage() );
}
