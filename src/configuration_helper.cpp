/*
 * freelan - An open, multi-platform software to establish peer-to-peer virtual
 * private networks.
 *
 * Copyright (C) 2010-2011 Julien KAUFFMANN <julien.kauffmann@freelan.org>
 *
 * This file is part of freelan.
 *
 * freelan is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * freelan is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 *
 * If you intend to use freelan in a commercial software, please
 * contact me : we may arrange this for a small fee or no fee at all,
 * depending on the nature of your project.
 */

/**
 * \file configuration_helper.cpp
 * \author Julien KAUFFMANN <julien.kauffmann@freelan.org>
 * \brief A configuration helper.
 */

#include "configuration_helper.hpp"

#include <vector>

#include <boost/asio.hpp>
#include <boost/foreach.hpp>

#include "configuration_types.hpp"

namespace po = boost::program_options;
namespace fs = boost::filesystem;
namespace fl = freelan;

namespace
{
	cryptoplus::file load_file(const fs::path& filename)
	{
		if (filename.empty())
		{
			throw std::runtime_error("Cannot load file: filename is empty");
		}

		if (!is_regular_file(filename))
		{
			throw std::runtime_error("No such file: " + filename.string());
		}

		return cryptoplus::file::open(filename.native());
	}

	fl::security_configuration::cert_type load_certificate(const fs::path& filename)
	{
		return fl::security_configuration::cert_type::from_certificate(load_file(filename));
	}

	cryptoplus::pkey::pkey load_private_key(const fs::path& filename)
	{
		return cryptoplus::pkey::pkey::from_private_key(load_file(filename));
	}

	fl::security_configuration::cert_type load_trusted_certificate(const fs::path& filename)
	{
		return fl::security_configuration::cert_type::from_trusted_certificate(load_file(filename));
	}

	fl::security_configuration::crl_type load_crl(const fs::path& filename)
	{
		return fl::security_configuration::crl_type::from_certificate_revocation_list(load_file(filename));
	}
}

po::options_description get_fscp_options()
{
	po::options_description result("FreeLAN Secure Channel Protocol (FSCP) options");

	result.add_options()
	("fscp.hostname_resolution_protocol", po::value<fl::fscp_configuration::hostname_resolution_protocol_type>()->default_value(fl::fscp_configuration::HRP_IPV4), "The hostname resolution protocol to use.")
	("fscp.listen_on", po::value<fl::endpoint>()->default_value(fl::ipv4_endpoint(boost::asio::ip::address_v4::any(), 12000)), "The endpoint to listen on.")
	("fscp.hello_timeout", po::value<millisecond_duration>()->default_value(3000), "The default timeout for HELLO messages, in milliseconds.")
	("fscp.contact", po::value<std::vector<fl::endpoint> >()->multitoken()->zero_tokens()->default_value(std::vector<fl::endpoint>(), ""), "The address of an host to contact.")
	;

	return result;
}

po::options_description get_security_options()
{
	po::options_description result("Security options");

	result.add_options()
	("security.signature_certificate_file", po::value<fs::path>()->required(), "The certificate file to use for signing.")
	("security.signature_private_key_file", po::value<fs::path>()->required(), "The private key file to use for signing.")
	("security.encryption_certificate_file", po::value<fs::path>(), "The certificate file to use for encryption.")
	("security.encryption_private_key_file", po::value<fs::path>(), "The private key file to use for encryption.")
	("security.certificate_validation_method", po::value<fl::security_configuration::certificate_validation_method_type>()->default_value(fl::security_configuration::CVM_DEFAULT), "The certificate validation method.")
	("security.certificate_validation_script", po::value<fs::path>()->default_value(""), "The certificate validation script to use.")
	("security.authority_certificate_file", po::value<std::vector<std::string> >()->multitoken()->zero_tokens()->default_value(std::vector<std::string>(), ""), "An authority certificate file to use.")
	("security.certificate_revocation_validation_method", po::value<fl::security_configuration::certificate_revocation_validation_method_type>()->default_value(fl::security_configuration::CRVM_NONE), "The certificate revocation validation method.")
	("security.certificate_revocation_list_file", po::value<std::vector<std::string> >()->multitoken()->zero_tokens()->default_value(std::vector<std::string>(), ""), "A certificate revocation list file to use.")
	;

	return result;
}

