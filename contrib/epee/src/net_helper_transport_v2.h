std::vector<boost::asio::ip::tcp::endpoint> direct_connect_endpoints_v2(
    const boost::asio::ip::tcp::resolver::results_type &results,
    const std::string &addr,
    uint8_t allowed_families,
    const std::function<bool(const boost::asio::ip::tcp::endpoint&)> &endpoint_allowed,
    size_t max_resolved_endpoints)
{
  using boost::asio::ip::tcp;
  if (max_resolved_endpoints != 0
      && static_cast<size_t>(std::distance(results.begin(), results.end())) > max_resolved_endpoints)
    return {};
  if (endpoint_allowed && std::any_of(results.begin(), results.end(),
      [&](const auto &result) { return !endpoint_allowed(result.endpoint()); }))
    return {};
  if (allowed_families == 0)
  {
    boost::system::error_code address_error;
    const auto address = boost::asio::ip::make_address(addr, address_error);
    const bool ipv6_literal = !address_error && address.is_v6();
    const auto preferred = std::find_if(results.begin(), results.end(),
        [ipv6_literal](const auto &result) {
          return result.endpoint().address().is_v6() == ipv6_literal;
        });
    if (preferred != results.end())
      return {preferred->endpoint()};
    if (!ipv6_literal)
    {
      const auto fallback = std::find_if(results.begin(), results.end(),
          [](const auto &result) { return result.endpoint().address().is_v6(); });
      if (fallback != results.end())
        return {fallback->endpoint()};
    }
    return {};
  }
  std::vector<tcp::endpoint> endpoints;
  for (const auto &result : results)
  {
    const tcp::endpoint endpoint = result.endpoint();
    if ((endpoint.address().is_v4() && (allowed_families & 1) != 0)
        || (endpoint.address().is_v6() && (allowed_families & 2) != 0))
      endpoints.push_back(endpoint);
  }
  std::stable_partition(endpoints.begin(), endpoints.end(),
      [](const tcp::endpoint &endpoint) { return endpoint.address().is_v4(); });
  return endpoints;
}
