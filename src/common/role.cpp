#include "broa11y/role.h"

#include <string_view>

namespace broa11y {

std::string_view role_to_string(Role role) {
    switch (role) {
        case Role::Unknown: return "unknown";
        case Role::Application: return "application";
        case Role::Window: return "window";
        case Role::Dialog: return "dialog";
        case Role::Alert: return "alert";
        case Role::Button: return "button";
        case Role::CheckBox: return "checkbox";
        case Role::RadioButton: return "radio_button";
        case Role::TextInput: return "text_input";
        case Role::Terminal: return "terminal";
        case Role::Label: return "label";
        case Role::Link: return "link";
        case Role::List: return "list";
        case Role::ListItem: return "list_item";
        case Role::Menu: return "menu";
        case Role::MenuItem: return "menu_item";
        case Role::MenuBar: return "menu_bar";
        case Role::Slider: return "slider";
        case Role::ProgressBar: return "progress_bar";
        case Role::ScrollBar: return "scroll_bar";
        case Role::Tree: return "tree";
        case Role::TreeItem: return "tree_item";
        case Role::Table: return "table";
        case Role::TableCell: return "table_cell";
        case Role::TableRow: return "table_row";
        case Role::TableColumn: return "table_column";
        case Role::Group: return "group";
        case Role::Panel: return "panel";
        case Role::Tab: return "tab";
        case Role::TabList: return "tab_list";
        case Role::TabPanel: return "tab_panel";
        case Role::ToolBar: return "toolbar";
        case Role::ToolTip: return "tooltip";
        case Role::Separator: return "separator";
        case Role::ComboBox: return "combobox";
        case Role::SpinButton: return "spin_button";
        case Role::StatusBar: return "status_bar";
        case Role::Heading: return "heading";
        case Role::Section: return "section";
        case Role::Canvas: return "canvas";
        case Role::Image: return "image";
        case Role::ScrollPane: return "scroll_pane";
        case Role::Document: return "document";
        case Role::Count: return "unknown";
    }
    return "unknown";
}

Role string_to_role(std::string_view str) {
    if (str == "application") return Role::Application;
    if (str == "window") return Role::Window;
    if (str == "dialog") return Role::Dialog;
    if (str == "alert") return Role::Alert;
    if (str == "button") return Role::Button;
    if (str == "checkbox") return Role::CheckBox;
    if (str == "radio_button") return Role::RadioButton;
    if (str == "text_input") return Role::TextInput;
    if (str == "terminal") return Role::Terminal;
    if (str == "label") return Role::Label;
    if (str == "link") return Role::Link;
    if (str == "list") return Role::List;
    if (str == "list_item") return Role::ListItem;
    if (str == "menu") return Role::Menu;
    if (str == "menu_item") return Role::MenuItem;
    if (str == "menu_bar") return Role::MenuBar;
    if (str == "slider") return Role::Slider;
    if (str == "progress_bar") return Role::ProgressBar;
    if (str == "scroll_bar") return Role::ScrollBar;
    if (str == "tree") return Role::Tree;
    if (str == "tree_item") return Role::TreeItem;
    if (str == "table") return Role::Table;
    if (str == "table_cell") return Role::TableCell;
    if (str == "table_row") return Role::TableRow;
    if (str == "table_column") return Role::TableColumn;
    if (str == "group") return Role::Group;
    if (str == "panel") return Role::Panel;
    if (str == "tab") return Role::Tab;
    if (str == "tab_list") return Role::TabList;
    if (str == "tab_panel") return Role::TabPanel;
    if (str == "toolbar") return Role::ToolBar;
    if (str == "tooltip") return Role::ToolTip;
    if (str == "separator") return Role::Separator;
    if (str == "combobox") return Role::ComboBox;
    if (str == "spin_button") return Role::SpinButton;
    if (str == "status_bar") return Role::StatusBar;
    if (str == "heading") return Role::Heading;
    if (str == "section") return Role::Section;
    if (str == "canvas") return Role::Canvas;
    if (str == "image") return Role::Image;
    if (str == "scroll_pane") return Role::ScrollPane;
    if (str == "document") return Role::Document;
    return Role::Unknown;
}

uint32_t role_to_atspi_role(Role role) {
    switch (role) {
        // AtspiRole values (at-spi2-core atspi-constants.h).
        case Role::Application: return 75;  // APPLICATION
        case Role::Window: return 23;       // FRAME: a top-level window, as GTK and Qt report it
        case Role::Dialog: return 16;       // DIALOG
        case Role::Alert: return 2;         // ALERT
        case Role::Button: return 43;       // BUTTON
        case Role::CheckBox: return 7;      // CHECK_BOX
        case Role::RadioButton: return 44;  // RADIO_BUTTON
        case Role::TextInput: return 79;    // ENTRY
        case Role::Terminal: return 60;     // TERMINAL
        case Role::Label: return 29;        // LABEL
        case Role::Link: return 88;         // LINK
        case Role::List: return 31;         // LIST
        case Role::ListItem: return 32;     // LIST_ITEM
        case Role::Menu: return 33;         // MENU
        case Role::MenuItem: return 35;     // MENU_ITEM
        case Role::MenuBar: return 34;      // MENU_BAR
        case Role::Slider: return 51;       // SLIDER
        case Role::ProgressBar: return 42;  // PROGRESS_BAR
        case Role::ScrollBar: return 48;    // SCROLL_BAR
        case Role::Tree: return 65;         // TREE
        case Role::TreeItem: return 91;     // TREE_ITEM
        case Role::Table: return 55;        // TABLE
        case Role::TableCell: return 56;    // TABLE_CELL
        case Role::TableRow: return 90;     // TABLE_ROW
        case Role::TableColumn: return 57;  // TABLE_COLUMN_HEADER
        case Role::Group: return 99;        // GROUPING
        case Role::Panel: return 39;        // PANEL
        case Role::Tab: return 37;          // PAGE_TAB
        case Role::TabList: return 38;      // PAGE_TAB_LIST
        case Role::TabPanel: return 39;     // PANEL
        case Role::ToolBar: return 63;      // TOOL_BAR
        case Role::ToolTip: return 64;      // TOOL_TIP
        case Role::Separator: return 50;    // SEPARATOR
        case Role::ComboBox: return 11;     // COMBO_BOX
        case Role::SpinButton: return 52;   // SPIN_BUTTON
        case Role::StatusBar: return 54;    // STATUS_BAR
        case Role::Heading: return 83;      // HEADING
        case Role::Section: return 85;      // SECTION
        case Role::Canvas: return 6;        // CANVAS
        case Role::Image: return 27;        // IMAGE
        case Role::ScrollPane: return 49;   // SCROLL_PANE
        case Role::Document: return 82;     // DOCUMENT_FRAME
        case Role::Unknown:
        case Role::Count: return 67;        // UNKNOWN
    }
    return 67;
}

std::string_view role_to_atspi_name(Role role) {
    switch (role) {
        case Role::Application: return "application";
        // The names libatspi's atspi_role_get_name() gives each AtspiRole above.
        case Role::Window: return "frame";
        case Role::Dialog: return "dialog";
        case Role::Alert: return "alert";
        case Role::Button: return "button";
        case Role::CheckBox: return "check box";
        case Role::RadioButton: return "radio button";
        case Role::TextInput: return "entry";
        case Role::Terminal: return "terminal";
        case Role::Label: return "label";
        case Role::Link: return "link";
        case Role::List: return "list";
        case Role::ListItem: return "list item";
        case Role::Menu: return "menu";
        case Role::MenuItem: return "menu item";
        case Role::MenuBar: return "menu bar";
        case Role::Slider: return "slider";
        case Role::ProgressBar: return "progress bar";
        case Role::ScrollBar: return "scroll bar";
        case Role::Tree: return "tree";
        case Role::TreeItem: return "tree item";
        case Role::Table: return "table";
        case Role::TableCell: return "table cell";
        case Role::TableRow: return "table row";
        case Role::TableColumn: return "table column header";
        case Role::Group: return "grouping";
        case Role::Panel: return "panel";
        case Role::Tab: return "page tab";
        case Role::TabList: return "page tab list";
        case Role::ToolBar: return "tool bar";
        case Role::ToolTip: return "tool tip";
        case Role::Separator: return "separator";
        case Role::ComboBox: return "combo box";
        case Role::SpinButton: return "spin button";
        case Role::StatusBar: return "status bar";
        case Role::Heading: return "heading";
        case Role::Section: return "section";
        case Role::Canvas: return "canvas";
        case Role::Image: return "image";
        case Role::ScrollPane: return "scroll pane";
        case Role::Document: return "document frame";
        case Role::TabPanel: return "panel";
        default: return "unknown";
    }
}

uint32_t role_to_uia_control_type(Role role) {
    switch (role) {
        case Role::Button: return 50000; // UIA_ButtonControlTypeId
        case Role::CheckBox: return 50002; // UIA_CheckBoxControlTypeId
        case Role::ComboBox: return 50003; // UIA_ComboBoxControlTypeId
        case Role::TextInput: return 50004; // UIA_EditControlTypeId
        case Role::Terminal: return 50030; // UIA_DocumentControlTypeId / Edit
        case Role::Link: return 50005; // UIA_HyperlinkControlTypeId
        case Role::Image: return 50006; // UIA_ImageControlTypeId
        case Role::ListItem: return 50007; // UIA_ListItemControlTypeId
        case Role::List: return 50008; // UIA_ListControlTypeId
        case Role::Menu: return 50009; // UIA_MenuControlTypeId
        case Role::MenuBar: return 50010; // UIA_MenuBarControlTypeId
        case Role::MenuItem: return 50011; // UIA_MenuItemControlTypeId
        case Role::ProgressBar: return 50012; // UIA_ProgressBarControlTypeId
        case Role::RadioButton: return 50013; // UIA_RadioButtonControlTypeId
        case Role::ScrollBar: return 50014; // UIA_ScrollBarControlTypeId
        case Role::Slider: return 50015; // UIA_SliderControlTypeId
        case Role::SpinButton: return 50016; // UIA_SpinnerControlTypeId
        case Role::StatusBar: return 50017; // UIA_StatusBarControlTypeId
        case Role::TabList: return 50018; // UIA_TabControlTypeId
        case Role::Tab: return 50019; // UIA_TabItemControlTypeId
        case Role::Label: return 50020; // UIA_TextControlTypeId
        case Role::ToolBar: return 50021; // UIA_ToolBarControlTypeId
        case Role::ToolTip: return 50022; // UIA_ToolTipControlTypeId
        case Role::Tree: return 50023; // UIA_TreeControlTypeId
        case Role::TreeItem: return 50024; // UIA_TreeItemControlTypeId
        case Role::Group: return 50026; // UIA_GroupControlTypeId
        case Role::Panel: return 50033; // UIA_PaneControlTypeId
        case Role::Document: return 50030; // UIA_DocumentControlTypeId
        case Role::Window: return 50032; // UIA_WindowControlTypeId
        case Role::Dialog: return 50032; // UIA_WindowControlTypeId
        case Role::Table: return 50036; // UIA_TableControlTypeId
        case Role::TableRow: return 50029; // UIA_DataItemControlTypeId
        case Role::TableCell: return 50029; // UIA_DataItemControlTypeId
        case Role::TableColumn: return 50035; // UIA_HeaderItemControlTypeId
        case Role::Separator: return 50038; // UIA_SeparatorControlTypeId
        case Role::Heading: return 50020; // UIA_TextControlTypeId (level in the heading-level property)
        case Role::Section: return 50026; // UIA_GroupControlTypeId
        default: return 50033; // UIA_PaneControlTypeId
    }
}

std::string_view role_to_uia_control_name(Role role) {
    switch (role) {
        case Role::Button: return "Button";
        case Role::CheckBox: return "CheckBox";
        case Role::ComboBox: return "ComboBox";
        case Role::TextInput: return "Edit";
        case Role::Terminal: return "Document";
        case Role::Link: return "Hyperlink";
        case Role::Image: return "Image";
        case Role::ListItem: return "ListItem";
        case Role::List: return "List";
        case Role::Menu: return "Menu";
        case Role::MenuBar: return "MenuBar";
        case Role::MenuItem: return "MenuItem";
        case Role::ProgressBar: return "ProgressBar";
        case Role::RadioButton: return "RadioButton";
        case Role::ScrollBar: return "ScrollBar";
        case Role::Slider: return "Slider";
        case Role::SpinButton: return "Spinner";
        case Role::StatusBar: return "StatusBar";
        case Role::TabList: return "Tab";
        case Role::Tab: return "TabItem";
        case Role::Label: return "Text";
        case Role::ToolBar: return "ToolBar";
        case Role::ToolTip: return "ToolTip";
        case Role::Tree: return "Tree";
        case Role::TreeItem: return "TreeItem";
        case Role::Group: return "Group";
        case Role::Panel: return "Pane";
        case Role::Document: return "Document";
        case Role::Window: return "Window";
        case Role::Table: return "Table";
        case Role::TableRow: return "DataItem";
        case Role::TableCell: return "DataItem";
        case Role::TableColumn: return "HeaderItem";
        case Role::Separator: return "Separator";
        case Role::Heading: return "Text";
        case Role::Section: return "Group";
        default: return "Pane";
    }
}

std::string_view role_to_mac_role(Role role) {
    switch (role) {
        case Role::Application: return "AXApplication";
        case Role::Window: return "AXWindow";
        case Role::Dialog: return "AXWindow";
        case Role::Alert: return "AXWindow";
        case Role::Button: return "AXButton";
        case Role::CheckBox: return "AXCheckBox";
        case Role::RadioButton: return "AXRadioButton";
        case Role::TextInput: return "AXTextField";
        case Role::Terminal: return "AXTextArea";
        case Role::Label: return "AXStaticText";
        case Role::Link: return "AXLink";
        case Role::List: return "AXList";
        case Role::ListItem: return "AXRow";
        case Role::Menu: return "AXMenu";
        case Role::MenuItem: return "AXMenuItem";
        case Role::MenuBar: return "AXMenuBar";
        case Role::Slider: return "AXSlider";
        case Role::ProgressBar: return "AXProgressIndicator";
        case Role::ScrollBar: return "AXScrollBar";
        case Role::Tree: return "AXOutline";
        case Role::TreeItem: return "AXRow";
        case Role::Table: return "AXTable";
        case Role::TableCell: return "AXCell";
        case Role::TableRow: return "AXRow";
        case Role::TableColumn: return "AXColumn";
        case Role::Group: return "AXGroup";
        case Role::Panel: return "AXGroup";
        case Role::Tab: return "AXRadioButton";
        case Role::TabList: return "AXTabGroup";
        case Role::TabPanel: return "AXGroup";
        case Role::ToolBar: return "AXToolbar";
        case Role::ToolTip: return "AXHelpTag";
        case Role::Separator: return "AXSplitter";
        case Role::ComboBox: return "AXComboBox";
        case Role::SpinButton: return "AXIncrementor";
        case Role::StatusBar: return "AXGroup";
        case Role::Heading: return "AXHeading";
        case Role::Section: return "AXGroup";
        case Role::Canvas: return "AXGroup";
        case Role::Image: return "AXImage";
        case Role::ScrollPane: return "AXScrollArea";
        case Role::Document: return "AXDocument";
        default: return "AXUnknown";
    }
}

std::string_view role_to_mac_subrole(Role role) {
    switch (role) {
        case Role::Dialog: return "AXDialog";
        case Role::Alert: return "AXSystemDialog";
        case Role::Tab: return "AXTabButton";
        case Role::Terminal: return "AXTerminal";
        default: return "";
    }
}

} // namespace broa11y
