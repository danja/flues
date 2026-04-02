#include "outsider_app.hpp"

#include "../net/ws_server.hpp"
#include "../semaphore/semaphore_engine.hpp"
#include "../session/session_registry.hpp"
#include "../transport/transport_authority.hpp"
#include "../ui/outsider_ui_x11.hpp"

namespace outsider {

int OutsiderApp::run() {
    SessionRegistry registry;
    TransportAuthority authority;
    SemaphoreEngine semaphore;
    WsServerStub server(registry, authority, semaphore);

    server.start();

    OutsiderUiX11 ui(registry, authority, server, semaphore);
    if (!ui.open()) {
        server.stop();
        return 1;
    }

    const int result = ui.run();
    server.stop();
    return result;
}

}  // namespace outsider
