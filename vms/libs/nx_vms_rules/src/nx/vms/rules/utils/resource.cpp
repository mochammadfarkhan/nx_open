// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

#include "../aggregated_event.h"
#include "../basic_action.h"
#include "../field_types.h"
#include "field.h"

namespace nx::vms::rules::utils {

QnUserResourceSet users(
    const UuidSelection& selection,
    const common::SystemContext* context,
    bool activeOnly)
{
    auto result = selection.all
        ? nx::utils::toQSet(context->resourcePool()->getResources<QnUserResource>())
        : nx::vms::common::allUsers(context, selection.ids);

    result.remove({});

    if (activeOnly)
        erase_if(result, [](const auto& user) { return !user->isEnabled(); });

    return result;
}

UuidSet userIds(
    const UuidSelection& selection, const common::SystemContext* context, bool activeOnly)
{
    UuidSet result;
    for (const auto& user: users(selection, context, activeOnly))
        result.insert(user->getId());

    return result;
}

bool isUserSelected(
    const UuidSelection& selection,
    const common::SystemContext* context,
    nx::Uuid userId)
{
    if (selection.all)
        return true;

    if (selection.ids.contains(userId))
        return true;

    for (const auto& user: nx::vms::common::allUsers(context, selection.ids))
    {
        if (user->getId() == userId)
            return true;
    };

    return false;
}

QnMediaServerResourceList servers(
    const UuidSelection& selection,
    const common::SystemContext* context)
{
    return selection.all
        ? context->resourcePool()->getResources<QnMediaServerResource>()
        : context->resourcePool()->getResourcesByIds<QnMediaServerResource>(selection.ids);
}

QnVirtualCameraResourceList cameras(
    const UuidSelection& selection,
    const common::SystemContext* context)
{
    return selection.all
        ? context->resourcePool()->getResources<QnVirtualCameraResource>()
        : context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(selection.ids);
}

UuidList getResourceIds(const QObject* entity, std::string_view fieldName)
{
    const auto value = entity->property(fieldName.data());

    if (!value.isValid())
        return {};

    UuidList result;

    if (value.canConvert<UuidList>())
        result = value.value<UuidList>();
    else if (value.canConvert<nx::Uuid>())
        result.emplace_back(value.value<nx::Uuid>());

    result.removeAll({});

    return result;
}


UuidList getDeviceIds(const AggregatedEventPtr& event)
{
    UuidList result;
    result << getFieldValue<nx::Uuid>(event, utils::kCameraIdFieldName);
    result << getFieldValue<UuidList>(event, utils::kDeviceIdsFieldName);
    result.removeAll(nx::Uuid());

    return result;
}

UuidList getResourceIds(const AggregatedEventPtr& event)
{
    auto result = getDeviceIds(event);
    result << getFieldValue<nx::Uuid>(event, kServerIdFieldName);
    result << getFieldValue<nx::Uuid>(event, kEngineIdFieldName);
    result.removeAll(nx::Uuid());

    return result;
}

UuidList getResourceIds(const ActionPtr& action)
{
    UuidList result;
    result << getFieldValue<nx::Uuid>(action, kCameraIdFieldName);
    result << getFieldValue<UuidList>(action, kDeviceIdsFieldName);
    result << getFieldValue<nx::Uuid>(action, kServerIdFieldName);
    result.removeAll(nx::Uuid());

    return result;
}

} // namespace nx::vms::rules::utils
