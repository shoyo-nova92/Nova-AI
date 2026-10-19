#include "primitive_registry.hpp"
#include <algorithm>
#include <unordered_map>
#include <utility>

namespace {
using R = ParameterRequirement;
using T = PrimitiveParameterType;

PrimitiveParameterDefinition p(string n, T t, R r, string desc,
    vector<string> values = {}, bool hasMin = false, double min = 0,
    bool hasMax = false, double max = 0) {
    return {move(n), t, r, move(desc), move(values), hasMin, min, hasMax, max};
}
PrimitiveOutputDefinition o(string n, string t, string desc) {
    return {move(n), move(t), move(desc)};
}
PrimitiveParameterSchema s(R n, R l, R a, R target, R c,
    vector<PrimitiveParameterDefinition> fields) {
    PrimitiveParameterSchema x;
    x.name=n; x.location=l; x.application=a; x.target=target; x.content=c;
    x.fields=move(fields); return x;
}
PrimitiveDefinition d(PrimitiveOperationType type, string desc, string cap,
    PrimitiveParameterSchema params, vector<PrimitiveOutputDefinition> outputs) {
    PrimitiveDefinition x;
    x.type=type; x.description=move(desc); x.capability=move(cap);
    x.parameters=move(params); x.outputs=move(outputs); return x;
}

const unordered_map<string, PrimitiveDefinition> REGISTRY = {
 {"filesystem.create", d(PrimitiveOperationType::FILESYSTEM_CREATE,
  "Create one new directory inside an existing parent location.", "filesystem",
  s(R::REQUIRED,R::REQUIRED,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("name",T::STRING,R::REQUIRED,"New directory name."),
   p("location",T::DIRECTORY_PATH,R::REQUIRED,"Existing parent directory or supported alias such as desktop.")}),
  {o("path","file_path","Absolute path of the created directory."),o("created","boolean","Whether creation succeeded.")})},

 {"filesystem.create_file", d(PrimitiveOperationType::FILESYSTEM_CREATE_FILE,
  "Create a new file, optionally initializing its text content.", "filesystem",
  s(R::REQUIRED,R::REQUIRED,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::OPTIONAL,{
   p("name",T::STRING,R::REQUIRED,"File name, including extension when applicable."),
   p("location",T::DIRECTORY_PATH,R::REQUIRED,"Existing parent directory or supported location alias."),
   p("content",T::TEXT,R::OPTIONAL,"Initial file contents; omitted means empty.")}),
  {o("path","file_path","Absolute path of the created file."),o("created","boolean","Whether creation succeeded.")})},

