// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_rules_dialog.h"

#include <nx/vms/api/rules/rule.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/dialogs/week_time_schedule_dialog.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/unique_id_field.h>
#include <nx/vms/rules/events/generic_event.h>
#include <nx/vms/rules/events/server_started_event.h>
#include <nx/vms/rules/utils/api.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_vms_rules_manager.h>

#include "edit_vms_rule_dialog.h"
#include "model_view/rules_table_model.h"
#include "utils/confirmation_dialogs.h"

namespace nx::vms::client::desktop::rules {

VmsRulesDialog::VmsRulesDialog(QWidget* parent):
    QmlDialogWrapper(
        appContext()->qmlEngine(),
        QUrl("Nx/VmsRules/VmsRulesDialog.qml"),
        /*initialProperties*/ {},
        parent),
    QnSessionAwareDelegate(parent),
    m_parentWidget{parent},
    m_rulesTableModel{QmlProperty<RulesTableModel*>(rootObjectHolder(), "rulesTableModel")}
{
    QmlProperty<QObject*>(rootObjectHolder(), "dialog") = this;
}

VmsRulesDialog::~VmsRulesDialog()
{
    QmlProperty<QObject*>(rootObjectHolder(), "dialog") = nullptr;
}

void VmsRulesDialog::setError(const QString& error)
{
    QmlProperty<QString>(rootObjectHolder(), "errorString") = error;
}

void VmsRulesDialog::setFilter(const QString& filter)
{
    QmlProperty<QString>(rootObjectHolder(), "filterText") = filter;
}

void VmsRulesDialog::addRule()
{
    auto engine = systemContext()->vmsRulesEngine();

    auto newRule = std::make_shared<vms::rules::Rule>(nx::Uuid::createUuid(), engine);
    EditVmsRuleDialog editVmsRuleDialog{m_parentWidget};

    editVmsRuleDialog.setRule(newRule, /*isNewRule*/ true);
    connect(&editVmsRuleDialog, &EditVmsRuleDialog::accepted, [&]() { saveRuleImpl(newRule); });

    if (editVmsRuleDialog.exec() == QDialogButtonBox::Cancel)
        return;

    saveRuleImpl(newRule);
}

void VmsRulesDialog::editSchedule(const UuidList& ids)
{
    if (!NX_ASSERT(!ids.empty()))
        return;

    auto engine = systemContext()->vmsRulesEngine();

    QVector<QByteArray> schedules;
    for (auto id: ids)
        schedules.push_back(engine->rule(id)->schedule());

    WeekTimeScheduleDialog dialog(m_parentWidget, /*isEmptyAllowed*/ false);
    dialog.setSchedules(schedules);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const auto schedule = dialog.schedule();
    for (auto id: ids)
    {
        auto clone = engine->cloneRule(id);
        if (!NX_ASSERT(clone))
            return;

        clone->setSchedule(schedule);

        saveRuleImpl(clone);
    }
}

void VmsRulesDialog::duplicateRule(nx::Uuid id)
{
    auto engine = systemContext()->vmsRulesEngine();

    auto clone = engine->cloneRule(id);
    if (!NX_ASSERT(clone))
        return;

    clone->setId(nx::Uuid::createUuid()); //< Change id to prevent rule override on save request.
    if (auto uniqueIdField = clone->eventFilters().at(0)->fieldByType<vms::rules::UniqueIdField>())
        uniqueIdField->setId(nx::Uuid::createUuid()); //< Fix field uniqueness after cloning. TODO: #mmalofeev fix this workaround.

    EditVmsRuleDialog editVmsRuleDialog(m_parentWidget);

    editVmsRuleDialog.setRule(clone, /*isNewRule*/ false);
    connect(&editVmsRuleDialog, &EditVmsRuleDialog::accepted, [&]() { saveRuleImpl(clone); });

    if (editVmsRuleDialog.exec() == QDialogButtonBox::Cancel)
        return;

    saveRuleImpl(clone);
}

void VmsRulesDialog::editRule(nx::Uuid id)
{
    auto engine = systemContext()->vmsRulesEngine();

    auto clone = engine->cloneRule(id);
    if (!NX_ASSERT(clone))
        return;

    EditVmsRuleDialog editVmsRuleDialog(m_parentWidget);

    editVmsRuleDialog.setRule(clone, /*isNewRule*/ false);
    connect(&editVmsRuleDialog, &EditVmsRuleDialog::accepted, [&]() { saveRuleImpl(clone); });

    const auto result = editVmsRuleDialog.exec();
    if (result == QDialogButtonBox::Cancel)
        return;

    if (result == QDialogButtonBox::Reset)
    {
        // QDialogButtonBox::Reset means user requested to delete the rule.
        deleteRulesImpl({id});
        return;
    }

    saveRuleImpl(clone);
}

void VmsRulesDialog::deleteRules(const UuidList& ids)
{
    if (ConfirmationDialogs::confirmDelete(m_parentWidget, ids.size()))
        deleteRulesImpl(ids);
}

void VmsRulesDialog::setRulesState(const UuidList& ids, bool isEnabled)
{
    auto engine = systemContext()->vmsRulesEngine();

    for (auto id: ids)
    {
        auto clone = engine->cloneRule(id);
        if (!NX_ASSERT(clone))
            return;

        clone->setEnabled(isEnabled);

        saveRuleImpl(clone);
    }
}

void VmsRulesDialog::resetToDefaults()
{
    if (ConfirmationDialogs::confirmReset(m_parentWidget))
        resetToDefaultsImpl();
}

void VmsRulesDialog::openEventLogDialog()
{
    action(menu::OpenEventLogAction)->trigger();
}

bool VmsRulesDialog::tryClose(bool /*force*/)
{
    reject();

    return true;
}

void VmsRulesDialog::deleteRulesImpl(const UuidList& ids)
{
    auto connection = messageBusConnection();
    if (!connection)
        return;

    const auto rulesManager = connection->getVmsRulesManager(nx::network::rest::kSystemSession);
    for (const auto& id: ids)
    {
        rulesManager->deleteRule(
        id,
        [this](int /*requestId*/, ec2::ErrorCode errorCode)
        {
            if (errorCode != ec2::ErrorCode::ok) // TODO: #mmalofeev add error to some list.
                setError(tr("Delete rule error:") + " " + ec2::toString(errorCode));
        },
        this);
    }
}

void VmsRulesDialog::saveRuleImpl(const std::shared_ptr<vms::rules::Rule>& rule)
{
    auto connection = messageBusConnection();
    if (!connection)
        return;

    const auto serializedRule = serialize(rule.get());
    const auto rulesManager = connection->getVmsRulesManager(nx::network::rest::kSystemSession);
    rulesManager->save(
        serializedRule,
        [this](int /*requestId*/, ec2::ErrorCode errorCode)
        {
            if (errorCode != ec2::ErrorCode::ok)
                setError(tr("Save rule error:") + " " + ec2::toString(errorCode));
        },
        this);
}

void VmsRulesDialog::resetToDefaultsImpl()
{
    auto connection = messageBusConnection();
    if (!connection)
        return;

    connection->getVmsRulesManager(nx::network::rest::kSystemSession)->resetVmsRules(
        /*useDefault*/ true,
        [this](int /*requestId*/, ec2::ErrorCode errorCode)
        {
            if (errorCode != ec2::ErrorCode::ok)
                setError(tr("Reset to defaults error:") + " " + ec2::toString(errorCode));
        },
        this);
}

} // namespace nx::vms::client::desktop::rules
