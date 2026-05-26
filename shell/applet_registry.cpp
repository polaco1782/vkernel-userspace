#include "applets/applet.h"

#include <array>

namespace applet::cat { extern const shell::command_spec kCommand; }
namespace applet::cd { extern const shell::command_spec kCommand; }
namespace applet::clear { extern const shell::command_spec kCommand; }
namespace applet::drvload { extern const shell::command_spec kCommand; }
namespace applet::exit { extern const shell::command_spec kCommand; }
namespace applet::help { extern const shell::command_spec kCommand; }
namespace applet::kill { extern const shell::command_spec kCommand; }
namespace applet::ls { extern const shell::command_spec kCommand; }
namespace applet::ps { extern const shell::command_spec kCommand; }
namespace applet::pwd { extern const shell::command_spec kCommand; }
namespace applet::reboot { extern const shell::command_spec kCommand; }
namespace applet::run { extern const shell::command_spec kCommand; }
namespace applet::version { extern const shell::command_spec kCommand; }

namespace shell {
namespace {

constexpr std::array<const command_spec*, 13> kAppletCommands = {{
    &applet::cat::kCommand,
    &applet::cd::kCommand,
    &applet::clear::kCommand,
    &applet::drvload::kCommand,
    &applet::exit::kCommand,
    &applet::help::kCommand,
    &applet::kill::kCommand,
    &applet::ls::kCommand,
    &applet::ps::kCommand,
    &applet::pwd::kCommand,
    &applet::reboot::kCommand,
    &applet::run::kCommand,
    &applet::version::kCommand,
}};

} // namespace

auto applet_commands() -> command_list_view
{
    return { kAppletCommands.data(), static_cast<vk_usize>(kAppletCommands.size()) };
}

} // namespace shell
