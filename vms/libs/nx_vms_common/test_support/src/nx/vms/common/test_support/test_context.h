// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <nx/core/access/access_types.h>
#include <nx/utils/impl_ptr.h>

#include "resource/resource_pool_test_helper.h"

class QnStaticCommonModule;
class QnCommonModule;

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::common::test {

class MessageProcessorMock;

class NX_VMS_COMMON_TEST_SUPPORT_API Context
{
public:
    Context(
        bool clientMode = false,
        nx::core::access::Mode resourceAccessMode = nx::core::access::Mode::direct);
    virtual ~Context();

    QnStaticCommonModule* staticCommonModule() const;
    QnCommonModule* commonModule() const;
    SystemContext* systemContext() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

class NX_VMS_COMMON_TEST_SUPPORT_API ContextBasedTest:
    public ::testing::Test,
    protected QnResourcePoolTestHelper
{
public:
    ContextBasedTest(
        bool clientMode = false,
        nx::core::access::Mode resourceAccessMode = nx::core::access::Mode::direct);

    ~ContextBasedTest();

    Context* context() const { return m_context.get(); }

    SystemContext* systemContext() const { return m_context->systemContext(); }

    MessageProcessorMock* createMessageProcessor();

private:
    std::unique_ptr<Context> m_context;
};

} // namespace nx::vms::common::test
