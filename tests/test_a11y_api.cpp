#include "../src/api/api.h"
#include "embed/embed.h"
#include "eval/eval.h"
#include "broa11y/broa11y.h"

#include <cstdlib>
#include <iostream>
#include <string>

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "CHECK failed: " #cond " (line " << __LINE__ << ")"   \
                      << std::endl;                                            \
            std::exit(1);                                                      \
        }                                                                      \
    } while (0)

#define CHECK_JS_OK(expr)                                                      \
    do {                                                                       \
        auto r = evalScript(                                                   \
            "(function() {\n"                                                  \
            "  try {\n"                                                        \
            "    const res = (" expr ");\n"                                    \
            "    if (res !== true && res !== 'ok') return 'fail: ' + res;\n"   \
            "    return 'ok';\n"                                               \
            "  } catch (e) {\n"                                                \
            "    return 'error: ' + (e && e.stack ? e.stack : e);\n"           \
            "  }\n"                                                            \
            "})();\n"                                                          \
        );                                                                     \
        if (r.thrown) {                                                        \
            std::cerr << "Script threw exception: " #expr << std::endl;        \
            std::exit(1);                                                      \
        }                                                                      \
        std::string str = ev::toUtf8(r.value);                                 \
        if (str != "ok") {                                                     \
            std::cerr << "CHECK_JS_OK failed: " << str << " for (" #expr ")"   \
                      << " (line " << __LINE__ << ")" << std::endl;            \
            std::exit(1);                                                      \
        }                                                                      \
    } while (0)

