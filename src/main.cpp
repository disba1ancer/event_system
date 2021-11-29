#include <iostream>
#include "event_manager.h"

using std::cout;
using std::endl;

const struct TestEventT {
    using HandlerType = void();
} TestEvent;

class SomeSender {
};

void free_func()
{
    cout << "Hello from free_func!" << endl;
}

struct SomeObserver {
    void MemberObserver() const
    {
        cout << "Hello from SomeObserver::MemberObserver!" << endl;
    }
};

int main()
{
    evt::EventManager evMgr;
    SomeSender sender;
    auto test = [](){
        cout << "Hello from lambda!" << endl;
    };
    SomeObserver observer;
    evMgr.BindEventHandler(TestEvent, sender, free_func);
    evMgr.BindEventHandler(TestEvent, sender, test);
    evMgr.BindEventHandler(TestEvent, sender, evt::MemFnToFunc<&SomeObserver::MemberObserver>(observer));
    evMgr.SendEvent(TestEvent, sender);
    return 0;
}
