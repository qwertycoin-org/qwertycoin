#include "net/net_helper.h"
#include <algorithm>
#include <vector>
#include <boost/asio/connect.hpp>

namespace epee
{
namespace net_utils
{
	#include "net_helper_transport_v2.h"
	namespace
	{
		struct new_connection
		{
			boost::promise<boost::asio::ip::tcp::socket> result_;
			boost::asio::ip::tcp::socket socket_;
			boost::asio::ip::tcp::resolver resolver_;
			std::atomic<bool> settled_{false};

			template<typename T>
			explicit new_connection(T executor)
			  : result_(), socket_(executor), resolver_(executor)
			{}

			void fail(const boost::system::error_code &error)
			{
				if (!settled_.exchange(true))
					result_.set_exception(boost::system::system_error{error});
			}

			void succeed()
			{
				if (!settled_.exchange(true))
					result_.set_value(std::move(socket_));
			}
		};
	}

	boost::unique_future<boost::asio::ip::tcp::socket>
	direct_connect::operator()(const std::string& addr, const std::string& port, boost::asio::steady_timer& timeout) const
	{
		const auto shared = std::make_shared<new_connection>(MONERO_GET_EXECUTOR(timeout));
		timeout.async_wait([shared] (boost::system::error_code error)
		{
			if (!error && !shared->settled_.exchange(true))
			{
				shared->resolver_.cancel();
				boost::system::error_code ignored;
				shared->socket_.cancel(ignored);
				shared->socket_.close(ignored);
				shared->result_.set_exception(
					boost::system::system_error{boost::asio::error::timed_out});
			}
		});
		resolve_handler on_resolved = [shared, addr, families = allowed_families,
			allowed = endpoint_allowed, limit = max_resolved_endpoints](
			const boost::system::error_code &error,
			boost::asio::ip::tcp::resolver::results_type results) {
			if (shared->settled_.load())
				return;
			if (error)
				return shared->fail(error);
			auto endpoints = direct_connect_endpoints_v2(results, addr, families, allowed, limit);
			if (endpoints.empty())
				return shared->fail(boost::asio::error::host_not_found);
			boost::asio::async_connect(shared->socket_, endpoints,
				[shared](const boost::system::error_code &connect_error,
					const boost::asio::ip::tcp::endpoint &) {
					if (connect_error)
						shared->fail(connect_error);
					else
						shared->succeed();
				});
		};
		if (start_resolve)
			start_resolve(shared->resolver_, addr, port, std::move(on_resolved));
		else
			shared->resolver_.async_resolve(addr, port,
				boost::asio::ip::tcp::resolver::canonical_name, std::move(on_resolved));
		return shared->result_.get_future();
	}
}
}

