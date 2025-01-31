/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "GradientSignal_precompiled.h"
#include "ReferenceGradientComponent.h"
#include <AzCore/Debug/Profiler.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace GradientSignal
{
    void ReferenceGradientConfig::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context);
        if (serialize)
        {
            serialize->Class<ReferenceGradientConfig, AZ::ComponentConfig>()
                ->Version(0)
                ->Field("Gradient", &ReferenceGradientConfig::m_gradientSampler)
                ;

            AZ::EditContext* edit = serialize->GetEditContext();
            if (edit)
            {
                edit->Class<ReferenceGradientConfig>(
                    "Reference Gradient", "")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &ReferenceGradientConfig::m_gradientSampler, "Gradient", "Input gradient whose values will be transformed in relation to threshold.")
                    ;
            }
        }

        if (auto behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<ReferenceGradientConfig>()
                ->Attribute(AZ::Script::Attributes::Category, "Vegetation")
                ->Constructor()
                ->Property("gradientSampler", BehaviorValueProperty(&ReferenceGradientConfig::m_gradientSampler))
                ;
        }
    }

    void ReferenceGradientComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& services)
    {
        services.push_back(AZ_CRC("GradientService", 0x21c18d23));
    }

    void ReferenceGradientComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& services)
    {
        services.push_back(AZ_CRC("GradientService", 0x21c18d23));
        services.push_back(AZ_CRC("GradientTransformService", 0x8c8c5ecc));
    }

    void ReferenceGradientComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& services)
    {
    }

    void ReferenceGradientComponent::Reflect(AZ::ReflectContext* context)
    {
        ReferenceGradientConfig::Reflect(context);

        AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context);
        if (serialize)
        {
            serialize->Class<ReferenceGradientComponent, AZ::Component>()
                ->Version(0)
                ->Field("Configuration", &ReferenceGradientComponent::m_configuration)
                ;
        }

        if (auto behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Constant("ReferenceGradientComponentTypeId", BehaviorConstant(ReferenceGradientComponentTypeId));

            behaviorContext->Class<ReferenceGradientComponent>()->RequestBus("ReferenceGradientRequestBus");

            behaviorContext->EBus<ReferenceGradientRequestBus>("ReferenceGradientRequestBus")
                ->Attribute(AZ::Script::Attributes::Category, "Vegetation")
                ->Event("GetGradientSampler", &ReferenceGradientRequestBus::Events::GetGradientSampler)
                ;
        }
    }

    ReferenceGradientComponent::ReferenceGradientComponent(const ReferenceGradientConfig& configuration)
        : m_configuration(configuration)
    {
    }

    void ReferenceGradientComponent::Activate()
    {
        m_dependencyMonitor.Reset();
        m_dependencyMonitor.ConnectOwner(GetEntityId());
        m_dependencyMonitor.ConnectDependency(m_configuration.m_gradientSampler.m_gradientId);
        GradientRequestBus::Handler::BusConnect(GetEntityId());
        ReferenceGradientRequestBus::Handler::BusConnect(GetEntityId());
    }

    void ReferenceGradientComponent::Deactivate()
    {
        m_dependencyMonitor.Reset();
        GradientRequestBus::Handler::BusDisconnect();
        ReferenceGradientRequestBus::Handler::BusDisconnect();
    }

    bool ReferenceGradientComponent::ReadInConfig(const AZ::ComponentConfig* baseConfig)
    {
        if (auto config = azrtti_cast<const ReferenceGradientConfig*>(baseConfig))
        {
            m_configuration = *config;
            return true;
        }
        return false;
    }

    bool ReferenceGradientComponent::WriteOutConfig(AZ::ComponentConfig* outBaseConfig) const
    {
        if (auto config = azrtti_cast<ReferenceGradientConfig*>(outBaseConfig))
        {
            *config = m_configuration;
            return true;
        }
        return false;
    }

    float ReferenceGradientComponent::GetValue(const GradientSampleParams& sampleParams) const
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::Entity);

        float output = 0.0f;

        output = m_configuration.m_gradientSampler.GetValue(sampleParams);

        return output;
    }

    bool ReferenceGradientComponent::IsEntityInHierarchy(const AZ::EntityId& entityId) const
    {
        return m_configuration.m_gradientSampler.IsEntityInHierarchy(entityId);
    }

    GradientSampler& ReferenceGradientComponent::GetGradientSampler()
    {
        return m_configuration.m_gradientSampler;
    }
}
