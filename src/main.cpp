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
 * \file main.cpp
 * \author Julien KAUFFMANN <julien.kauffmann@freelan.org>
 * \brief The main file.
 */

#include <iostream>
#include <cstdlib>
#include <csignal>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/function.hpp>

#include <cryptoplus/cryptoplus.hpp>
#include <cryptoplus/error/error_strings.hpp>

#include <freelan/freelan.hpp>
#include <freelan/logger_stream.hpp>

#ifdef WINDOWS
#include "win32/service.hpp"
#else
#include "posix/daemon.hpp"
#include "posix/locked_pid_file.hpp"
#endif

#include "version.hpp"
#include "tools.hpp"
#include "system.hpp"
#include "configuration_helper.hpp"

namespace fs = boost::filesystem;
namespace fl = freelan;

struct cli_configuration
{
	fl::configuration fl_configuration;
	bool debug;
#ifndef WINDOWS
	bool foreground;
	fs::path pid_file;
#endif
};

std::vector<fs::path> get_configuration_files()
{
	std::vector<fs::path> configuration_files;

#ifdef WINDOWS
	configuration_files.push_back(get_home_directory() / "freelan.cfg");
	configuration_files.push_back(get_application_directory() / "freelan.cfg");
#else
	configuration_files.push_back(get_home_directory() / ".freelan/freelan.cfg");
	configuration_files.push_back(get_application_directory() / "freelan.cfg");
#endif

	return configuration_files;
}

void do_log(freelan::log_level level, const std::string& msg)
{
	std::cout << boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::local_time()) << " [" << log_level_to_string(level) << "] " << msg << std::endl;
}

void signal_handler(const boost::system::error_code& error, int signal_number, fl::core& core, int& exit_signal)
{
	if (!error)
	{
		do_log(fl::LL_WARNING, "Signal caught (" + boost::lexical_cast<std::string>(signal_number) + "): exiting...");

		core.close();

		exit_signal = signal_number;
	}
}

bool parse_options(int argc, char** argv, cli_configuration& configuration)
{
	namespace po = boost::program_options;

	po::options_description visible_options;
	po::options_description all_options;

	po::options_description generic_options("Generic options");
	generic_options.add_options()
	("help,h", "Produce help message.")
	("version,v", "Get the program version.")
	("debug,d", "Enables debug output.")
	("configuration_file,c", po::value<std::string>(), "The configuration file to use.")
	;

	visible_options.add(generic_options);
	all_options.add(generic_options);

	po::options_description configuration_options("Configuration");
	configuration_options.add(get_server_options());
	configuration_options.add(get_fscp_options());
	configuration_options.add(get_security_options());
	configuration_options.add(get_tap_adapter_options());
	configuration_options.add(get_switch_options());

	visible_options.add(configuration_options);
	all_options.add(configuration_options);

#ifdef WINDOWS
	po::options_description service_options("Service");
	service_options.add_options()
	("install", "Install the service.")
	("uninstall", "Uninstall the service.")
	("reinstall", "Reinstall the service.")
	;

	visible_options.add(service_options);
	all_options.add(service_options);
#else
	po::options_description daemon_options("Daemon");
	daemon_options.add_options()
	("foreground,f", "Do not run as a daemon.")
	("pid_file,p", po::value<std::string>(), "A pid file to use.")
	;

	visible_options.add(daemon_options);
	all_options.add(daemon_options);
#endif

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, all_options), vm);

	if (vm.count("help"))
	{
		std::cout << visible_options << std::endl;

		return false;
	}

	if (vm.count("version"))
	{
		std::cout << FREELAN_NAME << " " << FREELAN_VERSION_STRING << " " << FREELAN_DATE << std::endl;

		return false;
	}

#ifdef WINDOWS
	if (vm.count("install"))
	{
		if (vm.count("uninstall"))
		{
			throw std::runtime_error("Cannot specify both --install and --uninstall options. Use --reinstall instead.");
		}
		else
		{
			if (win32::install_service())
			{
				std::cout << "Service installed." << std::endl;
			}
			else
			{
				std::cerr << "The service was already installed." << std::endl;
			}

			return false;
		}
	}
	else if (vm.count("uninstall"))
	{
		if (win32::uninstall_service())
		{
			std::cout << "Service uninstalled." << std::endl;
		}
		else
		{
			std::cerr << "The service has already been deleted." << std::endl;
		}

		return false;
	}
	else if (vm.count("reinstall"))
	{
		if (win32::uninstall_service())
		{
			std::cout << "Service uninstalled." << std::endl;
		}
		else
		{
			std::cerr << "The service has already been deleted." << std::endl;
		}

		if (win32::install_service())
		{
			std::cout << "Service installed." << std::endl;
		}
		else
		{
			std::cerr << "The service was already installed." << std::endl;
		}

		return false;
	}
#else
	configuration.foreground = (vm.count("foreground") > 0);

	if (vm.count("pid_file"))
	{
		configuration.pid_file = fs::absolute(vm["pid_file"].as<std::string>());
	}
	else
	{
		char* val = getenv("FREELAN_PID_FILE");

		if (val)
		{
			configuration.pid_file = fs::absolute(std::string(val));
		}
	}
