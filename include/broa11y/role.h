#pragma once

#include <cstdint>
#include <string_view>

namespace broa11y {

enum class Role : uint32_t {
    Unknown = 0,
    Application,
    Window,
    Dialog,
    Alert,
    Button,
    CheckBox,
    RadioButton,
    TextInput,
    Terminal,
    Label,
    Link,
    List,
    ListItem,
    Menu,
    MenuItem,
    MenuBar,
    Slider,
    ProgressBar,
    ScrollBar,
    Tree,
    TreeItem,
    Table,
    TableCell,
    TableRow,
    TableColumn,
    Group,
    Panel,
    Tab,
    TabList,
    TabPanel,
    ToolBar,
    ToolTip,
    Separator,
    ComboBox,
    SpinButton,
    StatusBar,
    Heading,
    Section,
    Canvas,
    Image,
    ScrollPane,
    Document,
    Count
};

std::string_view role_to_string(Role role);
Role string_to_role(std::string_view str);

uint32_t role_to_atspi_role(Role role);
std::string_view role_to_atspi_name(Role role);

uint32_t role_to_uia_control_type(Role role);
std::string_view role_to_uia_control_name(Role role);

std::string_view role_to_mac_role(Role role);
std::string_view role_to_mac_subrole(Role role);

} // namespace broa11y
