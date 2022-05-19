// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "user_group_model.h"

namespace nx::vms::api {

/**%apidoc User role information object.
 * %param[unused] parentRoleIds
 * %param[unused] type
 * %// Deprecated, used in /rest/v1/userRoles only, use UserGroupModel with /rest/v{2-}/userGroups
 * %// instead.
 */
struct UserRoleModel: UserGroupModel
{
    bool operator==(const UserRoleModel& other) const = default;
};

} // namespace nx::vms::api
