#include <iostream>
#include "event_manager.h"

using std::cout;
using std::endl;

const struct TestEventT {
    using HandlerType = void();
} TestEvent;

class SomeSender {
};

int main()
{
    SomeSender sender;
    evt::EventManager evMgr;
    auto test = [](){
        cout << "Hello World!" << endl;
    };
    evMgr.BindEventHandler(TestEvent, sender, test);
    evMgr.SendEvent(TestEvent, sender);
    return 0;
}
