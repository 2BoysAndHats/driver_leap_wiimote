#include "stdafx.h"

#include "CLeapMonitor.h"

#include "Utils.h"

void CLeapListener::SetMonitor(CLeapMonitor *f_monitor)
{
    m_monitorMutex.lock();
    m_monitor = f_monitor;
    m_monitorMutex.unlock();
}
void CLeapListener::onInit(const Leap::Controller &controller)
{
}
void CLeapListener::onConnect(const Leap::Controller &controller)
{
    m_monitorMutex.lock();
    if(m_monitor)
    {
        std::string l_message("Controller connected");
        m_monitor->SendNotification(l_message);
    }
    m_monitorMutex.unlock();
}
void CLeapListener::onDisconnect(const Leap::Controller &controller)
{
    m_monitorMutex.lock();
    if(m_monitor)
    {
        std::string l_message("Controller disconnected");
        m_monitor->SendNotification(l_message);
    }
    m_monitorMutex.unlock();
}
void CLeapListener::onServiceConnect(const Leap::Controller &controller)
{
    m_monitorMutex.lock();
    if(m_monitor)
    {
        std::string l_message("Service connected");
        m_monitor->SendNotification(l_message);
    }
    m_monitorMutex.unlock();
}
void CLeapListener::onServiceDisconnect(const Leap::Controller &controller)
{
    m_monitorMutex.lock();
    if(m_monitor)
    {
        std::string l_message("Service disconnected");
        m_monitor->SendNotification(l_message);
    }
    m_monitorMutex.unlock();
}
void CLeapListener::onLogMessage(const Leap::Controller &controller, Leap::MessageSeverity severity, int64_t timestamp, const char *msg)
{
    if(severity <= Leap::MESSAGE_CRITICAL)
    {
        m_monitorMutex.lock();
        if(m_monitor)
        {
            std::string l_message(msg);
            m_monitor->SendNotification(l_message);
        }
        m_monitorMutex.unlock();
    }
}

// ----
const std::vector<std::string> g_steamAppKeys
{
    "steam.app.438100" // VRChat
};
enum SteamAppID : size_t
{
    SAI_VRChat = 0U
};

const std::string g_profileName[2]
{
    "Default", "VRChat"
};

CLeapMonitor::CLeapMonitor()
{
    m_initialized = false;
    m_vrSystem = nullptr;
    m_vrDebug = nullptr;
    m_vrApplications = nullptr;
    m_vrOverlay = nullptr;
    m_vrNotifications = nullptr;
    m_notificationID = 0U;
    m_leapController = nullptr;
    m_gameProfile = GP_Default;
    m_specialCombinationState = false;
}
CLeapMonitor::~CLeapMonitor()
{
}

bool CLeapMonitor::Init()
{
    if(!m_initialized)
    {
        vr::EVRInitError eVRInitError;
        m_vrSystem = vr::VR_Init(&eVRInitError, vr::VRApplication_Background);
        if(eVRInitError == vr::VRInitError_None)
        {
            m_vrDebug = vr::VRDebug();
            m_vrApplications = vr::VRApplications();
            m_vrOverlay = vr::VROverlay();
            m_vrOverlay->CreateOverlay("leap_monitor_overlay", "Leap Motion Monitor", &m_overlayHandle);
            m_vrNotifications = vr::VRNotifications();

            for(uint32_t i = 0U; i < vr::k_unMaxTrackedDeviceCount; i++) AddTrackedDevice(i);

            m_leapController = new Leap::Controller();
            m_leapController->addListener(m_leapListener);
            m_leapListener.SetMonitor(this);

            m_initialized = true;
        }
    }
    return m_initialized;
}

