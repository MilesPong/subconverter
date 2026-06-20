#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "generator/config/subexport.h"
#include "parser/subparser.h"

namespace
{
void require(bool condition, const std::string &message)
{
    if(condition)
        return;

    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}
}

int main()
{
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
    require(ts["hostname"].as<std::string>() == "mihomo-test", "hostname should export");
    require(ts["auth-key"].as<std::string>() == "tskey-auth-fixture", "auth-key should export");
    require(ts["control-url"].as<std::string>() == "https://control.example.test", "control-url should export");
    require(ts["state-dir"].as<std::string>() == "./tailscale-test", "state-dir should export");
    require(!ts["ephemeral"].as<bool>(), "explicit ephemeral false should export");
    require(ts["udp"].as<bool>(), "udp true should export");
    require(!ts["accept-routes"].as<bool>(), "explicit accept-routes false should export");
    require(ts["exit-node"].as<std::string>() == "auto:any", "exit-node should export");
    require(!ts["exit-node-allow-lan-access"].as<bool>(), "explicit LAN-access false should export");
    require(ts["dialer-proxy"].as<std::string>() == "bootstrap", "Tailscale dialer should export");
    require(ts["interface-name"].as<std::string>() == "test0", "interface-name should export");
    require(ts["routing-mark"].as<unsigned int>() == 6666, "routing-mark should export");
    require(ts["ip-version"].as<std::string>() == "ipv4-prefer", "ip-version should export");
    require(proxies[1]["dialer-proxy"].as<std::string>() == "tailscale-test", "SOCKS5 dialer should export");

    const std::string missing_port = R"yaml(
proxies:
  - name: invalid-socks
    type: socks5
    server: 192.0.2.2
)yaml";
    std::vector<Proxy> invalid_nodes;
    require(explodeConfContent(missing_port, invalid_nodes) == 0, "non-Tailscale nodes should still require a port");
    require(invalid_nodes.empty(), "invalid non-Tailscale node should be skipped");

    std::vector<RulesetContent> rulesets;
    extra_settings surge_ext;
    const std::string surge_base = "[General]\n[Proxy]\n[Proxy Group]\n[Rule]\n";
    const std::string surge_output = proxyToSurge(nodes, surge_base, rulesets, groups, 4, surge_ext);
    require(surge_output.find("tskey-auth-fixture") == std::string::npos,
            "unsupported targets must not leak Tailscale authentication data");
    require(surge_output.find("tailscale-test") == std::string::npos,
            "unsupported targets must omit Tailscale nodes and dialer references");

    return 0;
}
