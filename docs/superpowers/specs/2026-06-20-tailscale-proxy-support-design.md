# Tailscale Proxy Support Design

## Goal

Add first-class support for Mihomo's `tailscale` outbound type so Clash and ClashR input containing Tailscale proxies can be parsed, filtered, grouped, and emitted without losing supported fields. Other target formats continue to omit Tailscale because they do not provide an equivalent outbound type.

The field contract follows the Mihomo v1.19.25 Tailscale outbound implementation and the official Tailscale proxy documentation.

## Supported configuration

The Clash parser and exporter will preserve these fields:

- `name`
- `type: tailscale`
- `hostname`
- `auth-key`
- `control-url`
- `state-dir`
- `ephemeral`
- `udp`
- `accept-routes`
- `exit-node`
- `exit-node-allow-lan-access`
- `dialer-proxy`
- `interface-name`
- `routing-mark`
- `ip-version`

Unlike conventional proxy nodes, a Tailscale outbound has no `server` or `port`. The Clash parser must therefore exempt `type: tailscale` from the existing nonzero-port requirement while retaining that validation for all current types.

Optional booleans must preserve three states: absent, explicitly `false`, and explicitly `true`. They will use the project's existing `tribool` type. Optional strings are omitted during export when empty. `routing-mark` is omitted when zero, matching Mihomo's effective default behavior.

## Internal model

Add `ProxyType::Tailscale`, return `Tailscale` from `getProxyTypeName`, and define a `TAILSCALE_DEFAULT_GROUP` constant.

Add dedicated Tailscale properties to `Proxy` for hostname, authentication key, control URL, state directory, ephemeral mode, accepted routes, exit node, exit-node LAN access, interface name, and routing mark. Reuse the existing `UDP` and `IpVersion` properties where their semantics match Mihomo.

Add a distinct `DialerProxy` property for Mihomo's `dialer-proxy`. Do not overload `UnderlyingProxy`: that property models Surge's `underlying-proxy`, which is a separate output concept. Clash parsing and export will preserve `dialer-proxy` for Tailscale and conventional node types, enabling configurations such as a SOCKS5 node whose dialer is a Tailscale outbound.

## Parsing flow

`explodeClash` will read the node's `type` before applying the port requirement. Non-Tailscale nodes keep the current requirement. A Tailscale branch will construct a `Proxy` without calling the server/port-oriented `commonConstruct`, assign its default group and name, and parse all supported fields with presence-aware boolean handling.

The common Clash parsing path will read `dialer-proxy` into `Proxy::DialerProxy`. Existing parsing of `underlying-proxy` remains unchanged.

Malformed or unknown proxy types continue to be skipped according to current behavior. Authentication keys are treated as opaque strings and must not be logged.

## Clash and ClashR export

`proxyToClash` will add a Tailscale switch branch that emits `type: tailscale` plus each populated or explicitly configured field. It will not add `server` or `port` for this type.

After emitting a supported Clash node, the exporter will write `dialer-proxy` when `Proxy::DialerProxy` is nonempty. This applies to existing proxy types as well as Tailscale. Existing `underlying-proxy` behavior for non-Clash targets remains unchanged.

The matcher type map will include `TAILSCALE` so existing rename, filter, and group matching mechanisms can identify Tailscale nodes consistently.

All non-Clash exporters will retain their existing default branches and therefore omit Tailscale nodes. No lossy translation to another protocol will be attempted.

## Testing

Introduce a CTest regression executable linked against the project's static-library build. Tests will exercise production parsing and export functions rather than duplicate their logic.

Coverage will include:

1. A full official-style Tailscale YAML object parses without `server` or `port` and populates every supported property.
2. Clash export preserves all configured strings, integer values, and explicit booleans.
3. Explicit `false` values survive a Clash parse/export round trip rather than being omitted.
4. A SOCKS5 node preserves `dialer-proxy: tailscale`.
5. A mixed configuration retains both a Tailscale node and a conventional node.
6. Existing non-Tailscale port validation remains unchanged.
7. A representative unsupported exporter omits Tailscale instead of producing an invalid approximation.

The implementation follows red-green-refactor: add the round-trip regression first and verify it fails because Tailscale is absent, then add the minimum production changes and run the focused test, the full CTest suite, and a normal project build.

## Documentation and compatibility

Update the supported proxy-type documentation with Tailscale and state that it is a Mihomo/Clash-only output type requiring Mihomo v1.19.25 or newer.

The change does not alter existing configuration syntax or output for nodes without `dialer-proxy`. Existing `underlying-proxy` data and Surge export remain compatible.

## References

- Mihomo Tailscale proxy documentation: <https://wiki.metacubex.one/config/proxies/tailscale/>
- Mihomo v1.19.25 release: <https://github.com/MetaCubeX/mihomo/releases/tag/v1.19.25>
- Mihomo Tailscale implementation pull request: <https://github.com/MetaCubeX/mihomo/pull/2786>
