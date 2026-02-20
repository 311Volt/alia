#ifndef EVENT_BUS_BBD77B98_9236_4FEB_9494_56F656C3DA48
#define EVENT_BUS_BBD77B98_9236_4FEB_9494_56F656C3DA48

#include <functional>
#include <unordered_map>
#include <vector>
#include <any>
#include <typeindex>
#include "event.hpp"
#include "event_source.hpp"
#include "event_router.hpp"

namespace alia {

class event_bus {
    using listener_id = uint64_t;

    struct subscription {
        std::type_index type;
        std::any handler;
    };

    std::unordered_map<listener_id, subscription> subscriptions_;
    std::unordered_map<std::type_index, std::vector<listener_id>> type_to_listeners_;
    listener_id next_id_ = 0;

public:
    void listen_to(event_source& source) {
        source.register_bus(this);
    }

    void stop_listening_to(event_source& source) {
        source.unregister_bus(this);
    }

    template <event EventT>
    void publish(const EventT& ev) {
        auto it = type_to_listeners_.find(typeid(EventT));
        if (it != type_to_listeners_.end()) {
            for (auto id : it->second) {
                auto sub_it = subscriptions_.find(id);
                if (sub_it != subscriptions_.end()) {
                    auto& handler = std::any_cast<std::function<void(const EventT&)>>(sub_it->second.handler);
                    handler(ev);
                }
            }
        }
    }

    template <event EventT>
    listener_id subscribe(std::function<void(const EventT&)> handler) {
        listener_id id = ++next_id_;
        subscriptions_[id] = {typeid(EventT), handler};
        type_to_listeners_[typeid(EventT)].push_back(id);
        return id;
    }

    template <event EventT, typename RouterT, typename MapperT>
    listener_id subscribe(mapped_router<RouterT, EventT, MapperT>& router, 
                          typename RouterT::key_type key, 
                          std::function<void(const EventT&)> handler) {
        listener_id id = subscribe(handler);
        router.insert(key, id);
        return id;
    }

    template <event EventT, typename RouterT, typename MapperT>
    void dispatch(const EventT& ev, mapped_router<RouterT, EventT, MapperT>& router) {
        auto relevant_ids = router.query_from_event(ev);
        for (auto id : relevant_ids) {
            auto it = subscriptions_.find(id);
            if (it != subscriptions_.end()) {
                auto& handler = std::any_cast<std::function<void(const EventT&)>>(it->second.handler);
                handler(ev);
            }
        }
    }
};

} // namespace alia

#endif /* EVENT_BUS_BBD77B98_9236_4FEB_9494_56F656C3DA48 */
