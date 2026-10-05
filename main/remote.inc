// One complete state lets the phone follow changes made on either screen.
struct RemoteState {
    char records[24][21];
    unsigned count;
};
RemoteState publishedState = {};
uint32_t stateRevision = 0;
bool statePublished = false;
uint64_t nextStatePublish = 0;
uint64_t sensorWatchUntil = 0;

const char *pageName() {
    const char *names[] = {"TRAINING", "SETTINGS", "DEBUG", "DEBUGZONE"};
    return names[static_cast<unsigned>(currentPage)];
}

const char *effectName() {
    const char *names[] = {"NONE", "CONFETTI", "ORBIT", "BOUNCE", "WARP"};
    return names[static_cast<unsigned>(celebration)];
}

RemoteState collectState() {
    RemoteState state = {};
    unsigned index = 0;
    auto add = [&](const char *format, ...) {
        va_list args;
        va_start(args, format);
        std::vsnprintf(state.records[index++], 21, format, args);
        va_end(args);
    };
    add("PROTO:2");
    add("MODE:%s", modeName(session.mode));
    add("SHOTS:%lu", static_cast<unsigned long>(session.shots));
    add("LEFT:%u", session.remaining());
    add("TOTAL:%lu", static_cast<unsigned long>(session.total));
    add("LAST:%d", session.hasShot ? session.lastScore : -1);
    add("DIST:%d", settings.distanceMillimeters);
    add("UNIT:%s", settings.unit == training::Unit::Meters ? "M" : "FT");
    add("CAL:%u", settings.sensitivity);
    add("PAGE:%s", pageName());
    add("FULL:%d", fullscreen);
    add("THEME:%s", darkMode ? "DARK" : "LIGHT");
    const char *views[] = {"PAYLOAD", "HEX", "BINARY"};
    add("VIEW:%s", views[static_cast<unsigned>(packetView)]);
    add("FX:%s", effectName());
    add("SOUND:%d", hardware::speakerTestActive());
    add("MENU:%d", menuOpen);
    add("PKTA:%.10s", lastPacket);
    add("PKTB:%s", lastPacket + std::min<size_t>(10, std::strlen(lastPacket)));
    const char *deliveries[] = {"EXAMPLE", "OFFLINE", "UNSUBSCRIBED", "QUEUED", "FAILED"};
    add("DEL:%s", deliveries[static_cast<unsigned>(delivery)]);
    // Only a visible phone Debug Zone asks for live sensor readings.
    if (hardware::nowMs() < sensorWatchUntil) {
        add("TLOW:%d", sensors::temperatureTenths(sensors::regulator33));
        add("THIGH:%d", sensors::temperatureTenths(sensors::regulator5));
        add("ADC:%d", sensors::input.raw);
        add("MV:%d", sensors::input.millivolts);
        add("DETECT:%d", sensors::input.raw >= static_cast<int>(settings.sensitivity));
    }
    state.count = index;
    return state;
}

// BEGIN and END stop the app from showing half of an update.
bool sendState(bool force) {
    if (!bluetooth::notificationsEnabled()) {
        statePublished = false;
        return false;
    }
    // Leave room for commands while sending live readings about three times a second.
    if (!force && hardware::nowMs() < nextStatePublish)
        return true;
    const RemoteState state = collectState();
    if (!force && statePublished && std::memcmp(&state, &publishedState, sizeof(state)) == 0)
        return true;
    if (!bluetooth::notificationRoom(28))
        return false;
    char packet[21];
    std::snprintf(packet, sizeof(packet), "BEGIN:%lu", static_cast<unsigned long>(++stateRevision));
    bool queued = bluetooth::notify(packet);
    for (unsigned i = 0; i < state.count; ++i)
        queued = bluetooth::notify(state.records[i]) && queued;
    std::snprintf(packet, sizeof(packet), "END:%lu", static_cast<unsigned long>(stateRevision));
    queued = bluetooth::notify(packet) && queued;
    statePublished = queued;
    if (queued) {
        publishedState = state;
        nextStatePublish = hardware::nowMs() + 300;
    }
    return queued;
}

void navigate(Page page) {
    hardware::stopSpeakerSequence();
    celebration = Celebration::None;
    currentPage = page;
    fullscreen = false;
    animateMenu(false);
    dirty = true;
}