po::options_description get_tap_adapter_options()
{
	po::options_description result("Tap adapter options");

	const fl::ipv4_network_address default_ipv4_network_address(boost::asio::ip::address_v4::from_string("9.0.0.1"), 24);
	const fl::ipv6_network_address default_ipv6_network_address(boost::asio::ip::address_v6::from_string("fe80::1"), 10);
	const fl::ipv4_network_address default_dhcp_ipv4_network_address(boost::asio::ip::address_v4::from_string("9.0.0.0"), 24);
	const fl::ipv6_network_address default_dhcp_ipv6_network_address(boost::asio::ip::address_v6::from_string("fe80::"), 10);

	result.add_options()
	("tap_adapter.enabled", po::value<bool>()->default_value(true, "yes"), "Whether to enable the tap adapter.")
	("tap_adapter.ipv4_address_prefix_length", po::value<fl::ipv4_network_address>()->default_value(default_ipv4_network_address), "The tap adapter IPv4 address and prefix length.")
	("tap_adapter.ipv6_address_prefix_length", po::value<fl::ipv6_network_address>()->default_value(default_ipv6_network_address), "The tap adapter IPv6 address and prefix length.")
	("tap_adapter.arp_proxy_enabled", po::value<bool>()->default_value(false), "Whether to enable the ARP proxy.")
	("tap_adapter.arp_proxy_fake_ethernet_address", po::value<fl::tap_adapter_configuration::ethernet_address_type>()->default_value(boost::lexical_cast<fl::tap_adapter_configuration::ethernet_address_type>("00:aa:bb:cc:dd:ee")), "The ARP proxy fake ethernet address.")
	("tap_adapter.dhcp_proxy_enabled", po::value<bool>()->default_value(true), "Whether to enable the DHCP proxy.")
	("tap_adapter.dhcp_server_ipv4_address_prefix_length", po::value<fl::ipv4_network_address>()->default_value(default_dhcp_ipv4_network_address), "The DHCP proxy server IPv4 address and prefix length.")
	("tap_adapter.dhcp_server_ipv6_address_prefix_length", po::value<fl::ipv6_network_address>()->default_value(default_dhcp_ipv6_network_address), "The DHCP proxy server IPv6 address and prefix length.")
	("tap_adapter.up_script", po::value<fs::path>()->default_value(""), "The tap adapter up script.")
	("tap_adapter.down_script", po::value<fs::path>()->default_value(""), "The tap adapter down script.")
	;

	return result;
}

po::options_description get_switch_options()
{
	po::options_description result("Switch options");

	result.add_options()
	("switch.routing_method", po::value<fl::switch_configuration::routing_method_type>()->default_value(fl::switch_configuration::RM_SWITCH), "The routing method for messages.")
	("switch.relay_mode_enabled", po::value<bool>()->default_value(false, "no"), "Whether to enable the relay mode.")
	;

	return result;
}

