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
        case Role::Application: return 2;
        case Role::Window: return 75;
        case Role::Dialog: return 14;
        case Role::Alert: return 1;
        case Role::Button: return 7;
        case Role::CheckBox: return 8;
        case Role::RadioButton: return 45;
        case Role::TextInput: return 18; // Entry
        case Role::Terminal: return 64; // Terminal
        case Role::Label: return 34; // Label
        case Role::Link: return 36; // Link
        case Role::List: return 37; // List
        case Role::ListItem: return 38; // ListItem
        case Role::Menu: return 39; // Menu
        case Role::MenuItem: return 40; // MenuItem
        case Role::MenuBar: return 41; // MenuBar
        case Role::Slider: return 54; // Slider
        case Role::ProgressBar: return 44; // ProgressBar
        case Role::ScrollBar: return 49; // ScrollBar
        case Role::Tree: return 68; // Tree
        case Role::TreeItem: return 69; // TreeItem
        case Role::Table: return 61; // Table
        case Role::TableCell: return 62; // TableCell
        case Role::TableRow: return 63; // TableRow
        case Role::TableColumn: return 64; // TableColumn
        case Role::Group: return 20; // Filler
        case Role::Panel: return 42; // Panel
        case Role::Tab: return 43; // PageTab
        case Role::TabList: return 44; // PageTabList
        case Role::ToolBar: return 66; // ToolBar
        case Role::ToolTip: return 67; // ToolTip
        case Role::Separator: return 53; // Separator
        case Role::ComboBox: return 10; // ComboBox
        case Role::SpinButton: return 55; // SpinButton
        case Role::StatusBar: return 56; // StatusBar
        case Role::Heading: return 27; // Heading
        case Role::Section: return 51; // Section
        case Role::Canvas: return 6; // Canvas
        case Role::Image: return 29; // Icon / Image
        case Role::ScrollPane: return 50; // ScrollPane
        case Role::Document: return 78; // Document
        default: return 0; // Invalid
    }
}

std::string_view role_to_atspi_name(Role role) {
    switch (role) {
        case Role::Application: return "application";
        case Role::Window: return "window";
        case Role::Dialog: return "dialog";
        case Role::Alert: return "alert";
        case Role::Button: return "push button";
        case Role::CheckBox: return "check box";
        case Role::RadioButton: return "radio button";
        case Role::TextInput: return "text";
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
        case Role::TableColumn: return "table column";
        case Role::Group: return "panel";
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
        case Role::Document: return "document";
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
