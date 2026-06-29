/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 MainstreamOS
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 */

#include "InstallMethodPage.h"

#include "Branding.h"

#include "utils/Logger.h"

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QSvgRenderer>

namespace InstallMethod
{

// ─── Styling constants ──────────────────────────────────────────────────────
// M3 dark tokens. Mirrored from the iso's branding stylesheet — kept inline
// rather than parsed from a .qss so the page renders the same even before
// a branding stylesheet is applied (the rest of Calamares applies branding
// styles to the parent window after this page is constructed).

namespace
{
// SVG glyph render size in logical pixels. Sized for the current
// 4-card layout (Default / Customize / Console Mode / OS Only). If the
// Developer / group-install Gaming cards are also re-enabled here, drop
// this to ~104 so all six fit in a 1100-wide window.
constexpr int kIconRenderSize = 112;

constexpr const char* kCardBaseStyle = R"qss(
    InstallMethod--MethodCard {
        background-color: #2b2a2a;
        border: 1px solid #49464a;
        border-radius: 16px;
    }
    InstallMethod--MethodCard QLabel#cardTitle {
        color: #ece6e9;
        font-size: 18px;
        font-weight: 500;
        background: transparent;
    }
    InstallMethod--MethodCard QLabel#cardDescription {
        color: #cbc5ca;
        font-size: 12px;
        background: transparent;
    }
)qss";

constexpr const char* kCardHoverStyle = R"qss(
    InstallMethod--MethodCard {
        background-color: #353333;
        border: 1px solid #6b676b;
        border-radius: 16px;
    }
)qss";

constexpr const char* kCardSelectedStyle = R"qss(
    InstallMethod--MethodCard {
        background-color: #353c45;
        border: 2px solid #18A0BE;
        border-radius: 16px;
    }
    InstallMethod--MethodCard QLabel#cardTitle {
        color: #ece6e9;
        font-size: 18px;
        font-weight: 600;
        background: transparent;
    }
    InstallMethod--MethodCard QLabel#cardDescription {
        color: #cbc5ca;
        font-size: 12px;
        background: transparent;
    }
)qss";

/** Render a designed SVG (qrc path) at @p size logical pixels into a
 *  QPixmap with DPR awareness. The SVG already carries its full
 *  Mainstream M3 palette, so we render it as-is — no SourceIn tint
 *  pass. Selection / hover feedback lives entirely on the card
 *  chrome (border + background), keeping the icon stable across
 *  states the way M3 illustration tiles are meant to behave. */
QPixmap
renderIconPixmap( const QString& resourcePath, int size )
{
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    const int   px  = qRound( size * dpr );

    QPixmap pm( px, px );
    pm.setDevicePixelRatio( dpr );
    pm.fill( Qt::transparent );

    QSvgRenderer renderer( resourcePath );
    if ( !renderer.isValid() )
    {
        cWarning() << "InstallMethod: SVG failed to load:" << resourcePath;
        return pm;
    }

    QPainter painter( &pm );
    painter.setRenderHint( QPainter::Antialiasing );
    painter.setRenderHint( QPainter::SmoothPixmapTransform );
    // After setDevicePixelRatio() the painter draws in LOGICAL pixels,
    // so the render rect is `size`, not `px`. Passing `px` works on a
    // dpr=1 display (px == size) but on HiDPI clips to the top-left
    // quadrant because the SVG ends up drawn into a logical region
    // larger than the QPixmap itself.
    renderer.render( &painter, QRectF( 0, 0, size, size ) );
    return pm;
}
}  // anonymous namespace

// ─── MethodCard ─────────────────────────────────────────────────────────────

