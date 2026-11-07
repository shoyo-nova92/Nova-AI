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
 {"connection.sources", d(PrimitiveOperationType::CONNECTION_SOURCES,
  "Read the user's configured personal source URLs, timezone and connection readiness. Inspect these before personal task/calendar questions; source titles are hints, never fixed task schemas. Missing native Notion credentials can be handled by browsing the configured URLs through an authenticated browser.","connections",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{}),{o("sources","object","Configured source references and connection readiness.")})},
 {"clock.now", d(PrimitiveOperationType::CLOCK_NOW,
  "Observe current date/time in the configured IANA timezone, UTC time and actual UTC offset (including DST). Use the user's local date for today; do not hardcode a date or assume UTC is local. Missing or invalid timezone configuration is an error, never a host-local guess.","clock",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{}),{o("local_date","date","Current host-local date with UTC offset and UTC timestamp.")})},
 {"notion.read", d(PrimitiveOperationType::NOTION_READ,
  "Read one observed Notion page/database/data source/block by UUID or URL. Inspect schema, status groups and data_sources before queries. Requires NOTION_API_KEY and page sharing; this is read-only. Preserve properties, completion facts and dates for semantic interpretation.","notion",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::REQUIRED,R::NOT_APPLICABLE,{
   p("target",T::STRING,R::REQUIRED,"Observed Notion UUID or user-provided Notion URL."),p("query",T::ENUM,R::OPTIONAL,"Resource kind hint; omit or use auto to discover from provider metadata. A page URL can identify a database.",{"auto","page","database","data_source","block"})}),{o("data","object","Exact provider metadata and schema.")})},
 {"notion.children", d(PrimitiveOperationType::NOTION_CHILDREN,
  "Read one page of Notion block children, preserving nested headings, checked to-dos, date mentions, table rows/cells and child_database links. Follow has_children and child pages semantically; follow has_more using next_cursor. Do not treat a truncated page as complete.","notion",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::REQUIRED,R::NOT_APPLICABLE,{
   p("target",T::STRING,R::REQUIRED,"Observed block/page UUID or URL."),p("offset",T::STRING,R::OPTIONAL,"Provider next_cursor from the previous result; omit for first page.")}),{o("data","object","Results, has_more and next_cursor exactly as returned by Notion.")})},
 {"notion.query", d(PrimitiveOperationType::NOTION_QUERY,
  "Read one page of records from an observed Notion data source. Interpret dates, recurrence, status groups and checkboxes semantically from its schema/content; never assume a field is called Date or Done. Deduplicate linked views by canonical data source/record ID. Use local dates for the requested day. Follow has_more/next_cursor.","notion",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::REQUIRED,R::OPTIONAL,{
   p("target",T::STRING,R::REQUIRED,"Observed data source UUID, not a saved view or parent database ID."),p("content",T::TEXT,R::OPTIONAL,"JSON object containing optional filter and sorts, using the observed property schema; omit to inspect records unfiltered."),p("offset",T::STRING,R::OPTIONAL,"Exact next_cursor returned by Notion.")}),{o("data","object","Records with original properties, dates, status, pagination and source identity.")})},
 {"voice.call", d(PrimitiveOperationType::VOICE_CALL,
  "Place ONE explicitly requested call to the user's own configured verified phone number and read factual content using a female Twilio voice. Requires NOVA_CALLS_ENABLED, Twilio credentials and phone setup. No arbitrary recipient parameter. Never call just because a user asks for a spoken/local summary. Stable request_id protects against duplicate dispatch; ambiguous writes require inspection, never a new ID to bypass the receipt. Accepted/queued does not prove answered/heard. Do not claim HeyGen quality on a telephone call.","telephony",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::REQUIRED,{
   p("content",T::TEXT,R::REQUIRED,"Accurate, concise readout grounded in observed sources."),p("request_id",T::STRING,R::REQUIRED,"Stable unique ID for this user call intent; preserve across retries."),p("confirmation",T::BOOLEAN,R::REQUIRED,"True only when current user request explicitly authorizes calling their configured own number.")}),{o("call_sid","string","Provider call identity and dispatch assurance, not delivery proof.")})},
 {"environment.observe", d(PrimitiveOperationType::ENVIRONMENT_OBSERVE,
  "Observe OS locale, known folders, foreground application and bounded visible application counts. Window titles and document text are omitted.", "environment",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{}),
  {o("environment","object","Local title-free environment inventory.")})},
 {"browser.tabs", d(PrimitiveOperationType::BROWSER_TABS,
  "List connected browser tabs. Optional target filters URLs/titles locally; unfiltered output exposes origins only. Shows attached versus isolated session honestly.", "browser_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::OPTIONAL,R::NOT_APPLICABLE,{
   p("target",T::STRING,R::OPTIONAL,"Optional relevant URL/domain/title substring to filter locally.")}),
  {o("tabs","array","Observed tab identifiers and relevant matching tabs.")})},
 {"browser.select_tab", d(PrimitiveOperationType::BROWSER_SELECT_TAB,
  "Select a tab using an identifier returned by browser.tabs; never invent IDs.", "browser_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::REQUIRED,R::NOT_APPLICABLE,{
   p("target",T::STRING,R::REQUIRED,"Exact observed tab identifier.")}),
  {o("url","url","Selected tab URL.")})},
 {"browser.download", d(PrimitiveOperationType::BROWSER_DOWNLOAD,
  "Click an observed control ONLY when user requested a download. Capture the transfer, save without overwriting and return verified path, byte count and completion. Never execute downloaded files automatically.", "browser_control",
  s(R::NOT_APPLICABLE,R::OPTIONAL,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("element_id",T::STRING,R::OPTIONAL,"ID from latest browser observation."),
   p("role",T::STRING,R::OPTIONAL,"Observed accessible role."),
   p("text",T::STRING,R::OPTIONAL,"Exact accessible label with role."),
   p("location",T::DIRECTORY_PATH,R::OPTIONAL,"Existing download directory; defaults to Windows Downloads."),
   p("timeout_ms",T::INTEGER,R::OPTIONAL,"Download event timeout milliseconds.",{},true,1000,true,120000)}),
  {o("path","file_path","Verified downloaded artifact path."),o("completed","boolean","Transfer completed successfully.")})},
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
   p("destination",T::FILE_PATH,R::REQUIRED,"Complete destination path including new filename. A relative destination is resolved against the explicit source parent; overwrite is not implied.")}),
  {o("path","file_path","Final path after moving."),o("moved","boolean","Whether move succeeded.")})},

 {"filesystem.copy", d(PrimitiveOperationType::FILESYSTEM_COPY,
  "Copy a file or directory from source to destination.", "filesystem",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("source",T::FILE_PATH,R::REQUIRED,"Existing file or directory to copy."),
   p("destination",T::FILE_PATH,R::REQUIRED,"Complete destination path including new filename, never a child of the source file. A relative destination uses the source parent."),
   p("overwrite",T::BOOLEAN,R::OPTIONAL,"Allow replacing destination; defaults false.",{},true,0,true,1)}),
  {o("path","file_path","Path of copied resource."),o("copied","boolean","Whether copy succeeded.")})},

 {"filesystem.rename", d(PrimitiveOperationType::FILESYSTEM_RENAME,
  "Rename a file or directory within its current parent directory.", "filesystem",
  s(R::REQUIRED,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::REQUIRED,R::NOT_APPLICABLE,{
   p("target",T::FILE_PATH,R::REQUIRED,"Existing resource to rename."),
   p("newName",T::STRING,R::REQUIRED,"New base name, not a parent path.")}),
  {o("path","file_path","Final path after renaming."),o("renamed","boolean","Whether rename succeeded.")})},

 {"filesystem.list_directory", d(PrimitiveOperationType::FILESYSTEM_LIST_DIRECTORY,
  "List a bounded set of entries in one authorized directory without recursing.", "filesystem",
  s(R::NOT_APPLICABLE,R::REQUIRED,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("location",T::DIRECTORY_PATH,R::REQUIRED,"Directory to list; accepts an absolute path or a supported known-folder alias."),
   p("max_results",T::INTEGER,R::OPTIONAL,"Maximum entries to return; defaults to 100.",{},true,1,true,200)}),
  {o("path","directory_path","Resolved directory path."),o("entries","array","Bounded list of absolute entry paths."),o("entry_count","integer","Number of entries returned.")})},

 {"filesystem.search", d(PrimitiveOperationType::FILESYSTEM_SEARCH,
  "Search an authorized root with deterministic normalized, substring, token-order-independent and fuzzy ranking. Returns canonical candidate identities, scores, reasons and completeness.", "filesystem",
  s(R::NOT_APPLICABLE,R::REQUIRED,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("location",T::DIRECTORY_PATH,R::REQUIRED,"Authorized root directory to search."),
   p("query",T::STRING,R::REQUIRED,"Meaningful filename words; punctuation and word order need not match. Use * only for an explicit type/time-only discovery request."),
   p("extension",T::STRING,R::OPTIONAL,"Requested extension including dot, compared case-insensitively."),
   p("modified_within_days",T::INTEGER,R::OPTIONAL,"Explicit recency bound in days; modification time is not download time.",{},true,0,true,36500),
   p("entry_type",T::ENUM,R::OPTIONAL,"Limit matches by resource type; defaults to any.",{"any","file","directory"}),
   p("max_results",T::INTEGER,R::OPTIONAL,"Maximum matches to return; defaults to 100.",{},true,1,true,1000)}),
  {o("path","directory_path","Resolved search root."),o("matches","array","Bounded list of matching absolute paths."),o("match_count","integer","Number of matches returned.")})},

 {"filesystem.inspect", d(PrimitiveOperationType::FILESYSTEM_INSPECT,
  "Inspect an authorized filesystem path: existence, resource type and file size. Small valid UTF-8 files also return a bounded text preview in execution evidence, with an explicit truncation flag; use it to read and compare short documents without opening an app.", "filesystem",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::REQUIRED,R::NOT_APPLICABLE,{
   p("target",T::FILE_PATH,R::REQUIRED,"Exact path to inspect.")}),
  {o("path","file_path","Resolved inspected path."),o("exists","boolean","Whether the path exists."),o("resource_type","string","file, directory, or other.")})},

 {"shell.observe", d(PrimitiveOperationType::SHELL_OBSERVE,
  "Read native Explorer shell windows and their current canonical filesystem locations without focusing or changing them. Prefer this observer for opened directories; Explorer PID/title alone does not identify a folder.", "application_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{}),
  {o("windows","array","Visible shell window handles and native canonical folder locations."),o("observer_available","boolean","Whether the native Shell observer is available.")})},

 {"application.observe", d(PrimitiveOperationType::APPLICATION_OBSERVE,
  "Read scoped process/window state without focusing or changing it. Supply observed process_id or exact executable application name, optionally window_handle and target path. Returns process instance identity, observation times, windows and direct document evidence. The runtime may derive correlated target proof from a successful exact-target launch of the same new process instance and a subsequent consistent visible document window. target_proof_level distinguishes exact, correlated and unavailable. Launch arguments, a PID or basename alone are never proof.", "application_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::OPTIONAL,R::OPTIONAL,R::NOT_APPLICABLE,{
   p("application",T::APPLICATION,R::OPTIONAL,"Exact observed executable name when process_id is unavailable."),
   p("process_id",T::INTEGER,R::OPTIONAL,"Actual observed or launch-returned process ID.",{},true,1,true,4294967295.0),
   p("window_handle",T::STRING,R::OPTIONAL,"Decimal window_handle from an earlier scoped application observation."),
   p("target",T::FILE_PATH,R::OPTIONAL,"Exact requested document path to check against observed document/tab metadata.")}),
  {o("processes","array","Observed matching process identities."),o("windows","array","Observed windows, visibility/title/foreground and supported target associations.")})},

 {"application.launch", d(PrimitiveOperationType::APPLICATION_LAUNCH,
  "Launch a registered application, or open an existing file/folder using its Windows association. Supply application, target, or both; execute a downloaded file only if explicitly requested.", "application_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::OPTIONAL,R::OPTIONAL,R::NOT_APPLICABLE,{
   p("application",T::APPLICATION,R::OPTIONAL,"Installed application name/AppID. Omit when opening target with its Windows association."),
   p("target",T::FILE_PATH,R::OPTIONAL,"Optional file or folder to open.")}),
  {o("launched","boolean","Whether launch request succeeded."),o("application","application","Resolved application identity when available.")})},

 {"application.close", d(PrimitiveOperationType::APPLICATION_CLOSE,
  "Request that the specified application close.", "application_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::REQUIRED,R::OPTIONAL,R::NOT_APPLICABLE,{
   p("application",T::APPLICATION,R::REQUIRED,"Exact executable name observed by environment.observe, rather than a guessed process alias."),
   p("target",T::STRING,R::OPTIONAL,"Optional window or document qualifier.")}),
  {o("closed","boolean","Whether close request succeeded."),o("application","application","Application targeted.")})},

 {"application.focus", d(PrimitiveOperationType::APPLICATION_FOCUS,
  "Bring an existing application window to the foreground.", "application_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::REQUIRED,R::OPTIONAL,R::NOT_APPLICABLE,{
   p("application",T::APPLICATION,R::REQUIRED,"Exact executable name observed by environment.observe, rather than a guessed process alias."),
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
   p("clickCount",T::INTEGER,R::OPTIONAL,"Click count; defaults to 1.",{},true,1,true,2)}),
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
 {"browser.observe", d(PrimitiveOperationType::BROWSER_OBSERVE,
  "Inspect the controlled page. Search query/role across the entire DOM, or page through elements with offset and next_offset. A page limit is not a limit on accessible elements.", "browser_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("query",T::STRING,R::OPTIONAL,"Case-insensitive words to find in element text, label or href, across the entire page."),
   p("role",T::STRING,R::OPTIONAL,"Accessibility role hint for ranking. Other relevant controls remain visible; interpret labels, form context and affordances semantically."),
   p("offset",T::INTEGER,R::OPTIONAL,"Start at this matching element offset.",{},true,0),
   p("max_results",T::INTEGER,R::OPTIONAL,"Elements per response. Follow next_offset for more.",{},true,1,true,1000)}),
  {o("url","url","Current browser URL."),o("elements","array","Visible interactive elements from the current page.")})},
 {"browser.scroll", d(PrimitiveOperationType::BROWSER_SCROLL,
  "Scroll the controlled browser page and return fresh DOM evidence. Positive y scrolls down; negative y up.", "browser_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("x",T::INTEGER,R::OPTIONAL,"Horizontal scroll distance."),
   p("y",T::INTEGER,R::REQUIRED,"Vertical scroll distance.")}),
  {o("observation","object","Page state after scrolling.")})},
 {"browser.press", d(PrimitiveOperationType::BROWSER_PRESS,
  "Send a key to an observed element in the controlled browser, regardless of which desktop window is focused. Use this for submitting browser forms instead of global keyboard.press.", "browser_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("key",T::KEY,R::REQUIRED,"Playwright key or chord, such as Enter, Tab or Control+A."),
   p("element_id",T::STRING,R::OPTIONAL,"ID of the observed browser input/control."),
   p("role",T::STRING,R::OPTIONAL,"Accessible role for an exact named target."),
   p("text",T::STRING,R::OPTIONAL,"Exact accessible name of the target."),
   p("timeout_ms",T::INTEGER,R::OPTIONAL,"Time to observe a resulting transition.",{},true,1000,true,120000)}),
  {o("action_outcome","string","Observed transition or no_observed_change."),o("post_action_observation","object","Fresh page and active tab state.")})},
 {"browser.hover", d(PrimitiveOperationType::BROWSER_HOVER,
  "Hover an observed record/control to reveal contextual actions, then observe fresh controls without clicking or editing.","browser_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("element_id",T::STRING,R::OPTIONAL,"ID from latest observation."),p("role",T::STRING,R::OPTIONAL,"Observed accessible role."),p("text",T::STRING,R::OPTIONAL,"Exact observed accessible name.")}),{o("post_action_observation","object","Fresh controls revealed by hovering.")})},
 {"browser.click", d(PrimitiveOperationType::BROWSER_CLICK,
  "Click a uniquely identified visible element in the controlled browser by observed ID or exact role and text.", "browser_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("element_id",T::STRING,R::OPTIONAL,"ID from the most recent browser.observe result."),
   p("role",T::STRING,R::OPTIONAL,"Accessible role, used with exact visible text."),
   p("text",T::STRING,R::OPTIONAL,"Exact accessible name or visible text, used with role.")}),
  {o("clicked","boolean","Whether the element was clicked."),o("url","url","Current browser URL after clicking.")})},
 {"browser.type", d(PrimitiveOperationType::BROWSER_TYPE,
  "Fill a uniquely identified input in the controlled browser by observed ID or exact role and text.", "browser_control",
  s(R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,R::NOT_APPLICABLE,{
   p("content",T::TEXT,R::REQUIRED,"Exact text to enter."),
   p("element_id",T::STRING,R::OPTIONAL,"ID from the most recent browser.observe result."),
   p("role",T::STRING,R::OPTIONAL,"Accessible role, used with exact visible text."),
   p("text",T::STRING,R::OPTIONAL,"Exact accessible name or visible text, used with role.")}),
  {o("typed","boolean","Whether text was entered."),o("url","url","Current browser URL.")})},

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
