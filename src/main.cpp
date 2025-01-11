#include <iostream>
#include "event_manager.h"

using std::cout;
using std::endl;

enum class Event {
    SingleEvent
};

class SomeSender {
};

template <>
struct evt::event_traits<Event::SingleEvent>
{
    using handler = void(const SomeSender&);
};

void free_func(const SomeSender&)
{
    cout << "Hello from free_func!" << endl;
}

struct SomeObserver {
    void MemberObserver(const SomeSender&)
    {
        cout << "Hello from SomeObserver::MemberObserver!" << endl;
    }
};

int main()
{
    SomeSender sender;
    auto test = [](const SomeSender&){
        cout << "Hello from lambda!" << endl;
    };
    SomeObserver observer;
    evt::EventManager<Event> evMgr;
    evMgr.Register<Event::SingleEvent>(free_func);
    evMgr.Register<Event::SingleEvent>(test);
    evMgr.Register<Event::SingleEvent>(observer, evt::fn_tag<&SomeObserver::MemberObserver>);
    evMgr.Send<Event::SingleEvent>(sender);
    return 0;
}
