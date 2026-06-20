# Tailscale Proxy Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Parse Mihomo Tailscale outbounds and preserve every v1.19.25 Tailscale field when exporting Clash/ClashR, while continuing to omit Tailscale from unsupported targets.

**Architecture:** Extend the existing `Proxy` tagged data model with a `Tailscale` variant and presence-aware Tailscale properties. Add one parser branch that bypasses the server/port invariant, one Clash exporter branch, and a distinct common `DialerProxy` field so Mihomo chaining is not conflated with Surge `UnderlyingProxy`.

**Tech Stack:** C++20, yaml-cpp, CMake/CTest, existing `tribool` and parser/exporter APIs.

---

## File structure

- Create `tests/tailscale_proxy_test.cpp`: production-level parse/export regression executable with a minimal assertion helper.
- Modify `CMakeLists.txt`: expose the regression executable only for `BUILD_STATIC_LIBRARY=ON` and register it with CTest.
- Modify `src/parser/config/proxy.h`: Tailscale enum, fields, default group, and Mihomo dialer field.
- Modify `src/parser/subparser.h`: declare the Tailscale constructor.
- Modify `src/parser/subparser.cpp`: construct and parse Tailscale; preserve `dialer-proxy`; exempt only Tailscale from port validation.
- Modify `src/generator/config/subexport.cpp`: matcher registration, Clash emission, and common `dialer-proxy` emission.
- Modify `README.md` and `README-cn.md`: document Clash-only Tailscale support and Mihomo version requirement.

### Task 1: Add the failing round-trip regression

**Files:**
- Create: `tests/tailscale_proxy_test.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Register a static-library test target**

In the `BUILD_STATIC_LIBRARY` branch of `CMakeLists.txt`, enable CTest and add:

```cmake
INCLUDE(CTest)

IF(BUILD_TESTING)
    ADD_EXECUTABLE(tailscale_proxy_test tests/tailscale_proxy_test.cpp)
    TARGET_COMPILE_DEFINITIONS(tailscale_proxy_test PRIVATE -DNO_JS_RUNTIME -DNO_WEBGET)
    TARGET_INCLUDE_DIRECTORIES(tailscale_proxy_test PRIVATE
        src
        ${YAML_CPP_INCLUDE_DIRS})
    TARGET_LINK_LIBRARIES(tailscale_proxy_test PRIVATE
        ${BUILD_TARGET_NAME}
        ${YAML_CPP_LIBRARY})
    ADD_TEST(NAME tailscale_proxy_roundtrip COMMAND tailscale_proxy_test)
ENDIF()
```

- [ ] **Step 2: Write the failing production-level test**

Create `tests/tailscale_proxy_test.cpp` with a `require(bool, message)` helper that prints the message and exits nonzero. Use this input with deliberately non-secret fixture data:

```cpp
const std::string input = R"yaml(
proxies:
  - name: tailscale-test
    type: tailscale
    hostname: mihomo-test
    auth-key: tskey-auth-fixture
    control-url: https://control.example.test
    state-dir: ./tailscale-test
    ephemeral: false
    udp: true
    accept-routes: false
    exit-node: auto:any
    exit-node-allow-lan-access: false
    dialer-proxy: bootstrap
    interface-name: test0
    routing-mark: 6666
    ip-version: ipv4-prefer
  - name: bootstrap
    type: socks5
    server: 192.0.2.1
    port: 1080
    dialer-proxy: tailscale-test
)yaml";
```

The test must:

```cpp
std::vector<Proxy> nodes;
require(explodeConfContent(input, nodes) == 1, "Clash input should parse");
require(nodes.size() == 2, "Tailscale and SOCKS5 nodes should both survive parsing");

const Proxy &tailscale = nodes[0];
require(tailscale.Type == ProxyType::Tailscale, "first node should be Tailscale");
require(tailscale.Remark == "tailscale-test", "name should parse");
require(tailscale.TailscaleHostname == "mihomo-test", "hostname should parse");
require(tailscale.TailscaleAuthKey == "tskey-auth-fixture", "auth-key should parse");
require(tailscale.TailscaleControlURL == "https://control.example.test", "control-url should parse");
require(tailscale.TailscaleStateDir == "./tailscale-test", "state-dir should parse");
require(!tailscale.TailscaleEphemeral.is_undef() && !tailscale.TailscaleEphemeral.get(), "explicit ephemeral false should parse");
require(!tailscale.UDP.is_undef() && tailscale.UDP.get(), "udp true should parse");
require(!tailscale.TailscaleAcceptRoutes.is_undef() && !tailscale.TailscaleAcceptRoutes.get(), "explicit accept-routes false should parse");
require(tailscale.TailscaleExitNode == "auto:any", "exit-node should parse");
require(!tailscale.TailscaleExitNodeAllowLANAccess.is_undef() && !tailscale.TailscaleExitNodeAllowLANAccess.get(), "explicit LAN-access false should parse");
require(tailscale.DialerProxy == "bootstrap", "Tailscale dialer-proxy should parse");
require(tailscale.TailscaleInterfaceName == "test0", "interface-name should parse");
require(tailscale.TailscaleRoutingMark == 6666, "routing-mark should parse");
require(tailscale.IpVersion == "ipv4-prefer", "ip-version should parse");
require(nodes[1].DialerProxy == "tailscale-test", "conventional node dialer-proxy should parse");
```

Then export using the production overload:

```cpp
YAML::Node output;
ProxyGroupConfigs groups;
extra_settings ext;
ext.clash_new_field_name = true;
ext.clash_proxies_style = "block";
proxyToClash(nodes, output, groups, false, ext);