#endif

	fs::path configuration_file;

	if (vm.count("configuration_file"))
	{
		configuration_file = fs::absolute(vm["configuration_file"].as<std::string>());
	}
	else
	{
		char* val = getenv("FREELAN_CONFIGURATION_FILE");

		if (val)
		{
			configuration_file = fs::absolute(std::string(val));
		}
	}

	if (!configuration_file.empty())
	{
		std::cout << "Reading configuration file at: " << configuration_file << std::endl;

		fs::basic_ifstream<char> ifs(configuration_file);

		if (!ifs)
		{
			throw po::reading_file(configuration_file.string().c_str());
		}

		po::store(po::parse_config_file(ifs, configuration_options, true), vm);
	}
	else
	{
		bool configuration_read = false;

		const std::vector<fs::path> configuration_files = get_configuration_files();

		BOOST_FOREACH(const fs::path& conf, configuration_files)
		{
			fs::basic_ifstream<char> ifs(conf);

			if (ifs)
			{
				std::cout << "Reading configuration file at: " << conf << std::endl;

				po::store(po::parse_config_file(ifs, configuration_options, true), vm);

				configuration_file = fs::absolute(conf);

				configuration_read = true;

				break;
			}
		}

		if (!configuration_read)
		{
			std::cerr << "Warning ! No configuration file specified and none found in the environment." << std::endl;
			std::cerr << "Looked up locations were:" << std::endl;

			BOOST_FOREACH(const fs::path& conf, configuration_files)
			{
				std::cerr << "- " << conf << std::endl;
			}
		}
	}

	po::notify(vm);

	const fs::path execution_root_directory = fs::current_path();

	setup_configuration(configuration.fl_configuration, execution_root_directory, vm);

	const fs::path tap_adapter_up_script = get_tap_adapter_up_script(execution_root_directory, vm);

	if (!tap_adapter_up_script.empty())
	{
		configuration.fl_configuration.tap_adapter.up_callback = boost::bind(&execute_tap_adapter_up_script, tap_adapter_up_script, _1, _2);
	}

	const fs::path tap_adapter_down_script = get_tap_adapter_down_script(execution_root_directory, vm);

	if (!tap_adapter_down_script.empty())
	{
		configuration.fl_configuration.tap_adapter.down_callback = boost::bind(&execute_tap_adapter_down_script, tap_adapter_down_script, _1, _2);
	}

	const fs::path certificate_validation_script = get_certificate_validation_script(execution_root_directory, vm);

	if (!certificate_validation_script.empty())
	{
		configuration.fl_configuration.security.certificate_validation_callback = boost::bind(&execute_certificate_validation_script, certificate_validation_script, _1, _2);
	}

	configuration.debug = vm.count("debug");

	return true;
}

void run(const cli_configuration& configuration, int& exit_signal)
{
#ifndef WINDOWS
	boost::shared_ptr<posix::locked_pid_file> pid_file;

	if (!configuration.pid_file.empty())
	{
		std::cout << "Creating PID file at: " << configuration.pid_file << std::endl;

		pid_file.reset(new posix::locked_pid_file(configuration.pid_file));
	}
#endif

	boost::function<void (freelan::log_level, const std::string&)> log_func = &do_log;

#ifndef WINDOWS
	if (!configuration.foreground)
	{
		posix::daemonize();

		log_func = &posix::syslog;
	}

	if (pid_file)
	{
		pid_file->write_pid();
	}
#endif

	boost::asio::io_service io_service;

	boost::asio::signal_set signals(io_service, SIGINT, SIGTERM);

	fl::logger logger(log_func, configuration.debug ? fl::LL_DEBUG : fl::LL_INFORMATION);

	fl::core core(io_service, configuration.fl_configuration, logger);

	core.open();

	signals.async_wait(boost::bind(signal_handler, _1, _2, boost::ref(core), boost::ref(exit_signal)));

	logger(fl::LL_INFORMATION) << "Execution started." << std::endl;

	if (!core.has_tap_adapter())
	{
		logger(fl::LL_INFORMATION) << "Configured not to use any tap adapter.";
	}

	logger(fl::LL_INFORMATION) << "Listening on: " << core.server().socket().local_endpoint();

	io_service.run();

	logger(fl::LL_INFORMATION) << "Execution stopped." << std::endl;
}

int main(int argc, char** argv)
{
#ifdef WINDOWS
	if (win32::run_service())
	{
		return EXIT_SUCCESS;
	}
#endif

	int exit_signal = 0;

	try
	{
		cryptoplus::crypto_initializer crypto_initializer;
		cryptoplus::algorithms_initializer algorithms_initializer;
		cryptoplus::error::error_strings_initializer error_strings_initializer;
		freelan::initializer freelan_initializer;

		cli_configuration configuration;

		if (parse_options(argc, argv, configuration))
		{
			run(configuration, exit_signal);
		}
	}
	catch (std::exception& ex)
	{
		std::cerr << "Error: " << ex.what() << std::endl;

		return EXIT_FAILURE;
	}

#ifndef WINDOWS
	if (exit_signal != 0)
	{
		do_log(fl::LL_ERROR, "Execution aborted because of a signal (" + boost::lexical_cast<std::string>(exit_signal) + ").");

		// We kill ourselves with the signal to ensure the process exits with the proprer state.
		//
		// This ensures that the calling process knows about this process being killed.
		kill(getpid(), exit_signal);
	}
#endif

	return EXIT_SUCCESS;
}