int main() {
    namespace ev = bronze::embed;
    using namespace bronze::eval;

    std::cout << "Starting broa11y JavaScript API tests..." << std::endl;

    // 1. Install bro.a11y into Bronze realm
    broa11y::api::installA11y();

    auto g = ev::globalValue("bro");
    CHECK(g.found);
    CHECK(ev::isObject(g.value));

    ev::Persistent a11y(ev::getProperty(g.value, "a11y"));
    CHECK(ev::isObject(a11y.get()));
    std::cout << "  Mounted bro.a11y successfully." << std::endl;

    CHECK(ev::isBool(ev::getProperty(a11y.get(), "available")));

    // 2. Verify all core methods exist on bro.a11y
    const char* methods[] = {
        "isAvailable", "tick", "shutdown",
        "announce", "getRootNode", "getNode", "createNode", "removeNode",
        "reparent", "setFocus", "clearFocus", "getFocusedNode", "hitTest",
        "findNodesByRole", "findNodesByName", "beginTransaction", "commitTransaction",
        "rollbackTransaction", "inTransaction", "performAction",
        "on", "off", "addEventListener", "removeEventListener", "addListener", "removeListener",
        "registerCustomRole", "unregisterCustomRole", "getCustomRoles", "hasCustomRole",
        "registerWidgetHook", "unregisterWidgetHook"
    };
    for (const char* m : methods) {
        auto fn = ev::getProperty(a11y.get(), m);
        CHECK(ev::isFunction(fn));
        std::cout << "  Found bro.a11y." << m << std::endl;
    }

    // 3. Test available
    CHECK_JS_OK("typeof bro.a11y.available === 'boolean'");
    CHECK_JS_OK("typeof bro.a11y.isAvailable() === 'boolean'");

    // 4. Test root node inspection
    CHECK_JS_OK("bro.a11y.getRootNode() !== null");
    CHECK_JS_OK("typeof bro.a11y.getRootNode().id === 'number'");
    CHECK_JS_OK("bro.a11y.getRootNode().role === 'application'");
    CHECK_JS_OK("bro.a11y.getRootNode().name === 'bro'");
    std::cout << "  Root node verified." << std::endl;

    // 5. Test building a widget tree in JS
    CHECK_JS_OK(
        "(() => {\n"
        "  const root = bro.a11y.getRootNode();\n"
        "  root.setBounds(0, 0, 800, 600);\n"
        "  const w = bro.a11y.createNode({\n"
        "    id: 10,\n"
        "    role: 'window',\n"
        "    name: 'Main App Window',\n"
        "    parentId: root.id,\n"
        "    bounds: { x: 0, y: 0, width: 800, height: 600 }\n"
        "  });\n"
        "  if (!w || w.id !== 10 || w.role !== 'window' || w.name !== 'Main App Window') return false;\n"
        "  return true;\n"
        "})()"
    );


    CHECK_JS_OK(
        "(() => {\n"
        "  const p = bro.a11y.createNode({\n"
        "    id: 20,\n"
        "    role: 'panel',\n"
        "    name: 'Control Panel',\n"
        "    parentId: 10,\n"
        "    bounds: { x: 10, y: 10, width: 780, height: 580 }\n"
        "  });\n"
        "  if (!p || p.id !== 20 || p.parentId !== 10) return false;\n"
        "  return true;\n"
        "})()"
    );

    CHECK_JS_OK(
        "(() => {\n"
        "  const btn = bro.a11y.createNode({\n"
        "    id: 30,\n"
        "    role: 'button',\n"
        "    name: 'Submit Button',\n"
        "    parentId: 20,\n"
        "    bounds: { x: 20, y: 20, width: 100, height: 40 }\n"
        "  });\n"
        "  btn.setState('focusable', true);\n"
        "  if (!btn.hasState('focusable')) return false;\n"
        "  return true;\n"
        "})()"
    );

    CHECK_JS_OK(
        "(() => {\n"
        "  const slider = bro.a11y.createNode({\n"
        "    id: 40,\n"
        "    role: 'slider',\n"
        "    name: 'Volume',\n"
        "    parentId: 20,\n"
        "    bounds: { x: 130, y: 20, width: 200, height: 40 }\n"
        "  });\n"
        "  slider.setValue(60, 0, 100, 5);\n"
        "  const sNode = bro.a11y.getNode(40);\n"
        "  if (!sNode || !sNode.value || sNode.value.current !== 60) return false;\n"
        "  if (sNode.value.minimum !== 0 || sNode.value.maximum !== 100 || sNode.value.step !== 5) return false;\n"
        "  return true;\n"
        "})()"
    );

    CHECK_JS_OK(
        "(() => {\n"
        "  const input = bro.a11y.createNode({\n"
        "    id: 50,\n"
        "    role: 'text_input',\n"
        "    name: 'Username',\n"
        "    parentId: 20,\n"
        "    bounds: { x: 350, y: 20, width: 200, height: 40 }\n"
        "  });\n"
        "  input.setText('john_doe');\n"
        "  input.setCaretOffset(4);\n"
        "  input.setSelection(0, 4);\n"
        "  input.setAttribute('placeholder', 'Enter username');\n"
        "  const iNode = bro.a11y.getNode(50);\n"
        "  if (!iNode || iNode.text !== 'john_doe') return false;\n"
        "  if (iNode.caretOffset !== 4) return false;\n"
        "  if (iNode.selection.startOffset !== 0 || iNode.selection.endOffset !== 4) return false;\n"
        "  if (iNode.attributes.placeholder !== 'Enter username') return false;\n"
        "  if (iNode.getAttribute('placeholder') !== 'Enter username') return false;\n"
        "  return true;\n"
        "})()"
    );
    std::cout << "  Tree hierarchy and property reading passed." << std::endl;

    // 6. Test hierarchy relationships & child inspection
    CHECK_JS_OK(
        "(() => {\n"
        "  const panel = bro.a11y.getNode(20);\n"
        "  if (!panel) return false;\n"
        "  if (panel.parentId !== 10) return false;\n"
        "  if (!panel.childIds.includes(30) || !panel.childIds.includes(40) || !panel.childIds.includes(50)) return false;\n"
        "  const children = panel.getChildren();\n"
        "  if (!Array.isArray(children) || children.length !== 3) return false;\n"
        "  const parent = panel.getParent();\n"
        "  if (!parent || parent.id !== 10) return false;\n"
        "  return true;\n"
        "})()"
    );

    // 7. Test hit testing
    CHECK_JS_OK(
        "(() => {\n"
        "  const hit = bro.a11y.hitTest(25, 25);\n"
        "  if (!hit || hit.id !== 30) return false;\n"
        "  return true;\n"
        "})()"
    );

    // 8. Test find by role and name
    CHECK_JS_OK(
        "(() => {\n"
        "  const buttons = bro.a11y.findNodesByRole('button');\n"
        "  if (!Array.isArray(buttons) || buttons.length < 1) return false;\n"
        "  if (buttons[0].id !== 30) return false;\n"
        "  const ok = bro.a11y.findNodesByName('Submit Button');\n"
        "  if (!Array.isArray(ok) || ok.length < 1 || ok[0].id !== 30) return false;\n"
        "  return true;\n"
        "})()"
    );

    // 9. Test focus management
    CHECK_JS_OK(
        "(() => {\n"
        "  const ok = bro.a11y.setFocus(30);\n"
        "  if (!ok) return false;\n"
        "  const focused = bro.a11y.getFocusedNode();\n"
        "  if (!focused || focused.id !== 30) return false;\n"
        "  if (!focused.hasState('focused')) return false;\n"
        "  bro.a11y.clearFocus();\n"
        "  if (bro.a11y.getFocusedNode() !== null) return false;\n"
        "  return true;\n"
        "})()"
    );
    std::cout << "  Focus and hit testing passed." << std::endl;

    // 10. Test actions & action handlers
    CHECK_JS_OK(
        "(() => {\n"
        "  const btn = bro.a11y.getNode(30);\n"
        "  btn.addAction('activate', 'Clicks button', 'Space');\n"
        "  let activated = false;\n"
        "  btn.onAction((act, params) => {\n"
        "    if (act === 'activate') {\n"
        "      activated = true;\n"
        "      return true;\n"
        "    }\n"
        "    return false;\n"
        "  });\n"
        "  const res = bro.a11y.performAction(30, 'activate');\n"
        "  if (!res || !activated) return false;\n"
        "  return true;\n"
        "})()"
    );

    // 11. Test reparenting and node removal
    CHECK_JS_OK(
        "(() => {\n"
        "  const repOk = bro.a11y.reparent(50, 10);\n"
        "  if (!repOk) return false;\n"
        "  const input = bro.a11y.getNode(50);\n"
        "  if (!input || input.parentId !== 10) return false;\n"
        "  const rmOk = bro.a11y.removeNode(50);\n"
        "  if (!rmOk) return false;\n"
        "  if (bro.a11y.getNode(50) !== null) return false;\n"
        "  return true;\n"
        "})()"
    );
    std::cout << "  Actions, reparenting, and removal passed." << std::endl;

    // 12. Test transactions
    CHECK_JS_OK(
        "(() => {\n"
        "  const btn = bro.a11y.getNode(30);\n"
        "  const oldName = btn.name;\n"
        "  bro.a11y.beginTransaction();\n"
        "  if (!bro.a11y.inTransaction()) return false;\n"
        "  btn.setName('Temp Name');\n"
        "  bro.a11y.rollbackTransaction();\n"
        "  if (bro.a11y.inTransaction()) return false;\n"
        "  const restoredBtn = bro.a11y.getNode(30);\n"
        "  if (restoredBtn.name !== oldName) return false;\n"
        "  return true;\n"
        "})()"
    );

    // 13. Test announce & event dispatch via tick()
    CHECK_JS_OK(
        "(() => {\n"
        "  let gotAnnouncement = null;\n"
        "  const handle = bro.a11y.on('announce', (payload) => {\n"
        "    gotAnnouncement = payload;\n"
        "  });\n"
        "  const announced = bro.a11y.announce('System updated', { priority: 'assertive' });\n"
        "  if (!announced) return false;\n"
        "  bro.a11y.tick();\n"
        "  handle.remove();\n"
        "  if (!gotAnnouncement || gotAnnouncement.message !== 'System updated' || gotAnnouncement.priority !== 'assertive') {\n"
        "    return false;\n"
        "  }\n"
        "  return true;\n"
        "})()"
    );

    // 14. Test focus event listener
    CHECK_JS_OK(
        "(() => {\n"
        "  let focusedId = null;\n"
        "  const handle = bro.a11y.on('nodeFocused', (payload) => {\n"
        "    focusedId = payload.currentFocusedId;\n"
        "  });\n"
        "  bro.a11y.setFocus(30);\n"
        "  bro.a11y.tick();\n"
        "  bro.a11y.off(handle);\n"
        "  if (focusedId !== 30) return false;\n"
        "  return true;\n"
        "})()"
    );
    std::cout << "  Transactions, announce, and event listeners passed." << std::endl;

    // 15. Test custom roles & widget hooks
    CHECK_JS_OK(
        "(() => {\n"
        "  const reg = bro.a11y.registerCustomRole('terminal_grid', {\n"
        "    baseRole: 'canvas',\n"
        "    description: 'Custom Terminal Grid',\n"
        "    supportedActions: ['activate', 'focus'],\n"
        "    supportedStates: ['focused', 'focusable']\n"
        "  });\n"
        "  if (!reg) return false;\n"
        "  if (!bro.a11y.hasCustomRole('terminal_grid')) return false;\n"
        "  const roles = bro.a11y.getCustomRoles();\n"
        "  const found = roles.some(r => r.name === 'terminal_grid');\n"
        "  if (!found) return false;\n"
        "  const unreg = bro.a11y.unregisterCustomRole('terminal_grid');\n"
        "  if (!unreg || bro.a11y.hasCustomRole('terminal_grid')) return false;\n"
        "  return true;\n"
        "})()"
    );

    // 16. Test widget hook dispatch
    CHECK_JS_OK(
        "(() => {\n"
        "  let hookRan = false;\n"
        "  bro.a11y.registerWidgetHook(30, {\n"
        "    onAction: (act, params, id) => {\n"
        "      if (act === 'custom_trigger') {\n"
        "        hookRan = true;\n"
        "        return true;\n"
        "      }\n"
        "      return false;\n"
        "    }\n"
        "  });\n"
        "  const actRes = bro.a11y.performAction(30, 'custom_trigger');\n"
        "  bro.a11y.unregisterWidgetHook(30);\n"
        "  if (!actRes || !hookRan) return false;\n"
        "  return true;\n"
        "})()"
    );
    std::cout << "  Custom roles and widget hooks passed." << std::endl;

    // 17. GC Stress test: allocating hundreds of nodes, attributes, actions, and ticking
    std::cout << "Running GC stress test loop..." << std::endl;
    CHECK_JS_OK(
        "(() => {\n"
        "  for (let i = 1000; i < 1150; ++i) {\n"
        "    const n = bro.a11y.createNode({\n"
        "      id: i,\n"
        "      role: 'button',\n"
        "      name: 'Stress Node ' + i,\n"
        "      description: 'Description for node ' + i,\n"
        "      parentId: 20,\n"
        "      bounds: { x: i % 100, y: i % 100, width: 50, height: 20 }\n"
        "    });\n"
        "    n.setAttribute('index', String(i));\n"
        "    n.setState('focusable', true);\n"
        "    n.addAction('click', 'Click node ' + i, 'Enter');\n"
        "    if (i % 20 === 0) {\n"
        "      bro.a11y.announce('Stress announcement ' + i);\n"
        "      bro.a11y.tick();\n"
        "    }\n"
        "  }\n"
        "  for (let i = 1000; i < 1150; ++i) {\n"
        "    const n = bro.a11y.getNode(i);\n"
        "    if (!n || n.name !== 'Stress Node ' + i || n.attributes.index !== String(i)) {\n"
        "      return false;\n"
        "    }\n"
        "    bro.a11y.removeNode(i);\n"
        "  }\n"
        "  bro.a11y.tick();\n"
        "  return true;\n"
        "})()"
    );
    std::cout << "  GC stress test passed." << std::endl;

    // 18. Cleanup
    broa11y::api::shutdownA11yAsync();
    std::cout << "broa11y API tests completed successfully!" << std::endl;

    return 0;
}