 {"filesystem.delete", d(PrimitiveOperationType::FILESYSTEM_DELETE,
  "Delete a specified file or directory; recursive directory deletion defaults off.", "filesystem",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::REQUIRED,R::NOT_APPLICABLE,{
   p("target",T::FILE_PATH,R::REQUIRED,"Exact path of the file or directory to delete."),
   p("recursive",T::BOOLEAN,R::OPTIONAL,"Allow deleting directory contents; defaults false.",{},true,0,true,1)}),
  {o("deleted_path","file_path","Path deleted."),o("deleted","boolean","Whether deletion succeeded.")})},

 {"filesystem.move", d(PrimitiveOperationType::FILESYSTEM_MOVE,
  "Move a file or directory from source to destination.", "filesystem",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("source",T::FILE_PATH,R::REQUIRED,"Existing file or directory to move."),
   p("destination",T::FILE_PATH,R::REQUIRED,"Destination path; overwrite is not implied.")}),
  {o("path","file_path","Final path after moving."),o("moved","boolean","Whether move succeeded.")})},

 {"filesystem.copy", d(PrimitiveOperationType::FILESYSTEM_COPY,
  "Copy a file or directory from source to destination.", "filesystem",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("source",T::FILE_PATH,R::REQUIRED,"Existing file or directory to copy."),
   p("destination",T::FILE_PATH,R::REQUIRED,"Destination path."),
   p("overwrite",T::BOOLEAN,R::OPTIONAL,"Allow replacing destination; defaults false.",{},true,0,true,1)}),
  {o("path","file_path","Path of copied resource."),o("copied","boolean","Whether copy succeeded.")})},

 {"filesystem.rename", d(PrimitiveOperationType::FILESYSTEM_RENAME,
  "Rename a file or directory within its current parent directory.", "filesystem",
  s(R::REQUIRED,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::REQUIRED,R::NOT_APPLICABLE,{
   p("target",T::FILE_PATH,R::REQUIRED,"Existing resource to rename."),
   p("newName",T::STRING,R::REQUIRED,"New base name, not a parent path.")}),
  {o("path","file_path","Final path after renaming."),o("renamed","boolean","Whether rename succeeded.")})},

 {"application.launch", d(PrimitiveOperationType::APPLICATION_LAUNCH,
  "Launch an application, optionally opening a specified file or folder.", "application_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::REQUIRED,R::OPTIONAL,R::NOT_APPLICABLE,{
   p("application",T::APPLICATION,R::REQUIRED,"Application name or resolvable executable."),
   p("target",T::FILE_PATH,R::OPTIONAL,"Optional file or folder to open.")}),
  {o("launched","boolean","Whether launch request succeeded."),o("application","application","Resolved application identity when available.")})},

 {"application.close", d(PrimitiveOperationType::APPLICATION_CLOSE,
  "Request that the specified application close.", "application_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::REQUIRED,R::OPTIONAL,R::NOT_APPLICABLE,{
   p("application",T::APPLICATION,R::REQUIRED,"Application name or process identity."),
   p("target",T::STRING,R::OPTIONAL,"Optional window or document qualifier.")}),
  {o("closed","boolean","Whether close request succeeded."),o("application","application","Application targeted.")})},

 {"application.focus", d(PrimitiveOperationType::APPLICATION_FOCUS,
  "Bring an existing application window to the foreground.", "application_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::REQUIRED,R::OPTIONAL,R::NOT_APPLICABLE,{
   p("application",T::APPLICATION,R::REQUIRED,"Application name or window identity."),
   p("target",T::STRING,R::OPTIONAL,"Optional window title qualifier.")}),
  {o("focused","boolean","Whether target window was focused."),o("application","application","Application targeted.")})},

 {"text.write", d(PrimitiveOperationType::TEXT_WRITE,
  "Write exact text to a supported text target.", "text_input",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::REQUIRED,R::REQUIRED,{
   p("target",T::STRING,R::REQUIRED,"File path or supported text target."),
   p("content",T::TEXT,R::REQUIRED,"Exact text to write.")}),
  {o("written","boolean","Whether write succeeded."),o("target","string","Target that received text.")})},

 {"keyboard.press", d(PrimitiveOperationType::KEYBOARD_PRESS,
  "Press a key with optional modifiers and repeat count.", "keyboard_input",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("key",T::KEY,R::REQUIRED,"Key name, e.g. ENTER, TAB, ESC, or A."),
   p("modifiers",T::MODIFIER_LIST,R::OPTIONAL,"Comma-separated modifier keys."),
   p("repeatCount",T::INTEGER,R::OPTIONAL,"Press count; defaults to 1.",{},true,1,true,100)}),
  {o("pressed","boolean","Whether key press was issued."),o("key","string","Key and modifiers issued.")})},

 {"keyboard.type", d(PrimitiveOperationType::KEYBOARD_TYPE,
  "Type literal text into the currently focused input target.", "keyboard_input",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::OPTIONAL,R::REQUIRED,{
   p("content",T::TEXT,R::REQUIRED,"Exact text to type."),
   p("target",T::STRING,R::OPTIONAL,"Optional expected target; input uses current focus.")}),
  {o("typed","boolean","Whether text input was issued."),o("character_count","integer","Number of requested characters.")})},

 {"mouse.click", d(PrimitiveOperationType::MOUSE_CLICK,
  "Click screen coordinates with optional button and click count.", "mouse_input",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("x",T::INTEGER,R::REQUIRED,"Horizontal screen coordinate.",{},true,0),
   p("y",T::INTEGER,R::REQUIRED,"Vertical screen coordinate.",{},true,0),
   p("button",T::ENUM,R::OPTIONAL,"Mouse button; defaults to left.",{"left","right","middle"}),
   p("clickCount",T::INTEGER,R::OPTIONAL,"Click count; defaults to 1.",{},true,1,true,3)}),
  {o("clicked","boolean","Whether click was issued."),o("x","integer","Horizontal coordinate."),o("y","integer","Vertical coordinate.")})},

 {"browser.navigate", d(PrimitiveOperationType::BROWSER_NAVIGATE,
  "Navigate the controlled browser to a validated HTTP(S) URL and wait for a DOM readiness condition.", "browser_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("url",T::URL,R::REQUIRED,"Absolute HTTP or HTTPS URL; credentials and non-web schemes are rejected."),
   p("ready_condition",T::ENUM,R::REQUIRED,"Readiness condition: domcontentloaded, load, networkidle, or selector.",{"domcontentloaded","load","networkidle","selector"}),
   p("selector",T::STRING,R::OPTIONAL,"CSS selector that must become visible when ready_condition is selector."),
   p("timeout_ms",T::INTEGER,R::OPTIONAL,"Readiness timeout in milliseconds; defaults to 30000.",{},true,1000,true,120000)}),
  {o("url","url","Final navigated URL."),o("ready","boolean","Whether the requested readiness condition was satisfied."),o("title","string","Document title after readiness.")})},

 {"browser.wait_for", d(PrimitiveOperationType::BROWSER_WAIT_FOR,
  "Wait for a condition on the current controlled page DOM without navigating.", "browser_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("ready_condition",T::ENUM,R::REQUIRED,"DOM readiness condition.",{"domcontentloaded","load","networkidle","selector"}),
   p("selector",T::STRING,R::OPTIONAL,"CSS selector that must become visible when ready_condition is selector."),
   p("timeout_ms",T::INTEGER,R::OPTIONAL,"Readiness timeout in milliseconds; defaults to 30000.",{},true,1000,true,120000)}),
  {o("ready","boolean","Whether the condition was satisfied."),o("url","url","Current page URL."),o("title","string","Current document title.")})},

 {"mouse.move", d(PrimitiveOperationType::MOUSE_MOVE,
  "Move the pointer to screen coordinates without clicking.", "mouse_input",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("x",T::INTEGER,R::REQUIRED,"Horizontal screen coordinate.",{},true,0),
   p("y",T::INTEGER,R::REQUIRED,"Vertical screen coordinate.",{},true,0)}),
  {o("moved","boolean","Whether pointer movement was issued."),o("x","integer","Requested horizontal coordinate."),o("y","integer","Requested vertical coordinate.")})}
};
}

bool PrimitiveRegistry::isSupported(const string& name) {
    return REGISTRY.contains(name);
}
bool PrimitiveRegistry::resolveType(const string& name, PrimitiveOperationType& type) {
    PrimitiveDefinition d;
    if (!resolveDefinition(name,d)) return false;
    type=d.type; return true;
}
bool PrimitiveRegistry::resolveDefinition(const string& name, PrimitiveDefinition& result) {
    auto it=REGISTRY.find(name);
    if (it==REGISTRY.end()) return false;
    result=it->second; return true;
}
vector<string> PrimitiveRegistry::getSupportedNames() {
    vector<string> names;
    for (const auto& [name, _] : REGISTRY) names.push_back(name);
    sort(names.begin(),names.end());
    return names;
}
vector<pair<string, PrimitiveDefinition>> PrimitiveRegistry::getCatalog() {
    vector<pair<string, PrimitiveDefinition>> result;
    for (const auto& name : getSupportedNames()) result.emplace_back(name,REGISTRY.at(name));
    return result;
}