// Use the same score and setting functions for touch and phone commands.
const char *applyCommand(const char *command) {
    unsigned value = 0;
    if (std::strcmp(command, "PING") == 0)
        bluetooth::notify("{\"pong\":1}");
    else if (std::strcmp(command, "STATE") == 0) {
        // The reply below includes a fresh state.
        return nullptr;
    } else if (std::strcmp(command, "WATCH:1") == 0 || std::strcmp(command, "WATCH:0") == 0) {
        // A short lease also stops updates if the app is killed or suspended.
        sensorWatchUntil = command[6] == '1' ? hardware::nowMs() + 10000 : 0;
        return nullptr;
    } else if (std::strcmp(command, "RESET") == 0)
        resetSession();
    else if (std::strcmp(command, "MODE:FREESTYLE") == 0)
        setMode(training::Mode::Freestyle);
    else if (std::strcmp(command, "MODE:CLASSIC") == 0)
        setMode(training::Mode::Classic);
    else if (std::strcmp(command, "TEST") == 0)
        simulateShot();
    else if (std::strncmp(command, "TEST:", 5) == 0 && parseUnsigned(command + 5, 10, value))
        recordTestShot(value);
    else if (std::strncmp(command, "CAL:", 4) == 0 && parseUnsigned(command + 4, 4095, value))
        settings.sensitivity = value;
    else if (std::strcmp(command, "DIST:+") == 0)
        settings.adjustDistance(1);
    else if (std::strcmp(command, "DIST:-") == 0)
        settings.adjustDistance(-1);
    else if (std::strcmp(command, "CAL:+") == 0)
        settings.adjustSensitivity(1);
    else if (std::strcmp(command, "CAL:-") == 0)
        settings.adjustSensitivity(-1);
    else if (std::strcmp(command, "UNIT:M") == 0)
        settings.unit = training::Unit::Meters;
    else if (std::strcmp(command, "UNIT:FT") == 0)
        settings.unit = training::Unit::Feet;
    else if (std::strcmp(command, "NAV:TRAINING") == 0)
        navigate(Page::Training);
    else if (std::strcmp(command, "NAV:SETTINGS") == 0)
        navigate(Page::Settings);
    else if (std::strcmp(command, "NAV:DEBUGZONE") == 0)
        navigate(Page::DebugZone);
    else if (std::strcmp(command, "NAV:DEBUG") == 0) {
        navigate(Page::DebugZone);
        openDebug();
    } else if (std::strcmp(command, "FULL:0") == 0 || std::strcmp(command, "FULL:1") == 0) {
        navigate(Page::Training);
        fullscreen = command[5] == '1';
    } else if (std::strcmp(command, "THEME:DARK") == 0)
        darkMode = true;
    else if (std::strcmp(command, "THEME:LIGHT") == 0)
        darkMode = false;
    else if (std::strcmp(command, "MENU:OPEN") == 0 || std::strcmp(command, "MENU:CLOSE") == 0)
        animateMenu(command[5] == 'O');
    else if (std::strncmp(command, "VIEW:", 5) == 0) {
        if (std::strcmp(command + 5, "PAYLOAD") == 0)
            packetView = PacketView::Payload;
        else if (std::strcmp(command + 5, "HEX") == 0)
            packetView = PacketView::Hex;
        else if (std::strcmp(command + 5, "BINARY") == 0)
            packetView = PacketView::Binary;
        else
            return "COMMAND";
    } else if (std::strcmp(command, "FX:STOP") == 0) {
        hardware::stopSpeakerSequence();
        celebration = Celebration::None;
    } else if (std::strncmp(command, "FX:", 3) == 0) {
        const char *names[] = {"CONFETTI", "ORBIT", "BOUNCE", "WARP"};
        unsigned effect = 0;
        for (unsigned i = 0; i < 4; ++i) {
            if (std::strcmp(command + 3, names[i]) == 0)
                effect = i + 1;
        }
        if (effect == 0)
            return "COMMAND";
        navigate(Page::DebugZone);
        startCelebration(static_cast<Celebration>(effect));
    } else if (std::strcmp(command, "SOUND:TEST") == 0) {
        if (!hardware::startSpeakerTest())
            return "BUSY";
    } else if (std::strcmp(command, "BLE:RESTART") == 0)
        bluetooth::restartAdvertising();
    else if (std::strncmp(command, "DIST:", 5) == 0) {
        char *end = nullptr;
        const double distance = std::strtod(command + 5, &end);
        const bool feet = std::strcmp(end, "FT") == 0;
        const bool meters = std::strcmp(end, "M") == 0;
        const double millimeters = distance * (feet ? 304.8 : 1000.0);
        if ((!feet && !meters) || !std::isfinite(millimeters) || millimeters < 1000.0 ||
            millimeters > 100000.0)
            return "DIST RANGE";
        settings.distanceMillimeters = static_cast<int>(millimeters + 0.5);
        settings.unit = feet ? training::Unit::Feet : training::Unit::Meters;
    } else
        return "COMMAND";
    dirty = true;
    return nullptr;
}

// Match each app request to its reply. Plain commands still work for tools.
void handleCommand(const char *text) {
    const char *command = text;
    unsigned id = 0;
    if (*text == '@') {
        const char *colon = std::strchr(text, ':');
        if (colon == nullptr || colon - text > 4)
            return;
        char number[4] = {};
        std::memcpy(number, text + 1, colon - text - 1);
        if (!parseUnsigned(number, 999, id) || id == 0)
            return;
        command = colon + 1;
    }
    const char *error = applyCommand(command);
    const bool queued = sendState(true);
    char reply[21];
    if (error != nullptr) {
        if (id)
            std::snprintf(reply, sizeof(reply), "ERR:%u:%s", id, error);
        else
            std::snprintf(reply, sizeof(reply), "ERR:%s", error);
        bluetooth::notify(reply);
    } else if (id && queued) {
        std::snprintf(reply, sizeof(reply), "ACK:%u", id);
        bluetooth::notify(reply);
    }
}
