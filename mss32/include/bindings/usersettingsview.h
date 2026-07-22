#ifndef USERSETTINGSVIEW_H
#define USERSETTINGSVIEW_H

#include <sol/forward.hpp>

namespace bindings {

class UserSettingsView
{
public:
    static void bind(sol::state& lua);
};

} // namespace bindings

#endif