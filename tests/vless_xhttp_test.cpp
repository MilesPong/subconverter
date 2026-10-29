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
        "vless://613d8a3a-691c-4dec-8810-d586388364e6@cc-cf.immiles.com:443"
        "?encryption=none&security=tls&sni=cc-cf.immiles.com&fp=chrome"
        "&type=xhttp&host=cc-cf.immiles.com&path=%2Fxhttp-stream&mode=auto"
        "&x_padding_bytes=100-1000"
        "&extra=%7B%22mode%22%3A%22auto%22%2C%22xPaddingBytes%22%3A%22100-1000%22%7D"
        "&packetEncoding=xudp#cc-xhttp";
    explodeVLESS(raw_uri, raw_node);

    require(raw_node.Type == ProxyType::VLESS, "raw URI should parse as VLESS");
    require(raw_node.TransferProtocol == "xhttp", "raw URI should parse network as xhttp");
    require(raw_node.Host == "cc-cf.immiles.com", "raw URI host should parse correctly");
    require(raw_node.Path == "/xhttp-stream", "raw URI path should be decoded to /xhttp-stream");
    require(raw_node.XhttpMode == "auto", "raw URI mode should parse as auto");
    require(raw_node.XhttpPaddingBytes == "100-1000", "x_padding_bytes should parse as 100-1000");
    require(raw_node.PacketEncoding == "xudp", "packetEncoding should parse as xudp");
    require(raw_node.ClientFingerprint == "chrome", "fp should parse as client fingerprint");
    require(raw_node.SNI == "cc-cf.immiles.com", "sni should parse correctly");
    require(raw_node.TLSSecure, "security=tls should set TLSSecure");

    std::vector<Proxy> raw_nodes{raw_node};
    const YAML::Node raw_output = exportToClash(raw_nodes);
    const YAML::Node raw_proxy = raw_output["proxies"][0];

    require(raw_proxy["type"].as<std::string>() == "vless", "type should export as vless");
    require(raw_proxy["network"].as<std::string>() == "xhttp", "network should export as xhttp");
    require(raw_proxy["tls"].as<bool>(), "tls should export as true");
    require(raw_proxy["servername"].as<std::string>() == "cc-cf.immiles.com", "servername should match");
    require(raw_proxy["client-fingerprint"].as<std::string>() == "chrome", "client-fingerprint should be chrome");
    require(raw_proxy["packet-encoding"].as<std::string>() == "xudp", "packet-encoding should export as xudp");
    require(raw_proxy["xhttp-opts"]["path"].as<std::string>() == "/xhttp-stream", "xhttp-opts path should match");
    require(raw_proxy["xhttp-opts"]["host"].as<std::string>() == "cc-cf.immiles.com", "xhttp-opts host should match");
    require(raw_proxy["xhttp-opts"]["mode"].as<std::string>() == "auto", "xhttp-opts mode should match");
    require(raw_proxy["xhttp-opts"]["x-padding-bytes"].as<std::string>() == "100-1000", "xhttp-opts x-padding-bytes should match");

    const std::string clash_input = R"yaml(
proxies:
  - name: vless-xhttp
    type: vless
    server: cc-cf.immiles.com
    port: 443
    uuid: 613d8a3a-691c-4dec-8810-d586388364e6
    udp: true
    packet-encoding: xudp
    network: xhttp
    xhttp-opts:
      path: /xhttp-stream
      host: cc-cf.immiles.com
      mode: auto
      x-padding-bytes: "100-1000"
    tls: true
    servername: cc-cf.immiles.com
    client-fingerprint: chrome
)yaml";

    std::vector<Proxy> clash_nodes;
    require(explodeConfContent(clash_input, clash_nodes) == 1, "Clash XHTTP input should parse");
    require(clash_nodes.size() == 1, "Clash XHTTP input should contain one node");

    const YAML::Node clash_output = exportToClash(clash_nodes);
    const YAML::Node clash_proxy = clash_output["proxies"][0];
    require(clash_proxy["network"].as<std::string>() == "xhttp", "Clash round-trip should keep network xhttp");
    require(clash_proxy["packet-encoding"].as<std::string>() == "xudp", "Clash round-trip should keep packet-encoding xudp");
    require(clash_proxy["xhttp-opts"]["path"].as<std::string>() == "/xhttp-stream", "Clash round-trip should keep path");
    require(clash_proxy["xhttp-opts"]["host"].as<std::string>() == "cc-cf.immiles.com", "Clash round-trip should keep host");
    require(clash_proxy["xhttp-opts"]["mode"].as<std::string>() == "auto", "Clash round-trip should keep mode");
    require(clash_proxy["xhttp-opts"]["x-padding-bytes"].as<std::string>() == "100-1000", "Clash round-trip should keep x-padding-bytes");

    std::cout << "All VLESS XHTTP tests passed successfully!\n";
    return 0;
}