const YAML::Node proxies = output["proxies"];
require(proxies.size() == 2, "Clash output should contain both nodes");
const YAML::Node ts = proxies[0];
require(ts["type"].as<std::string>() == "tailscale", "Tailscale type should export");
require(!ts["server"].IsDefined() && !ts["port"].IsDefined(), "Tailscale should not gain server or port");
require(ts["ephemeral"].as<bool>() == false, "explicit ephemeral false should export");
require(ts["accept-routes"].as<bool>() == false, "explicit accept-routes false should export");
require(ts["exit-node-allow-lan-access"].as<bool>() == false, "explicit LAN-access false should export");
require(ts["dialer-proxy"].as<std::string>() == "bootstrap", "Tailscale dialer should export");
require(proxies[1]["dialer-proxy"].as<std::string>() == "tailscale-test", "SOCKS5 dialer should export");
```

Also parse a YAML document containing only a normal node without `port` and assert the result stays empty, proving the exemption is limited to Tailscale.

- [ ] **Step 3: Configure and build the red test**

Run:

```bash
cmake -S . -B build-tailscale-test \
  -DBUILD_STATIC_LIBRARY=ON \
  -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tailscale-test --target tailscale_proxy_test -j2
```

Expected: compilation fails because `ProxyType::Tailscale` and the Tailscale model fields do not exist yet. This is the required RED state.

- [ ] **Step 4: Commit the failing regression**

```bash
git add CMakeLists.txt tests/tailscale_proxy_test.cpp
git commit -m "test: cover Tailscale proxy round trip"
```

### Task 2: Add the Tailscale internal model and parser

**Files:**
- Modify: `src/parser/config/proxy.h`
- Modify: `src/parser/subparser.h`
- Modify: `src/parser/subparser.cpp`

- [ ] **Step 1: Extend the proxy model**

Add `Tailscale` after `AnyTLS` in `ProxyType`, return `"Tailscale"` from `getProxyTypeName`, and add:

```cpp
String DialerProxy;

String TailscaleHostname;
String TailscaleAuthKey;
String TailscaleControlURL;
String TailscaleStateDir;
tribool TailscaleEphemeral;
tribool TailscaleAcceptRoutes;
String TailscaleExitNode;
tribool TailscaleExitNodeAllowLANAccess;
String TailscaleInterfaceName;
uint32_t TailscaleRoutingMark = 0;
```

Define:

```cpp
#define TAILSCALE_DEFAULT_GROUP "TailscaleProvider"
```

- [ ] **Step 2: Add a focused constructor**

Declare and define `tailscaleConstruct` with parameters matching the official fields. It must assign `Type`, `Group`, and `Remark` directly rather than call `commonConstruct`, then assign strings, `tribool` values, `UDP`, `DialerProxy`, `IpVersion`, and `TailscaleRoutingMark` using `to_int`.

The signature must use strings for YAML values and convert inside the constructor, matching current constructor patterns:

```cpp
void tailscaleConstruct(
    Proxy &node,
    const std::string &group,
    const std::string &remarks,
    const std::string &hostname,
    const std::string &auth_key,
    const std::string &control_url,
    const std::string &state_dir,
    const std::string &ephemeral,
    tribool udp,
    const std::string &accept_routes,
    const std::string &exit_node,
    const std::string &exit_node_allow_lan_access,
    const std::string &dialer_proxy,
    const std::string &interface_name,
    const std::string &routing_mark,
    const std::string &ip_version);
```

- [ ] **Step 3: Parse Tailscale and common Mihomo dialers**

In `explodeClash`, read `dialer-proxy` independently from `underlying-proxy` for every node. Change the validation from unconditional port enforcement to:

```cpp
if(proxytype != "tailscale" && (port.empty() || port == "0"))
    continue;
