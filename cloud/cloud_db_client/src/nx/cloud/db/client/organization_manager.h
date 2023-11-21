// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "async_http_requests_executor.h"
#include "include/nx/cloud/db/api/organization_manager.h"

namespace nx::cloud::db::client {

class OrganizationManager:
    public api::OrganizationManager
{
public:
    OrganizationManager(AsyncRequestsExecutor* requestsExecutor);

    virtual void bindSystem(
        const std::string& organizationId,
        api::SystemRegistrationData registrationData,
        std::function<void(api::ResultCode, api::SystemData)> completionHandler) override;

private:
    AsyncRequestsExecutor* m_requestsExecutor = nullptr;
};

} // namespace nx::cloud::db::api