MethodCard::MethodCard( Choice choice,
                        const QString& iconResourcePath,
                        const QString& title,
                        const QString& description,
                        QWidget* parent )
    : QFrame( parent )
    , m_choice( choice )
{
    setFrameShape( QFrame::NoFrame );
    setCursor( Qt::PointingHandCursor );
    setFocusPolicy( Qt::StrongFocus );
    // Equal-width sharing of the cards' row is enforced by the parent
    // QHBoxLayout giving every card a stretch factor of 1. Expanding
    // both directions lets the card grow vertically into any leftover
    // space on the page.
    setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    setMinimumWidth( 220 );

    auto* col = new QVBoxLayout( this );
    col->setContentsMargins( 22, 28, 22, 24 );
    col->setSpacing( 14 );
    col->setAlignment( Qt::AlignHCenter );

    // ── Big SVG icon at the top ────────────────────────────────────────
    m_iconLabel = new QLabel( this );
    m_iconLabel->setObjectName( QStringLiteral( "cardIcon" ) );
    m_iconLabel->setAlignment( Qt::AlignCenter );
    m_iconLabel->setFixedSize( kIconRenderSize, kIconRenderSize );
    m_iconLabel->setPixmap( renderIconPixmap( iconResourcePath, kIconRenderSize ) );

    // ── Title ──────────────────────────────────────────────────────────
    auto* titleLabel = new QLabel( title, this );
    titleLabel->setObjectName( QStringLiteral( "cardTitle" ) );
    titleLabel->setAlignment( Qt::AlignCenter );
    titleLabel->setWordWrap( true );

    // ── Description ────────────────────────────────────────────────────
    auto* descLabel = new QLabel( description, this );
    descLabel->setObjectName( QStringLiteral( "cardDescription" ) );
    descLabel->setAlignment( Qt::AlignCenter );
    descLabel->setWordWrap( true );

    // Top + bottom stretches center the icon/title/description group
    // vertically within the card. Without them the content sits flush
    // against the top margin and the bottom half of the card is empty
    // when the page has lots of vertical room.
    col->addStretch( 1 );
    col->addWidget( m_iconLabel, 0, Qt::AlignHCenter );
    col->addSpacing( 6 );
    col->addWidget( titleLabel );
    col->addWidget( descLabel );
    col->addStretch( 1 );

    applyStyle();
}

void
MethodCard::setSelected( bool selected )
{
    if ( selected == m_selected )
    {
        return;
    }
    m_selected = selected;
    applyStyle();
}

void
MethodCard::mousePressEvent( QMouseEvent* event )
{
    if ( event->button() == Qt::LeftButton )
    {
        emit picked( m_choice );
    }
    QFrame::mousePressEvent( event );
}

void
MethodCard::keyPressEvent( QKeyEvent* event )
{
    // Space / Enter selects the focused card — keyboard parity with
    // mouse click for accessibility.
    if ( event->key() == Qt::Key_Space || event->key() == Qt::Key_Return
         || event->key() == Qt::Key_Enter )
    {
        emit picked( m_choice );
        event->accept();
        return;
    }
    QFrame::keyPressEvent( event );
}

void
MethodCard::enterEvent( QEnterEvent* event )
{
    m_hovered = true;
    applyStyle();
    QFrame::enterEvent( event );
}

void
MethodCard::leaveEvent( QEvent* event )
{
    m_hovered = false;
    applyStyle();
    QFrame::leaveEvent( event );
}

void
MethodCard::applyStyle()
{
    // Selected wins over hover wins over base. We append the variant
    // stylesheet onto the base so partial overrides (just the border
    // change on hover) inherit untouched base rules.
    if ( m_selected )
    {
        setStyleSheet( QString::fromUtf8( kCardSelectedStyle ) );
    }
    else if ( m_hovered )
    {
        setStyleSheet( QString::fromUtf8( kCardBaseStyle ) + QString::fromUtf8( kCardHoverStyle ) );
    }
    else
    {
        setStyleSheet( QString::fromUtf8( kCardBaseStyle ) );
    }
}

// ─── InstallMethodPage ──────────────────────────────────────────────────────