void CLeapMonitor::Run()
{
    if(m_initialized)
    {
        const std::chrono::milliseconds l_monitorInterval(100U);
        bool l_quitEvent = false;

        while(!l_quitEvent)
        {
            // System messages
            MSG msg = { 0 };
            while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            if(msg.message == WM_QUIT) break;

            // VR messages
            vr::VREvent_t l_event;
            while(vr::VRSystem()->PollNextEvent(&l_event, sizeof(vr::VREvent_t)))
            {
                switch(l_event.eventType)
                {
                    case vr::VREvent_Quit:
                        l_quitEvent = true;
                        break;
                    case vr::VREvent_TrackedDeviceActivated:
                        AddTrackedDevice(l_event.trackedDeviceIndex);
                        break;
                    case vr::VREvent_TrackedDeviceDeactivated:
                        RemoveTrackedDevice(l_event.trackedDeviceIndex);
                        break;
                    case vr::VREvent_SceneApplicationStateChanged:
                    {
                        vr::EVRSceneApplicationState l_appState = m_vrApplications->GetSceneApplicationState();
                        switch(l_appState)
                        {
                            case vr::EVRSceneApplicationState_Starting:
                            {
                                char l_appKey[vr::k_unMaxApplicationKeyLength];
                                vr::EVRApplicationError l_appError = m_vrApplications->GetStartingApplication(l_appKey, vr::k_unMaxApplicationKeyLength);
                                if(l_appError == vr::VRApplicationError_None) UpdateGameProfile(l_appKey);
                            } break;
                            case vr::EVRSceneApplicationState_None:
                                UpdateGameProfile(""); // Revert to default
                                break;
                        }
                    } break;
                }
                if(l_quitEvent) break;
            }

            // Process special combinations if NumLock is active
            if((GetKeyState(VK_NUMLOCK) & 0xFFFF) != 0)
            {
                bool l_combinationState = ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(0x58) & 0x8000)); // Ctrl+X
                if(m_specialCombinationState != l_combinationState)
                {
                    m_specialCombinationState = l_combinationState;
                    if(m_specialCombinationState)
                    {
                        SendCommand("game vrchat drawing_mode");
                        std::string l_message("VRChat drawing mode toggled");
                        SendNotification(l_message);
                    }
                }
            }

            std::this_thread::sleep_for(l_monitorInterval);
        }
    }
}

void CLeapMonitor::Terminate()
{
    if(m_initialized)
    {
        m_initialized = false;

        m_leapListener.SetMonitor(nullptr);
        m_leapController->removeListener(m_leapListener);
        delete m_leapController;

        if(m_notificationID) m_vrNotifications->RemoveNotification(m_notificationID);
        m_vrOverlay->DestroyOverlay(m_overlayHandle);

        vr::VR_Shutdown();

        m_vrSystem = nullptr;
        m_vrDebug = nullptr;
        m_vrApplications = nullptr;
        m_vrOverlay = nullptr;
        m_vrNotifications = nullptr;
        m_notificationID = 0U;
        m_leapController = nullptr;
        m_gameProfile = GP_Default;
        m_specialCombinationState = false;
    }
}

void CLeapMonitor::SendNotification(const std::string &f_text)
{
    if(m_initialized)
    {
        if(!f_text.empty())
        {
            m_notificationLock.lock();
            if(m_notificationID) m_vrNotifications->RemoveNotification(m_notificationID);
            m_vrNotifications->CreateNotification(m_overlayHandle, 500U, vr::EVRNotificationType_Transient, f_text.c_str(), vr::EVRNotificationStyle_None, nullptr, &m_notificationID);
            m_notificationLock.unlock();
        }
    }
}

void CLeapMonitor::AddTrackedDevice(uint32_t unTrackedDeviceIndex)
{
    if(m_vrSystem->GetUint64TrackedDeviceProperty(unTrackedDeviceIndex, vr::Prop_VendorSpecific_Reserved_Start) == 0x1EA8U) m_leapDevices.insert(unTrackedDeviceIndex);
}
void CLeapMonitor::RemoveTrackedDevice(uint32_t unTrackedDeviceIndex)
{
    auto l_searchIter = m_leapDevices.find(unTrackedDeviceIndex);
    if(l_searchIter != m_leapDevices.end()) m_leapDevices.erase(l_searchIter);
}

void CLeapMonitor::UpdateGameProfile(const char *f_appKey)
{
    std::string l_appString(f_appKey);
    GameProfile l_newProfile;
    switch(ReadEnumVector(l_appString, g_steamAppKeys))
    {
        case SAI_VRChat:
            l_newProfile = GP_VRChat;
            break;
        default:
            l_newProfile = GP_Default;
            break;
    }
    if(m_gameProfile != l_newProfile)
    {
        m_gameProfile = l_newProfile;

        if(!m_leapDevices.empty())
        {
            char l_response[32U];
            std::string l_data("profile ");
            l_data.append(g_profileName[m_gameProfile]);

            for(auto l_device : m_leapDevices) m_vrDebug->DriverDebugRequest(l_device, l_data.c_str(), l_response, 32U);
        }

        std::string l_notifyText("Game profile has been changed to '");
        l_notifyText.append(g_profileName[m_gameProfile]);
        l_notifyText.push_back('\'');
        SendNotification(l_notifyText);
    }
}

void CLeapMonitor::SendCommand(const char *f_char)
{
    char l_response[32U];
    for(auto l_device : m_leapDevices) m_vrDebug->DriverDebugRequest(l_device, f_char, l_response, 32U);
}
