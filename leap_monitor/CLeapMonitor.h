#pragma once

class CLeapMonitor;

class CLeapListener final : public Leap::Listener
{
    CLeapMonitor* m_monitor = nullptr;
    std::mutex m_monitorMutex;

    virtual void onInit(const Leap::Controller &controller);
    virtual void onConnect(const Leap::Controller &controller);
    virtual void onDisconnect(const Leap::Controller &controller);
    virtual void onServiceConnect(const Leap::Controller &controller);
    virtual void onServiceDisconnect(const Leap::Controller &controller);
    virtual void onLogMessage(const Leap::Controller &controller, Leap::MessageSeverity severity, int64_t timestamp, const char *msg); // Async
public:
    void SetMonitor(CLeapMonitor *f_monitor); // Async method
};

class CLeapMonitor final
{
    std::atomic<bool> m_initialized;
    vr::IVRSystem *m_vrSystem;
    vr::IVRDebug *m_vrDebug;
    vr::IVRApplications *m_vrApplications;
    vr::IVROverlay *m_vrOverlay;
    vr::IVRNotifications *m_vrNotifications;
    vr::VROverlayHandle_t m_overlayHandle;
    vr::VRNotificationId m_notificationID;
    std::mutex m_notificationLock;
    std::set<uint32_t> m_leapDevices;

    Leap::Controller *m_leapController;
    CLeapListener m_leapListener;

    enum GameProfile : unsigned char
    {
        GP_Default = 0U,
        GP_VRChat
    } m_gameProfile;
    bool m_specialCombinationState;

    void AddTrackedDevice(uint32_t unTrackedDeviceIndex);
    void RemoveTrackedDevice(uint32_t unTrackedDeviceIndex);
    void UpdateGameProfile(const char *f_appKey);
    void SendCommand(const char *f_cmd);

    CLeapMonitor(const CLeapMonitor &that) = delete;
    CLeapMonitor& operator=(const CLeapMonitor &that) = delete;
public:
    CLeapMonitor();
    ~CLeapMonitor();

    bool Init();
    void Run();
    void Terminate();

    void SendNotification(const std::string &f_text); // Async method
};