InstallMethodPage::InstallMethodPage( Config* config, QWidget* parent )
    : QWidget( parent )
    , m_config( config )
{
    auto* root = new QVBoxLayout( this );
    root->setContentsMargins( 28, 24, 28, 24 );
    root->setSpacing( 14 );

    // Header — keeps the layout consistent with welcome / users pages.
    auto* headerTitle = new QLabel( tr( "Let’s get started", "@title" ), this );
    headerTitle->setStyleSheet(
        QStringLiteral( "color: #ece6e9; font-size: 22px; font-weight: 600; background: transparent;" ) );

    auto* headerSub = new QLabel(
        tr( "How should we get your new system ready? Pick the option that fits how you "
            "use a computer — you can always add or remove apps later.",
            "@subtitle" ),
        this );
    headerSub->setStyleSheet(
        QStringLiteral( "color: #cbc5ca; font-size: 13px; background: transparent;" ) );
    headerSub->setWordWrap( true );

    root->addWidget( headerTitle );
    root->addWidget( headerSub );
    root->addSpacing( 12 );

    // ── Four cards laid out horizontally with equal stretch ───────────
    // Each card gets stretch=1, so the row divides its width evenly
    // regardless of content length. The row itself takes stretch=1 in
    // the page's vertical layout so the cards fill the available
    // height below the header.
    auto* cardsRow = new QHBoxLayout;
    cardsRow->setSpacing( 16 );
    cardsRow->setContentsMargins( 0, 0, 0, 0 );

    auto addCard = [ this, cardsRow ]( Choice choice,
                                       const QString& iconResource,
                                       const QString& title,
                                       const QString& desc )
    {
        auto* card = new MethodCard( choice, iconResource, title, desc, this );
        connect( card, &MethodCard::picked, this, &InstallMethodPage::onCardPicked );
        m_cards.append( card );
        cardsRow->addWidget( card, /*stretch=*/1 );
    };

    // Custom SVG glyphs shipped via installmethod.qrc — drawn directly
    // in the Mainstream M3 palette so no runtime recolor is needed.
    //
    // The Developer card and the group-install Gaming card are
    // intentionally NOT wired up here even though their Choice enum
    // values, Config plumbing, prettyStatus copy, and SVG icons
    // (developer.svg; gaming.svg is reused below for Console Mode) are
    // all kept. Re-enabling them is a matter of pasting two more
    // addCard() calls below and shrinking the per-card sizing (see
    // kIconRenderSize note above) so all the cards still fit the window.
    addCard( Choice::Default,
             QStringLiteral( ":/installmethod/icons/default-apps.svg" ),
             tr( "Default Apps", "@option" ),
             tr( "Everything you need for a complete desktop experience. Ideal for new "
                 "Linux users, family computers, or anyone who wants a fully-equipped "
                 "system right out of the box.",
                 "@option-description" ) );

    addCard( Choice::Custom,
             QStringLiteral( ":/installmethod/icons/customize.svg" ),
             tr( "Customize Your Apps", "@option" ),
             tr( "Hand-pick from a curated selection of popular Linux applications. "
                 "Perfect if you know what you want or prefer to choose exactly what "
                 "gets installed.",
                 "@option-description" ) );

    addCard( Choice::Console,
             QStringLiteral( ":/installmethod/icons/gaming.svg" ),
             tr( "Console Mode", "@option" ),
             tr( "The same apps as Default, but your computer boots straight into Steam's "
                 "gamescope mode like a game console. You can switch to the desktop, and "
                 "change this anytime in Gaming Mode Setup. The install method may take a "
                 "few extra minutes.",
                 "@option-description" ) );

    addCard( Choice::OsOnly,
             QStringLiteral( ":/installmethod/icons/os-only.svg" ),
             tr( "OS Only", "@option" ),
             tr( "A clean slate with just the base system and desktop. For experienced "
                 "users who prefer to build their own setup from scratch.",
                 "@option-description" ) );

    root->addLayout( cardsRow, /*stretch=*/1 );

    // Reflect the initial / config-supplied choice in the UI.
    connect( m_config, &Config::choiceChanged, this, &InstallMethodPage::onConfigChanged );
    onConfigChanged( m_config->choice() );
}

void
InstallMethodPage::onCardPicked( Choice picked )
{
    m_config->setChoice( picked );
}

void
InstallMethodPage::onConfigChanged( Choice newChoice )
{
    for ( auto* card : m_cards )
    {
        card->setSelected( card->choice() == newChoice );
    }
}

}  // namespace InstallMethod