void setup_configuration(fl::configuration& configuration, const boost::filesystem::path& root, const po::variables_map& vm)
{
	typedef boost::asio::ip::udp::resolver::query query;
	typedef fl::security_configuration::cert_type cert_type;
	typedef cryptoplus::pkey::pkey pkey;

	// FSCP options
	configuration.fscp.hostname_resolution_protocol = vm["fscp.hostname_resolution_protocol"].as<fl::fscp_configuration::hostname_resolution_protocol_type>();
	configuration.fscp.listen_on = vm["fscp.listen_on"].as<fl::endpoint>();
	configuration.fscp.hello_timeout = vm["fscp.hello_timeout"].as<millisecond_duration>();

	configuration.fscp.contact_list = vm["fscp.contact"].as<std::vector<fl::endpoint> >();

	// Security options
	cert_type signature_certificate = load_certificate(fs::absolute(vm["security.signature_certificate_file"].as<fs::path>(), root));
	pkey signature_private_key = load_private_key(fs::absolute(vm["security.signature_private_key_file"].as<fs::path>(), root));

	cert_type encryption_certificate;
	pkey encryption_private_key;

	if (vm.count("security.encryption_certificate_file"))
	{
		encryption_certificate = load_certificate(fs::absolute(vm["security.encryption_certificate_file"].as<fs::path>(), root));
	}

	if (vm.count("security.encryption_private_key_file"))
	{
		encryption_private_key = load_private_key(fs::absolute(vm["security.encryption_private_key_file"].as<fs::path>(), root));
	}

	configuration.security.identity = fscp::identity_store(signature_certificate, signature_private_key, encryption_certificate, encryption_private_key);

	configuration.security.certificate_validation_method = vm["security.certificate_validation_method"].as<fl::security_configuration::certificate_validation_method_type>();

	const std::vector<std::string> authority_certificate_file_list = vm["security.authority_certificate_file"].as<std::vector<std::string> >();

	configuration.security.certificate_authority_list.clear();

	BOOST_FOREACH(const std::string& authority_certificate_file, authority_certificate_file_list)
	{
		configuration.security.certificate_authority_list.push_back(load_trusted_certificate(fs::absolute(authority_certificate_file, root)));
	}

	configuration.security.certificate_revocation_validation_method = vm["security.certificate_revocation_validation_method"].as<fl::security_configuration::certificate_revocation_validation_method_type>();

	const std::vector<std::string> crl_file_list = vm["security.certificate_revocation_list_file"].as<std::vector<std::string> >();

	configuration.security.certificate_revocation_list_list.clear();

	BOOST_FOREACH(const std::string& crl_file, crl_file_list)
	{
		configuration.security.certificate_revocation_list_list.push_back(load_crl(fs::absolute(crl_file, root)));
	}

	// Tap adapter options
	configuration.tap_adapter.enabled = vm["tap_adapter.enabled"].as<bool>();
	configuration.tap_adapter.ipv4_address_prefix_length = vm["tap_adapter.ipv4_address_prefix_length"].as<fl::ipv4_network_address>();
	configuration.tap_adapter.ipv6_address_prefix_length = vm["tap_adapter.ipv6_address_prefix_length"].as<fl::ipv6_network_address>();
	configuration.tap_adapter.arp_proxy_enabled = vm["tap_adapter.arp_proxy_enabled"].as<bool>();
	configuration.tap_adapter.arp_proxy_fake_ethernet_address = vm["tap_adapter.arp_proxy_fake_ethernet_address"].as<fl::tap_adapter_configuration::ethernet_address_type>();
	configuration.tap_adapter.dhcp_proxy_enabled = vm["tap_adapter.dhcp_proxy_enabled"].as<bool>();
	configuration.tap_adapter.dhcp_server_ipv4_address_prefix_length = vm["tap_adapter.dhcp_server_ipv4_address_prefix_length"].as<fl::ipv4_network_address>();
	configuration.tap_adapter.dhcp_server_ipv6_address_prefix_length = vm["tap_adapter.dhcp_server_ipv6_address_prefix_length"].as<fl::ipv6_network_address>();

	// Switch options
	configuration.switch_.routing_method = vm["switch.routing_method"].as<fl::switch_configuration::routing_method_type>();
	configuration.switch_.relay_mode_enabled = vm["switch.relay_mode_enabled"].as<bool>();
}

boost::filesystem::path get_tap_adapter_up_script(const boost::filesystem::path& root, const boost::program_options::variables_map& vm)
{
	fs::path tap_adapter_up_script_file = vm["tap_adapter.up_script"].as<fs::path>();

	return tap_adapter_up_script_file.empty() ? tap_adapter_up_script_file : fs::absolute(tap_adapter_up_script_file, root);
}

boost::filesystem::path get_tap_adapter_down_script(const boost::filesystem::path& root, const boost::program_options::variables_map& vm)
{
	fs::path tap_adapter_down_script_file = vm["tap_adapter.down_script"].as<fs::path>();

	return tap_adapter_down_script_file.empty() ? tap_adapter_down_script_file : fs::absolute(tap_adapter_down_script_file, root);
}

boost::filesystem::path get_certificate_validation_script(const boost::filesystem::path& root, const boost::program_options::variables_map& vm)
{
	fs::path certificate_validation_script_file = vm["security.certificate_validation_script"].as<fs::path>();

	return certificate_validation_script_file.empty() ? certificate_validation_script_file : fs::absolute(certificate_validation_script_file, root);
}