```

Add a `case "tailscale"_hash` that reads all official keys and calls `tailscaleConstruct`. Use `safe_as<std::string>` for optional booleans so absent fields stay undefined through `tribool(std::string)`.

- [ ] **Step 4: Build and confirm the failure advances**

Run:

```bash
cmake --build build-tailscale-test --target tailscale_proxy_test -j2
ctest --test-dir build-tailscale-test -R tailscale_proxy_roundtrip --output-on-failure
```

Expected: compilation succeeds and parsing assertions pass, but the test fails because Clash export does not yet emit Tailscale.

- [ ] **Step 5: Commit the model and parser**

```bash
git add src/parser/config/proxy.h src/parser/subparser.h src/parser/subparser.cpp
git commit -m "feat: parse Mihomo Tailscale proxies"
```

### Task 3: Export Tailscale and preserve dialer-proxy

**Files:**
- Modify: `src/generator/config/subexport.cpp`

- [ ] **Step 1: Register the matcher type**

Add:

```cpp
{ProxyType::Tailscale, "TAILSCALE"}
```

to the type map used by `applyMatcher`.

- [ ] **Step 2: Add the Clash exporter branch**

Add `case ProxyType::Tailscale` to `proxyToClash`. Set `type` to `tailscale`; emit nonempty strings and nonzero routing mark; emit each `tribool` only when it is not undefined. Do not emit `server` or `port`.

Because the exporter creates those common keys before the type switch, start the branch with:

```cpp
singleproxy.remove("server");
singleproxy.remove("port");
singleproxy["type"] = "tailscale";
```

The branch must contain these mappings:

```cpp
singleproxy["hostname"] = x.TailscaleHostname;
singleproxy["auth-key"] = x.TailscaleAuthKey;
singleproxy["control-url"] = x.TailscaleControlURL;
singleproxy["state-dir"] = x.TailscaleStateDir;
singleproxy["ephemeral"] = x.TailscaleEphemeral.get();
singleproxy["accept-routes"] = x.TailscaleAcceptRoutes.get();
singleproxy["exit-node"] = x.TailscaleExitNode;
singleproxy["exit-node-allow-lan-access"] = x.TailscaleExitNodeAllowLANAccess.get();
singleproxy["interface-name"] = x.TailscaleInterfaceName;
singleproxy["routing-mark"] = x.TailscaleRoutingMark;
singleproxy["ip-version"] = x.IpVersion;
```

Guard each assignment according to the presence rules above.

- [ ] **Step 3: Emit Mihomo dialer-proxy for all Clash nodes**

Replace the existing pre-switch `UnderlyingProxy` emission with post-switch precedence logic before style selection/push-back:

```cpp
if(!x.DialerProxy.empty())
    singleproxy["dialer-proxy"] = x.DialerProxy;
else if(!x.UnderlyingProxy.empty())
    singleproxy["dialer-proxy"] = x.UnderlyingProxy;
```

This preserves the current Clash behavior for nodes populated through `underlying-proxy`. Keep `UnderlyingProxy` handling unchanged in non-Clash exporters.

- [ ] **Step 4: Verify GREEN**

Run:

```bash
cmake --build build-tailscale-test --target tailscale_proxy_test -j2
ctest --test-dir build-tailscale-test -R tailscale_proxy_roundtrip --output-on-failure
```

Expected: `1/1` test passes.

- [ ] **Step 5: Add unsupported-target coverage**

Extend the test with a manually constructed Tailscale-only vector and call `proxyToSurge` using a minimal valid Surge base. Assert the result contains neither a `tailscale` proxy definition nor the fixture auth key. Re-run the focused test and confirm it remains green.

- [ ] **Step 6: Commit the exporter**

```bash
git add src/generator/config/subexport.cpp tests/tailscale_proxy_test.cpp
git commit -m "feat: export Tailscale proxies to Clash"
```

### Task 4: Document compatibility and run full verification

**Files:**
- Modify: `README.md`
- Modify: `README-cn.md`

- [ ] **Step 1: Document the supported type**

Add a concise entry to both READMEs stating:

```text
Tailscale outbound input/output is supported for Clash/ClashR targets and requires Mihomo v1.19.25 or newer. Unsupported target formats omit Tailscale nodes.
```

Link the Chinese documentation entry to <https://wiki.metacubex.one/config/proxies/tailscale/>.

- [ ] **Step 2: Run formatting and diff checks**

```bash
git diff --check
git status --short
```

Expected: no whitespace errors; only planned files are modified.

- [ ] **Step 3: Run the complete static test build**

```bash
cmake --build build-tailscale-test -j2
ctest --test-dir build-tailscale-test --output-on-failure
```

Expected: build exits zero and all registered tests pass.

- [ ] **Step 4: Run a normal executable build**

```bash
cmake -S . -B build-tailscale-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-tailscale-release -j2
```

Expected: `subconverter` builds successfully with no new compiler errors.

- [ ] **Step 5: Inspect the final behavior and commit documentation**

Run the focused test executable directly, inspect the final diff, and commit:

```bash
./build-tailscale-test/tailscale_proxy_test
git diff --stat HEAD~3
git diff --check HEAD~3
git add README.md README-cn.md
git commit -m "docs: document Tailscale proxy support"
```

Expected: the test exits zero, the diff contains only the planned model/parser/exporter/test/documentation changes, and the worktree is clean.
