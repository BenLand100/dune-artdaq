#include <map>
#include "../TriggerPrimitiveFinder.h"

int main(int, char**)
{
    std::map<uint64_t, std::unique_ptr<TriggerPrimitiveFinder>> m_tp_finders;
    m_tp_finders[13]=std::make_unique<TriggerPrimitiveFinder>(500, 128, 1);
}
