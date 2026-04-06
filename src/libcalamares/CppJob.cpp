/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2016 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "CppJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"

namespace Calamares
{

CppJob::CppJob( QObject* parent )
    : Job( parent )
{
}


CppJob::~CppJob() {}


void
CppJob::setModuleInstanceKey( const Calamares::ModuleSystem::InstanceKey& instanceKey )
{
    m_instanceKey = instanceKey;
}


void
CppJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    Q_UNUSED( configurationMap )
}

QVariantMap
CppJob::mergedConfiguration( const QVariantMap& base ) const
{
    auto* gs = Calamares::JobQueue::instance()->globalStorage();
    if ( !gs )
        return base;

    const auto overrides = gs->value( QStringLiteral( "moduleConfigOverrides" ) ).toMap()
                               .value( m_instanceKey.module() )
                               .toMap();
    if ( overrides.isEmpty() )
        return base;

    QVariantMap merged = base;
    for ( auto it = overrides.cbegin(); it != overrides.cend(); ++it )
        merged[ it.key() ] = it.value();
    return merged;
}

}  // namespace Calamares
