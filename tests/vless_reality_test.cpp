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

YAML::Node exportToClash(std::vector<Proxy> &nodes)
{
    YAML::Node output;
    ProxyGroupConfigs groups;
    extra_settings ext;
    ext.clash_new_field_name = true;
    ext.clash_proxies_style = "block";
    proxyToClash(nodes, output, groups, false, ext);
    return output;
}
}

int main()
{
    Proxy raw_node;
    const std::string raw_uri =
        "vless://00000000-0000-4000-8000-000000000001@192.0.2.10:443"
        "?type=tcp&security=reality&sni=server.example.test"
        "&pbk=fixture-public-key&sid=0123456789abcdef&fp=chrome"
        "&hpkp=fixture-certificate-fingerprint"
        "#reality-fixture";
    explodeVLESS(raw_uri, raw_node);

    require(raw_node.Type == ProxyType::VLESS, "raw URI should parse as VLESS");
    require(raw_node.ClientFingerprint == "chrome", "fp should parse as client fingerprint");
    require(raw_node.Fingerprint == "fixture-certificate-fingerprint",
            "hpkp should remain the certificate fingerprint");

    std::vector<Proxy> raw_nodes{raw_node};
    const YAML::Node raw_output = exportToClash(raw_nodes);
    const YAML::Node raw_proxy = raw_output["proxies"][0];
    require(raw_proxy["client-fingerprint"].as<std::string>() == "chrome",
            "explicit fp should export as client-fingerprint");
    require(raw_proxy["reality-opts"]["support-x25519mlkem768"].as<bool>(),
            "raw REALITY VLESS should enable ML-KEM by default");

    Proxy hpkp_only_node;
    const std::string hpkp_only_uri =
        "vless://00000000-0000-4000-8000-000000000003@192.0.2.30:443"
        "?type=tcp&security=reality&sni=server.example.test"
        "&pbk=fixture-public-key&sid=0011223344556677"
        "&hpkp=fixture-certificate-fingerprint"
        "#reality-hpkp-only";
    explodeVLESS(hpkp_only_uri, hpkp_only_node);

    std::vector<Proxy> hpkp_only_nodes{hpkp_only_node};
    const YAML::Node hpkp_only_output = exportToClash(hpkp_only_nodes);
    const YAML::Node hpkp_only_proxy = hpkp_only_output["proxies"][0];
    require(hpkp_only_proxy["fingerprint"].as<std::string>() == "fixture-certificate-fingerprint",
            "hpkp should export as the top-level certificate fingerprint");
    require(hpkp_only_proxy["client-fingerprint"].as<std::string>() == "random",
            "missing fp should use random rather than the hpkp certificate fingerprint");

    Proxy tls_node;
    const std::string tls_uri =
        "vless://00000000-0000-4000-8000-000000000004@192.0.2.40:443"
        "?type=tcp&security=tls&sni=tls.example.test&fp=chrome"
        "#tls-fixture";
    explodeVLESS(tls_uri, tls_node);

    std::vector<Proxy> tls_nodes{tls_node};
    const YAML::Node tls_output = exportToClash(tls_nodes);
    const YAML::Node tls_proxy = tls_output["proxies"][0];
    require(!tls_proxy["reality-opts"].IsDefined(),
            "non-REALITY TLS VLESS should not export reality options");

    const std::string clash_input = R"yaml(
proxies:
  - name: reality-explicit-false
    type: vless
    server: 192.0.2.20
    port: 443
    uuid: 00000000-0000-4000-8000-000000000002
    network: tcp
    tls: true
    servername: server.example.test
    client-fingerprint: chrome
    reality-opts:
      public-key: fixture-public-key
      short-id: fedcba9876543210
      support-x25519mlkem768: false
)yaml";

    std::vector<Proxy> clash_nodes;
    require(explodeConfContent(clash_input, clash_nodes) == 1, "Clash REALITY input should parse");
    require(clash_nodes.size() == 1, "Clash REALITY input should contain one node");

    const YAML::Node clash_output = exportToClash(clash_nodes);
    const YAML::Node clash_proxy = clash_output["proxies"][0];
    require(clash_proxy["reality-opts"]["support-x25519mlkem768"].IsDefined(),
            "explicit ML-KEM false should remain present");
    require(!clash_proxy["reality-opts"]["support-x25519mlkem768"].as<bool>(),
            "explicit ML-KEM false should round-trip unchanged");

    return 0;
}
