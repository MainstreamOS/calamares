/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "ResultsListWidget.h"

#include "ResultDelegate.h"

#include "Branding.h"
#include "Settings.h"
#include "utils/Gui.h"
#include "utils/Logger.h"
#include "utils/Retranslator.h"
#include "widgets/FixedAspectRatioLabel.h"
#include "widgets/WaitingWidget.h"

#include <QAbstractButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListView>
#include <QVBoxLayout>

ResultsListWidget::ResultsListWidget( Config* config, QWidget* parent )
    : QWidget( parent )
    , m_config( config )
{
    setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

    auto mainLayout = new QVBoxLayout;
    setLayout( mainLayout );

    QHBoxLayout* explanationLayout = new QHBoxLayout;
    m_explanation = new QLabel( m_config->warningMessage() );
    m_explanation->setWordWrap( true );
    m_explanation->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
    m_explanation->setOpenExternalLinks( false );
    m_explanation->setObjectName( "resultsExplanation" );
    explanationLayout->addWidget( m_explanation );
    m_countdown = new CountdownWaitingWidget;
    m_countdown->setToolTip( tr( "Checking requirements again in a few seconds…" ) );
    m_countdown->start();
    explanationLayout->addWidget( m_countdown );

    mainLayout->addLayout( explanationLayout );
    mainLayout->addSpacing( Calamares::defaultFontHeight() / 2 );

    auto* listview = new QListView( this );
    listview->setSelectionMode( QAbstractItemView::NoSelection );
    listview->setDragDropMode( QAbstractItemView::NoDragDrop );
    listview->setAcceptDrops( false );
    listview->setItemDelegate( new ResultDelegate( this, Calamares::RequirementsModel::NegatedText ) );
    listview->setModel( config->unsatisfiedRequirements() );
    m_centralWidget = listview;
    m_centralLayout = mainLayout;

    mainLayout->addWidget( listview );
    mainLayout->addStretch();

    connect( config, &Config::warningMessageChanged, m_explanation, &QLabel::setText );
}

void
ResultsListWidget::requirementsComplete()
{
    // Check that the satisfaction of the requirements:
    // - if everything is satisfied, show the welcome image
    // - otherwise, if all the mandatory ones are satisfied,
    //   we won't be re-checking (see ModuleManager::checkRequirements)
    //   so hide the countdown,
    // - otherwise we have unsatisfied mandatory requirements,
    //   so keep the countdown and the list of problems.
    const bool requirementsSatisfied = m_config->requirementsModel()->satisfiedRequirements();
    const bool mandatoryRequirementsSatisfied = m_config->requirementsModel()->satisfiedMandatory();

    if ( mandatoryRequirementsSatisfied )
    {
        m_countdown->stop();
        m_countdown->hide();
    }
    if ( requirementsSatisfied )
    {
        delete m_centralWidget;
        m_centralWidget = nullptr;

        // Mainstream fork: when requirements pass, this widget becomes the
        // sole content of the welcome step (the textual blurb in
        // WelcomePage.ui's mainText is hidden by our WelcomePage patch).
        // Hide the explanation paragraph and let the productWelcome image
        // fill the entire allocated space — no inner margins, no caption.
        m_explanation->hide();

        // The constructor seeded mainLayout with two QSpacerItems sized for
        // the QListView path: an addSpacing(defaultFontHeight/2) under the
        // explanation row to breathe before the list, and a trailing
        // addStretch() to push the list contents up. With the explanation
        // hidden and the list deleted, both spacers are just dead vertical
        // padding around the productWelcome image — strip them all in one
        // pass so the image label inherits the full layout area.
        for ( int i = m_centralLayout->count() - 1; i >= 0; --i )
        {
            QLayoutItem* item = m_centralLayout->itemAt( i );
            if ( item && item->spacerItem() )
            {
                m_centralLayout->takeAt( i );
                delete item;
            }
        }

        // Also zero the layout's own contentsMargins (Qt defaults to roughly
        // 11 px on each side via the style metrics). The welcome step's
        // outer layout already provides whatever framing chrome is needed —
        // an extra 11 px inset here pushes the image away from every edge.
        m_centralLayout->setContentsMargins( 0, 0, 0, 0 );

        if ( !Calamares::Branding::instance()->imagePath( Calamares::Branding::ProductWelcome ).isEmpty() )
        {
            QPixmap theImage
                = QPixmap( Calamares::Branding::instance()->imagePath( Calamares::Branding::ProductWelcome ) );
            if ( !theImage.isNull() )
            {
                // Always use FixedAspectRatioLabel so the pixmap scales to
                // fill the label with KeepAspectRatio (the previous
                // welcomeExpandingLogo branding flag gated this; we make it
                // unconditional because a non-scaling QLabel at native
                // pixmap size makes any "full image" branding overflow).
                auto* imageLabel = new FixedAspectRatioLabel;
                imageLabel->setPixmap( theImage );
                imageLabel->setContentsMargins( 0, 0, 0, 0 );
                imageLabel->setAlignment( Qt::AlignCenter );
                imageLabel->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
                imageLabel->setObjectName( "welcomeLogo" );
                // This specifically isn't assigned to m_centralWidget
                m_centralLayout->addWidget( imageLabel );
            }
        }
    }
}
