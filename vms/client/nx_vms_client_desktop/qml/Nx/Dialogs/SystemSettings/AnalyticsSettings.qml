// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx
import Nx.Core
import Nx.Items
import Nx.Utils
import Nx.Controls
import Nx.Controls.NavigationMenu
import Nx.InteractiveSettings

import nx.vms.client.desktop

Item
{
    id: analyticsSettings

    property var store: null
    property var engineLicenseSummaryProvider: null
    property var saasServiceManager: null
    property alias viewModel: viewModel

    function activateEngine(engineId) { viewModel.setCurrentEngine(engineId) }

    AnalyticsSettingsViewModel
    {
        id: viewModel

        requestsModel: store ? store.makeApiIntegrationRequestsModel() : null

        onEngineRequested: (engineId) =>
        {
            if (store)
                store.setCurrentEngineId(engineId ?? NxGlobals.uuid(""))
        }

        function storeUpdated()
        {
            const engineId = store.getCurrentEngineId()
            const licenseSummary = engineLicenseSummaryProvider.licenseSummary(engineId)
            viewModel.updateState(
                store.analyticsEngines,
                licenseSummary,
                store.settingsModel(engineId),
                store.settingsValues(engineId),
                store.errors(engineId))
        }

        Connections
        {
            target: analyticsSettings.store

            function onAnalyticsEnginesChanged() { viewModel.storeUpdated() }
            function onCurrentSettingsStateChanged() { viewModel.storeUpdated() }
            function onCurrentErrorsChanged() { viewModel.storeUpdated() }
            function onLicenseSummariesChanged() { viewModel.storeUpdated() }
        }
    }

    Component
    {
        id: analyticsSettingsMenu
        AnalyticsSettingsMenu { viewModel: analyticsSettings.viewModel }
    }

    Component
    {
        id: integrationsMenu
        IntegrationsMenu { viewModel: analyticsSettings.viewModel }
    }

    Loader
    {
        id: menu
        width: 240
        height: parent.height - saasBanner.height

        sourceComponent: LocalSettings.iniConfigValue("integrationsManagement")
            ? integrationsMenu
            : analyticsSettingsMenu
    }

    Item
    {
        id: scrollBarParent

        width: 8

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
    }

    AnalyticsSettingsView
    {
        id: settings

        viewModel: viewModel
        enabled: !saasBanner.visible

        anchors.top: parent.top
        anchors.bottom: saasBanner.top
        anchors.left: menu.right
        anchors.right: scrollBarParent.left
        anchors.margins: 16

        settingsView
        {
            enabled: !!store && !store.loading
            scrollBarParent: scrollBarParent

            requestParametersFunction: (model) => { return store.requestParameters(model) }

            onValuesEdited: (activeItem, parameters) =>
            {
                store.setSettingsValues(
                    viewModel.currentEngineId,
                    activeItem ? activeItem.name : "",
                    settings.settingsView.getValues(),
                    parameters)
            }
        }

        settingsViewHeader
        {
            checkable: false
            refreshable: false
            removable: !!viewModel.currentRequestId
            streamSelectorVisible: false
        }

        settingsViewPlaceholder
        {
            header: qsTr("This plugin has no settings at the System level.")
            description: qsTr("Check Camera Settings to configure this plugin.")
            loading: !!store && store.loading
        }

        Binding
        {
            target: viewModel.requestsModel
            property: "isActive"
            value: settings.visible
        }
    }

    SaasBanner
    {
        id: saasBanner

        saasServiceManager: analyticsSettings.saasServiceManager
        licenseSummary: viewModel.currentEngineLicenseSummary
        deviceSpecific: false

        height: visible ? implicitHeight : 0
        anchors.bottom: parent.bottom
        width: parent.width
    }

    onStoreChanged: store.setCurrentEngineId(viewModel.currentEngineId ?? NxGlobals.uuid(""))
}
